#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mem.h"


uint8_t   gdtPtr[6];           // 48-bit structure to hold address of GDT.
uint32_t  theGdt[16*2] = {0};  // The actual GDT
uint32_t  systemTSS[26] __attribute__((aligned(4096))) = {0};

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
}
