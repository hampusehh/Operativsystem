#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "user-printf.c"

/*
const char msg[]="hello world\n";
void syscall(uint32_t number, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    __asm__("int $0x80" : : "d" (number), "b" (arg0), "S" (arg1), "a" (arg2));
}

int main(int argc, char **argv)
{
  char charBuffer[300] = "letsgoo";

    syscall(1, (uint32_t)charBuffer, 0, 0);      // WRITE stdout
     printf("raw string\n");
     printf("number: %08x\n", 1234);
     printf("embedded string: %s\n", "inside");
}
*/

char scanMsg[] = "Address 0x10000000 = 00\n";
	void syscall(uint32_t number, uint32_t arg0, uint32_t arg1, uint32_t arg2)
	{
	    __asm__("int $0x80" : : "d" (number), "b" (arg0), "S" (arg1), "D" (arg2));
	}
	uint8_t hexDig(uint8_t x)
	{
	    return (x<10)?x+'0':x-10+'a';
	}
	int main(int argc, char **argv)
	{
	uint8_t *heap = 0x10000000;
	    syscall(2, 10*4096, 0, 0);            // BRK(10 pages)

	    for(int i=0; i<40960; i++)
	        heap[i] = i;

	    for(int i=0; i<11; i++)
	    {
	        scanMsg[14] = '0' + i;
	        scanMsg[21] = hexDig(heap[i*4096+i] >> 4);
	        scanMsg[22] = hexDig(heap[i*4096+i] & 15);
	        syscall(1, scanMsg, 24, 0);
	    }
	    // 11th iteration should be outside the heap and trigger a page-fault.
	}
