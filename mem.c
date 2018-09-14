#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "serial.h"
#include "mem.h"
#include "x86.h"
#include "pcb.h"
uint8_t   gdtPtr[6];           // 48-bit structure to hold address of GDT.
uint32_t  theGdt[16*2] = {0};  // The actual GDT


void loadGdt()
{
    uint16_t *t1 = (uint16_t*)&gdtPtr[0];   // Avoid aliasing warning
    *t1          = 16*8-1;                  // Minimum size of GDT (leaves blank entries)
    uint32_t *t2 = (uint32_t*)&gdtPtr[2];
    *t2          = (uint32_t)theGdt;

    __asm__ __volatile__("                                    \n\
    cli                  # Make sure interrupts are disabled  \n\
    movl %[tab], %%eax   # Get the gdtPtr address             \n\
    lgdt (%%eax)         # Set the GDT                        \n\
    ljmp $0x08, $1f      # Change cs, code segment = 0x08     \n\
    1:                   # Local label as long-jump target    \n\
    movl $16, %%eax      # Set all data segments to 0x10      \n\
    movw %%ax, %%ds                                           \n\
    movw %%ax, %%es                                           \n\
    movw %%ax, %%fs                                           \n\
    movw %%ax, %%gs                                           \n\
    movw %%ax, %%ss                                           \n\
    " : : [tab] "p" (&gdtPtr) : "%eax");
}

/**
 A Segment Descriptor is a 64-bit structure that sits inside a region of memory
 called the GDT (global descriptor table). These are described in §3.4.5 of the
 Intel Systems Programming Manual (Vol 3a). Take note of the bit-ordering, I've
 mirrored Intel's diagrams so that they match the little-endian memory layout.
 *
    0             7   8             15 16             23 24             31
   +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
   |       Segment Limit 0-15        | |    Base Address 0-15            |
   +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
   32             39 40             47 48             55 56             63
   +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
   | Base 16-23    | | Type  |S|DPL|P| |Li16-19|0|0|1|G| | Base 24-31    |
   +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
 *
   Segment Limit: 20-bit value
   Base:          32-bit physical address
   S:             1=code/data 0=system (e.g. a TSS)
   DPL:           descriptor privlege level (i.e. ring number for use).
   G:             granularity, 0=bytes 1=4kb-pages
   P:             present
**/

void encodeMemDescriptor(
      uint32_t index,       // Which 8-byte entry in the GDT
      uint32_t base,        // The start address
      uint32_t limit,       // Highest valid offset (in bytes or pages)
      uint8_t  type,        // Access flags
      uint8_t  dpl,         // Maximum ring that can access
      bool     kind         // False=system/bytes,  True=non-system/pages
      )
{
// 32-bit, present, and system flags set. Granularity is one for non-system segments
// (i.e. measured in pages) and zero for the system segments (i.e. bytes).
uint32_t flags = kind ? 0x00c09000UL : 0x00008000UL;
    theGdt[index*2]   = ((base & 0xffffUL) << 16) | (limit&0xffffUL);
    theGdt[index*2+1] = flags   |
                        (base & 0xff000000UL) | ((base & 0xff0000UL) >> 16) |
                        (limit&0xf0000UL)     | ((type&0xfUL) << 8)         |
                        ((dpl&0x3UL) <<13);
}

// Load the task register: sets the TSS segment offset within the GDT.
inline void loadTr(uint32_t index)
{
  __asm__ __volatile__("ltr %%ax" : : "a" (index*8) );
}


uint8_t *allocateFrame()
{
    return (uint8_t*)freeList[ --numFree ];
}

void freeFrame(uint8_t *page)
{
    freeList[ numFree++] = (uint32_t)page;
}

extern uint8_t stack_top[], stack_bottom[];   // asm glue
void initMemory(struct grubMMap *regions,
                uint32_t excludeLo,   uint32_t excludeHi)
{

	    // Setup the GDT, entry 0=null
	    encodeMemDescriptor(1, 0,                   0xfffff, CodeType, 0, true);
	    encodeMemDescriptor(2, 0,                   0xfffff, DataType, 0, true);
	    encodeMemDescriptor(3, (uint32_t)systemTSS, 0x67,    TssType,  0, false);
	    encodeMemDescriptor(4, 0,                   0xfffff, CodeType, 3, true);
	    encodeMemDescriptor(5, 0,                   0xfffff, DataType, 3, true);
	    loadGdt();
	    loadTr(3);
	    systemTSS[18] = 16;   // ES
	    systemTSS[19] = 8;    // CS
	    systemTSS[20] = 16;   // SS
	    systemTSS[21] = 16;   // DS
	    systemTSS[22] = 16;   // FS
	    systemTSS[23] = 16;   // GS
	    systemTSS[14] = stack_top;
	    systemTSS[1]  = stack_top;    // Stack for syscalls
	    systemTSS[2]  = 16;           // SS for interrupts

	    // First region is probably the low 640kb
	    // Use this as an unsorted (void*)[] of free pages (max 160MB of memory).
	    freeList = (uint32_t*)regions[0].base;
	    uint32_t firstPage = (excludeHi + 0xfff) & 0xfffff000;
	    uint32_t space = regions[1].base + regions[1].length - firstPage;
	    numFree  = min( regions[0].length / 4,
	                    space / 4096);

	    for(uint32_t i=0; i<numFree; i++)
	        freeList[i] = firstPage + i*4096;
	    serialPrintf("Mem: Placed %u pages (%u-kb) in freeList (%u-kb) \n",
	                 numFree,numFree*4,numFree/256);
	    serialPrintf("Mem: Kernel stack %08x - %08x\n", stack_bottom, stack_top);

      uint32_t pageDir = (uint32_t)allocateFrame();
    	setCR3( pageDir );
    	memset((uint8_t*)pageDir,0,4096);
    	forceFrameAsPage(pageDir, pageDir);
            forceFrameAsPage(pageDir, 0xb8000);
    	serialPrintf("Mem: kernel cr3=%08x\n", getCR3());
    	forceFramesAsPages((uint32_t*)pageDir, freeList, (uint32_t)(freeList+numFree) );
    	forceFramesAsPages((uint32_t*)pageDir, excludeLo, excludeHi);
    	activatePaging();
}
/*
	    Write an entry into the kernel page-directory / page-tables to allow the frame
	    to be accessed as a page, i.e. wire-up 1:1 the address between the virtual and
	    physical address spaces. If the frame is in a region of memory without a page-
	    table (1024*4kb=4MB) then a table is allocated (and wired in for access). The
	    worst case behaviour is if allocateFrame returns a sparse distribution of new
	    frames so that each table requires a new table to access it - this will consume
	    4100kb of memory but it will still terminate when the directory is complete.
	    This does not happen with the dense freeList when the kernel boots.
	*/
	void forceFrameAsPage(uint32_t *pageDir, uint32_t frame)
	{
	uint32_t *pageTable;
	uint32_t index = frame>>22;
	    serialPrintf("Mem: forceFrameAsPage %08x into directory %08x [%u] table %08x\n", frame, pageDir, index, pageDir[index]);
	    if( pageDir[index]==0 )
	    {
	        pageTable = (uint32_t*)allocateFrame();
	        serialPrintf("Mem: new page-table %08x\n",pageTable);
	        pageDir[index] = (uint32_t)pageTable | 0x03UL;  // Supervisor|RW|Present
	        if(pageDir!=getCR3())   // When wiring user-mode dirs make tables kernel-accessible
	            forceFrameAsPage(getCR3(), pageTable);
	        forceFrameAsPage(pageDir, (uint32_t)pageTable);
	        memset((uint8_t*)pageTable,0,4096);
	        // This looks like a nasty hack, but it is not! Because we are wiring up
	        // frames as pages with the same addresses it is possible for a page-table
	        // to contain its own address as an entry inside itself... If this happens
	        // then the memset just wiped out that entry so we must put it back in to
	        // continue accessing the page-table in paged mode.
	        if( ((uint32_t)pageTable>>22) == index )
	            pageTable[ ((uint32_t)pageTable>>12) & 0x3ffUL ] = (uint32_t)pageTable | 0x03UL;
	    }
	    else
	        pageTable = (uint32_t*)(pageDir[index] & 0xfffff000UL);
	    index = (frame>>12) & 0x3ff;
	    pageTable[index] = frame | 0x03UL; // Supervisor|RW|Present
	}

	void forceFramesAsPages(uint32_t *pageDir, uint32_t loAddress, uint32_t hiAddress)
	{
	uint32_t loFrame = loAddress & 0xfffff000UL;
	uint32_t hiFrame = (hiAddress + 4095UL) & 0xfffff000UL;
	    for(uint32_t frame = loFrame; frame<=hiFrame; frame+=4096)
	        forceFrameAsPage(pageDir, frame);
	}

	void memset(uint8_t *target, uint8_t val, uint32_t size)
	{
	    for(uint32_t i=0; i<size; i++)
	        *(target++) = val;
	}

    void memcpy(uint8_t *target, uint8_t *target2, uint32_t size)
    {
        for(uint32_t i=0; i<size; i++)
            *(target++)= *(target2++);
    }

    /*
	   Ensure that a page is in the page-table structure at the target virtual
	   address, allocating and inserting a table and then the page (only) if
	   necessary. Return the linear address of the page. Also force the page
	   into the kernel page-structures so that we can access it.
	*/
	uint32_t forceVirtualPage(Pcb *pcb, uint32_t vpage)
	{
	uint32_t tableNum = (vpage >> 22) & 0x3ffUL;
	    if( pcb->pageDirectory[tableNum]==0 )
	    {
	        uint32_t frame = allocateFrame();
	        pcb->pageDirectory[tableNum] = frame | 0x07UL; // User | RW | Present
	        forceFrameAsPage(getCR3(), frame);
	        memset(pcb->pageDirectory[tableNum],0,4096);
	    }
	uint32_t *table = (uint32_t*)(pcb->pageDirectory[tableNum]&0xfffff000UL);
	uint32_t offset = (vpage >> 12) & 0x3ffUL;
	    if( table[offset]==0 )
	    {
	        uint32_t frame = (uint32_t)allocateFrame();
	        table[offset] = frame | 0x07UL; // User | RW | Present
	        forceFrameAsPage(getCR3(), frame);
	    }
	    return table[offset] & 0xfffff000UL;
	}

	/*
	   The interval in virtual memory might not be aligned with pages (i.e. if we are
	   copying section from an ELF container). We need the forced inserts into the
	   page-table to happen on page-aligned addresses. For the data copy we do not know
	   that the target interval is contiguous (i.e. we are using the simplest single-page
	   allocator).
	*/

	void copyIntoVirtual(uint8_t *source, uint32_t size, uint32_t virtualAddress, Pcb *pcb)
	{
	    serialPrintf("cpVM: %08x -> %08x (%u-bytes)\n",
	                 source, virtualAddress, size);
	    // If the low-end of the interval is not aligned with a page then handle
	    // the alignment first by doing a chunk less than a page.
	    if((virtualAddress&0xfffUL) != 0)
	    {
	        uint32_t pageBelow = virtualAddress & 0xfffff000UL;
	        uint32_t offset    = virtualAddress & 0x00000fffUL;
	        uint32_t pageAbove = pageBelow + 4096UL;

	        uint8_t *frame = forceVirtualPage(pcb,pageBelow);
	        if( virtualAddress+size < pageAbove )
	        {
	            serialPrintf("cpVM: offset=%08x below=%08x above=%08x frame=%08x\n",
	                         offset, pageBelow, pageAbove, frame);
	            memcpy(frame+offset, source, size);
	            return;
	        }
	        memcpy(frame+offset, source, pageAbove - virtualAddress);
	        size -= pageAbove-virtualAddress;
	        virtualAddress = pageAbove;
	    }

	    // Life is easy in the middle: just do entire pages
	    while(size>=4096)
	    {
	        uint8_t *frame = forceVirtualPage(pcb,virtualAddress);
	        memcpy(frame, virtualAddress, 4096);
	        virtualAddress += 4096;
	        size -= 4096;
	    }

	    // If the high-end of the interval is not aligned with a page then do a chunk
	    // less than a page.
	    if(size>0)
	    {
	        uint8_t *frame = forceVirtualPage(pcb,virtualAddress);
	        memcpy(frame, virtualAddress, size);
	    }
	}
