#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "serial.h"
#include "ints.h"
#include "mem.h"
#include "x86.h"
#include "pcb.h"
#include "initrd.h"

#if defined(__linux__)
#error "You must use the cross-compiler, not the linux compiler!"
#endif
#if !defined(__i386__)
#error "Wrong architecture, use the i686-elf-gcc compiler."
#endif

void __attribute__((fastcall)) pfHandler(uint32_t gateNum, uint32_t errCode)
{
  intCounters[ gateNum&255 ] ++;
uint32_t addr = (uint32_t)getCR2();
  serialPrintf("Page fault! errcode=%08x addr=%08x\n", errCode, addr);
  // Don't do anything else, this will freeze the kernel so that we don't get
  // lots of spurious errors after this one.
  __asm__ __volatile__("\
    jmp waiting");
}
void decodeElf(Pcb *pcb, uint32_t base, uint32_t length)
	{
	uint8_t *sheader = *((uint32_t*)(base+32)) + base;
	uint16_t sheaderSize = *((uint16_t*)(base+46));
	uint16_t sheaderNum = *((uint16_t*)(base+48));
	    serialPrintf("Section Header offset: %08x\n", *((uint32_t*)(base+32)) );
	    serialPrintf("SHeaders: %u x %u-bytes\n", sheaderNum, sheaderSize);
	uint16_t strTabIndex = *((uint16_t*)(base+50));
	uint8_t *strTab = (uint8_t*)(base+*((uint32_t*)(sheader + strTabIndex*sheaderSize + 16)));
	    serialPrintf("STab index=%u, section-offset=%08x @%08x\n",
	                 strTabIndex, strTab-base, strTab);
	    for(uint16_t i=0; i<sheaderNum; i++)
	    {
	        uint32_t targetVirt = *((uint32_t*)(sheader+i*sheaderSize+12));
	        uint32_t fileOffset = *((uint32_t*)(sheader+i*sheaderSize+16));
	        uint32_t size       = *((uint32_t*)(sheader+i*sheaderSize+20));
	        if(targetVirt==0) continue;
	        serialPrintf("Section %u: Name %s @%08x length=%u file-offset=%u\n",
	                     i,
	                     strTab + *((uint32_t*)(sheader+i*sheaderSize)),
	                     targetVirt, size, fileOffset
	                     );
	        copyIntoVirtual(base+fileOffset,size,targetVirt,pcb);
	    }
	}

Pcb *createProcess(uint32_t kernelLo, uint32_t kernelHi, uint8_t *elf, uint32_t size)
	{
	Pcb *pcb = (Pcb*)allocateFrame();
	    forceFrameAsPage(getCR3(),pcb);
	    pcb->pageDirectory = allocateFrame();
	    forceFrameAsPage(getCR3(), pcb->pageDirectory);
	    memset(pcb->pageDirectory,0,4096);
	    pcb->stackVirt = 0;               // Wrap down into top of memory
	uint32_t newTable = (uint32_t)allocateFrame();
	    forceFrameAsPage(getCR3(), newTable);
	    pcb->pageDirectory[1023] = newTable | 0x07UL; // User | RW | Present
	    memset(newTable,0,4096);
	uint32_t *table = (uint32_t*)newTable;
	    table[1023] = (uint32_t)allocateFrame() | 0x07UL; // User | RW | Present
	    pcb->stackPhys = table[1023] & 0xfffff000;
	    forceFrameAsPage(getCR3(), pcb->stackPhys);
	    memset(pcb->stackPhys,0,4096);
	    decodeElf(pcb,elf,size);    // Builds virtual-memory and loads program
        forceFrameAsPage(pcb->pageDirectory, 0xb8000);

	    serialPrintf("processs page-dir[32] = %08x\n",pcb->pageDirectory[32]);
	    uint32_t *t = (uint32_t*)(pcb->pageDirectory[32] & 0xfffff000UL);
	    serialPrintf("processs page-dir[32][72] = %08x\n",t[72]);
	    //memDump(t[72]&0xfffff000UL, 4096);

	    forceFramesAsPages(pcb->pageDirectory, kernelLo, kernelHi);

	    systemTSS[0] = 0x30;  // We will simulate a return to force task-switch, TSS in 6
	    //theGdt[13] |= 0x200;  // Mark target task as busy
	uint32_t entryPoint = *((uint32_t*)(elf+24));

	    /* Switch to the new process page-directory and set the initial stack so that
	       we can borrow it for the iret instruction. Control never returns through
	       here so we can destroy the kernel stack in the process - each entry into the
	       kernel for a handler will use a fresh stack.
	    */
	    __asm__ __volatile__("  \
	     cli; \n\
	     mov   $0x2b, %%ax; \n\
	     mov   %%ax, %%ds; \n\
	     mov   %%ax, %%es; \n\
	     mov   %%ax, %%fs; \n\
	     mov   %%ax, %%gs; \n\
	     movl  %%ecx, %%cr3;      # Process page-directory                         \n\
	     xorl  %%esp, %%esp;      # Stack starts at top of virtual address space   \n\
	     mov %%esp, %%eax; \n\
	     pushl $0x2b; \n\
	     pushl %%eax; \n\
	     pushf;                   # EFLAGS on stack                                \n\
	     popl %%eax;              # EFLAGS in register                             \n\
	     #orl  $0x4000, %%eax;     # Set NT (nested task)                           \n\
	     pushl %%eax;                                                              \n\
	     popf;                    # Set EFLAGS now (for kernel) to include NT      \n\
	     andl $0xffffbfff, %%eax  # Wipe NT again                                  \n\
	     orl  $0x0200, %%eax;     # Set IF (interrupt enable)                      \n\
	     pushl %%eax;             # Back on stack for iret                         \n\
	     pushl $0x23; \n\
	     pushl %%ebx; \n\
	     #ljmp $0x33, $0; \n\
	     iret; " : : [target] "b" (entryPoint), [pagedir] "c" (pcb->pageDirectory) );

	    serialPrintf("userMode?\n");
	    //    Long-jump through the selector...
	    //__asm__ __volatile__("ljmp $0x30, $0"); // 0x30 = 48, 6x64-bits offset into GDT
	}

    void __attribute__((fastcall)) syscallHandler(uint32_t gateNum, uint32_t callNum,
                   uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
  intCounters[ gateNum&255 ] ++;
  serialPrintf("Syscall %u: arg0=%08x arg1=%08x arg2=%08x\n", callNum, arg0, arg1, arg2);
  switch(callNum)
  {
      case 0:
          __asm__ __volatile("sti  \n\
                              jmp waiting");    // Reenable interrupts, mainloop.
          break;
  }
}

	void kernel_main(uint32_t *grubMulti)
	{
	    serialPrintf("Multiboot: flags=%08x\n", grubMulti[0]);
	    if( (grubMulti[0] & 0x40)==0 )
	    {
	        serialPrintf("No memory-map from bootloader :(");
	        return;
	    }

	uint32_t mmapSize = grubMulti[11];
	uint32_t *mmap    = (uint32_t*)grubMulti[12];
	    serialPrintf("Multiboot: mmap=%d bytes @ %08x\n", mmapSize, mmap);
	uint32_t mmapRegions = 0;
	    for(uint8_t *t = (uint8_t*)mmap; t<(mmap+mmapSize/4); t += *((uint32_t*)t)+4)
	      if( t[20] == 1)   // Available
	        mmapRegions++;

	struct grubMMap grubMem[mmapRegions];
	uint32_t region = 0;
	    for(uint8_t *t = (uint8_t*)mmap; t<(mmap+mmapSize/4); t += *((uint32_t*)t)+4)
	      if( t[20] == 1)   // Available
	      {
	        grubMem[region].base = *((uint32_t*)(t+4));
	        grubMem[region].length = *((uint32_t*)(t+12));
	        serialPrintf("Multiboot: mmap region %d %uK @ %08x\n", region,
	                     grubMem[region].length / 1024, grubMem[region].base);
	        region++;
	      }

        uint32_t elfLo = -1, elfHi = 0;       // Discovered from multiboot info
    if( (grubMulti[0] & 0x20)==0 )
    {
        serialPrintf("No ELF symbol-map from bootloader :(");
        return;
    }
	uint32_t numElf = grubMulti[7], sizeElf=grubMulti[8], baseElf=grubMulti[9],
	stableIdxElf=grubMulti[10];
	uint32_t *stableElfRecord = baseElf + stableIdxElf*sizeElf;
	char     *stable = (char*)stableElfRecord[3];
    serialPrintf("Multiboot: ELF header has %u symbols @ %08x, each %u-bytes. String-table @ %08x\n",
                 numElf, baseElf, sizeElf, stable);
    for(uint32_t i=1; i<numElf; i++)
    {
        uint32_t *elfRecord = (uint32_t*)(baseElf + sizeElf*i);
        serialPrintf("Multiboot: ELF section %u @ %08x (%u-bytes) %s\n", i,
                     elfRecord[3], elfRecord[5], &stable[elfRecord[0]] );
        if( elfRecord[3] < elfLo )
            elfLo = elfRecord[3];
        if( elfRecord[3] + elfRecord[5] - 1 > elfHi )
            elfHi = elfRecord[3] + elfRecord[5] - 1;
    }

	init_serial();
  initMemory(grubMem, elfLo,elfHi);
	initInterrupts();
  serialPrintf("Boot: grub structure at %08x\n", grubMulti);
	timer_phase(100);
	setHandler(32, tickInterrupt);
	setHandler(14, pfHandler);
    setHandler(0x80, syscallHandler);

  uint8_t *vga = (uint8_t*)0xb8000;
  const char msg[] = "Hello darkness, my old friend";
  for(int i=0; msg[i]!=0; i++)
      vga[i*2] = msg[i];

  serialString((uint8_t*)"\"Memes are dank\" -- Abraham Lincoln\n");
  serialPrintf("Entering procedure %s @ %08x\n", "kernel_main", kernel_main);

	uint8_t* frameArray[25];
	 for (int i = 0; i < 20; i++) {
		 frameArray[i] = allocateFrame();
		 serialPrintf("allocateFrame %x\n", frameArray[i]);
	 }

	 for (int i = 0; i < 10; i++) {
		 freeFrame(frameArray[i]);
		 serialPrintf("freeFrame %x\n", frameArray[i]);
 }
	 for (int i = 10; i < 30; i++) {
		 frameArray[i] = allocateFrame();
		 serialPrintf("allocateFrame %x\n", frameArray[i]);
	 }

     uint32_t initProgram;
 uint32_t initLen = loadInitRd("initrd/init", &initProgram);
     if( initLen>0 )
     {
         serialPrintf("Init: found init process on initrd, len=%u-bytes @ %08x\n",
                      initLen, initProgram);
         createProcess(elfLo,elfHi,initProgram,initLen);
     }


  __asm__ __volatile__("\
  waiting: hlt \n\
       jmp waiting");


}
