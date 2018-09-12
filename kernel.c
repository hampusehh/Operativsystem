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

void kernel_main(void)
{
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
