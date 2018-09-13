#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "serial.h"
#include "ints.h"
#include "mem.h"

#if defined(__linux__)
#error "You must use the cross-compiler, not the linux compiler!"
#endif
#if !defined(__i386__)
#error "Wrong architecture, use the i686-elf-gcc compiler."
#endif


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

 uint8_t* frameArray[25];
  for (int i = 0; i < 20; i++) {
    frameArray[i] = allocateFrame();
    serialPrintf("Alloc: Allocating frame %x\n", frameArray[i]);
  }

  serialPrintf("Boot: grub structure at %08x\n", grubMulti);

  initMemory();
  uint8_t *vga = (uint8_t*)0xb8000;
  const char msg[] = "Hello darkness, my old friend";
  for(int i=0; msg[i]!=0; i++)
      vga[i*2] = msg[i];
  init_serial();
  serialString((uint8_t*)"\"Memes are dank\" -- Abraham Lincoln\n");
  serialPrintf("Entering procedure %s @ %08x\n", "kernel_main", kernel_main);
  initInterrupts();
  timer_phase(100);
  setHandler(32, tickInterrupt);

  __asm__ __volatile__("\
  waiting: hlt \n\
       jmp waiting");
}
