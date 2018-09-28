#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "x86.h"
#define COM1 0x3f8

void init_serial() {
   outb(COM1 + 1, 0x00);    // Disable all interrupts
   outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   outb(COM1 + 1, 0x00);    //                  (hi byte)
   outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void serialByte(uint8_t c)
{
    while( (inb(COM1+5) & 0x20) == 0)
      ;
    outb(COM1,c);
}

// Caution - this will act as a delay!
void serialString(uint8_t *str)
{
  while(*str != 0)
    serialByte(*(str++));
}
#include <stdarg.h>

	// Number to ascii in somewhat arbitrary base (2-36).
	int numAscii(uint32_t num, uint32_t base, uint8_t *opBuffer)
	{
	int size=0;
	    if(num==0)
	        opBuffer[size++] = '0';
	    while(num>0)
	    {
	        uint8_t dig      = num % base;
	        opBuffer[size++] = ((dig<10) ? '0' : 'a'-10) + dig;
	        num              = num / base;
	    }
	    opBuffer[size] = 0;
	int lastPos = size-1;
	    for(int j=0; j<size/2; j++)
	    {
	        uint8_t swap        = opBuffer[j];
	        opBuffer[j]         = opBuffer[lastPos-j];
	        opBuffer[lastPos-j] = swap;
	    }
	    return size;
	}


	void serialPadding(uint8_t padChar, int len, int minLen)
	{
	    if( len>=minLen )  return;
	    for(int i=0; i<minLen-len; i++)
	        serialByte(padChar);
	}


	// Internal entry point after variadic args converted to va_list
	void Vprintf(const char *fmt, va_list va)
	{
	char c;
	uint8_t buffer[33];
	    do
	    {
	        c = *(fmt++);
	        if( c!=0 && c!='%' )
	            serialByte(c);
	        else if( c==0 )
	            break;
	        else
	        {
	            uint8_t pad = ' ';
	            uint8_t   *sptr;
	            int     minLen = 0, len, v;
	            c = *(fmt++);
	            if(c==0)  break;
	            if(c=='0')
	            {
	                pad = '0';
	                c = *(fmt++);
	                if(c==0)  break;
	            }
	            while(c>='0' && c<='9')
	            {
	                minLen = minLen * 10 + c - '0';
	                c = *(fmt++);
	                if(c==0)  break;
	            }

	            switch(c)
	            {
	                case 0:                    break;
	                case '%': serialByte('%'); break;
	                case 'd':
	                    v = va_arg(va, int);
	                    if(v>=0)
	                        len = numAscii(v,10,buffer);
	                    else
	                    {
	                        serialByte('-');
	                        len = numAscii(-v,10,buffer);
	                    }
	                    serialPadding(pad, len, minLen);
	                    serialString(buffer);
	                    break;
	                case 'u':
	                    v = va_arg(va, unsigned int);
	                    len = numAscii(v,10,buffer);
	                    serialPadding(pad, len, minLen);
	                    serialString(buffer);
	                    break;
	                case 'x':
	                    v = va_arg(va, unsigned int);
	                    len = numAscii(v,16,buffer);
	                    serialPadding(pad, len, minLen);
	                    serialString(buffer);
	                    break;
	                case 'b':     // Unusual fmt-specifier - binary
	                    v = va_arg(va, unsigned int);
	                    len = numAscii(v,2,buffer);
	                    serialPadding(pad, len, minLen);
	                    serialString(buffer);
	                    break;
	                case 's':
	                    sptr = va_arg(va, uint8_t *);
	                    serialString(sptr);
	                    break;
	            }
	        }
	    }
	    while( c != 0 );
	}

	// The entry-point that we call
	void printf(const char *fmt, ...)
	{
	va_list va;
	    va_start(va,fmt);
	    Vprintf(fmt, va);
	    va_end(va);
	}
