#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "x86.h"
#include "ints.h"
// IDT - Gates are 64-bits, 256 entries = 2048 bytes
// One copy of intHandler, padded to 32-bytes each: 8192 bytes

uint8_t idtPtr[6];
uint8_t theIdt[2048];
uint8_t intHandlers[8192];
uint32_t intCounters[256] = {0};
uint8_t intHandlerMasterIrq[] = {
    0x60,                          // pushal
    0xfc,                          // cld
    0x0f, 0x20, 0xd8,              // movl cr3, eax
    0x50,                          // push eax
    0x31, 0xc9,                    // xorl ecx, ecx
    0xb1, 0x00,                    // movb $0, cl  (gatenumber)
    0xe8, 0x01, 0x02, 0x03, 0x04,  // Call 0x04030201
    0xb0, 0x20,                    // movb $0x20, al
    0xe6, 0x20,                    // outb %al, $0x20  EOI to master 8259
    0x58,                          // pop  eax
    0x0f, 0x22, 0xd8,              // movl eax, cr3
    0x61,                          // popal
    0xcf                           // iret
};

uint8_t intHandler[] = {
    0x60,                          // pushal
    0xfc,                          // cld
    0x0f, 0x20, 0xd8,              // movl cr3, eax
    0x50,                          // push eax
    0x31, 0xc9,                    // xorl ecx, ecx
    0xb1, 0x00,                    // movb $0, cl  (gatenumber)
    0xe8, 0x01, 0x02, 0x03, 0x04,  // Call 0x04030201
    0x58,                          // pop  eax
    0x0f, 0x22, 0xd8,              // movl eax, cr3
    0x61,                          // popal
    0xcf                           // iret
};

uint8_t syscallGlue[] = {
    0x60,                          // pushal
    0xfc,                          // cld
    0x0f, 0x20, 0xd8,              // movl cr3, eax
    0x50,                          // push eax
    0x31, 0xc9,                    // xorl ecx, ecx
    0xb1, 0x00,                    // movb $0, cl  (gatenumber)
    0x50, 0x56, 0x53,              // Push eax, esi, ebx
    0xe8, 0x01, 0x02, 0x03, 0x04,  // Call 0x04030201
    0x58,                          // pop  eax
    0x0f, 0x22, 0xd8,              // movl eax, cr3
    0x61,                          // popal
    0xcf                           // iret
};

void __attribute__((fastcall)) ignoreInterrupt(uint32_t gateNum)
{
  intCounters[ gateNum&255 ] ++;
  serialPrintf("Ignoring interrupt %u\n",gateNum);
}

// Default handler: fastcall ABI forces the argument into ECX.

void timer_phase(int hz)
{
    int divisor = 1193180 / hz;       /* Calculate our divisor */
    outb(0x43, 0x36);             /* Set our command byte 0x36 */
    outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    outb(0x40, divisor >> 8);     /* Set high byte of divisor */
}

void __attribute__((fastcall)) tickInterrupt(uint32_t gateNum)
	{
	  intCounters[ gateNum&255 ] ++;
	  if( (intCounters[32] & 0xf) == 0)
	  {
	      for(uint32_t i=0; i<256; i++)
	          showCounter(i, intCounters[i]);
	  }
	}

void showCounter(uint32_t position, uint32_t value){
    uint8_t *vga = (uint8_t*)0xb8000;
    uint8_t opBuff[4] = {'0', '0', '0', '0'};

    numToHex(value, opBuff);

    position = position * 2 * 5;
    vga[position + 0] = opBuff[3];
    vga[position + 2] = opBuff[2];
    vga[position + 4] = opBuff[1];
    vga[position + 6] = opBuff[0];

}



void numToHex(uint32_t num, uint8_t *opBuffer)
{
    int size=0;
    while(num>0)
    {
        uint8_t dig      = num % 16;
        opBuffer[size++] = ((dig<10) ? '0' : 'a'-10) + dig;
        num              = num / 16;
    }
}


void initInterrupts()
{
    // 8 bytes per gate, individual handler address for each
    for(int i=0; i<256; i++)
    {
        uint32_t serviceR = (uint32_t)&intHandlers[i*32];
        uint8_t *gate     = &theIdt[i*8];
        gate[0] = serviceR;
        gate[1] = (serviceR>>8);
        gate[2] = 8;  // Code descriptor in GDT
        gate[3] = 0;  // Code descriptor high bits
        gate[4] = 0;  // Unused
        gate[5] = 0x8e; // Flags
        gate[6] = serviceR>>16;
        gate[7] = serviceR>>24;
        setHandler(i, ignoreInterrupt);
    }
    theIdt[128*8+5] = 0xee;     // Overwrite DPL on int 80h for user-mode call.

    // Install the IDT
    uint16_t *t = (uint16_t*)idtPtr;     // Avoid the type-punning warning
    *t = 2047;
    uint32_t *t2 = (uint32_t*)&idtPtr[2];
    *t2 = (uint32_t)theIdt;

    // Reprogram 8259's - from Bran's Kernel Dev Tutorial
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);   // Base address 32
    outb(0xA1, 0x28);   // Base address 40
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);    // Enable all eight IRQs
    outb(0xA1, 0x0);    // Enable all eight IRQs
    __asm__ __volatile__("\n\
    movl %[tab], %%eax     \n\
    lidt (%%eax)            \n\
    sti                    # Enable interrupts   \n\
    " : : [tab] "p" (&idtPtr) : "%eax");
}

void setHandler(uint8_t index, void __attribute__((fastcall)) (*handler)(uint32_t))
{
uint32_t tPos = 32*index;
uint8_t *intRaw = (index>=32 && index<40) ? intHandlerMasterIrq : intHandler;
    if(index==0x80)
        intRaw = syscallGlue;
    for(int i=0; i<32; i++)
        intHandlers[tPos+i] = intRaw[i];
    intHandlers[tPos+9] = index;
    // Encode call target as offset for relative addressing mode.
    // EIP after instruction -> procedure handler.
uint32_t callOffset = (uint32_t)handler - (uint32_t)&intHandlers[tPos+(index==0x80?18:15)];
uint32_t *callAddr = (uint32_t*)(&intHandlers[tPos+(index==0x80?14:11)]);
    *callAddr = callOffset;
}
