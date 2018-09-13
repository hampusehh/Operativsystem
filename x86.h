#include <stdint.h>
	#include <stdbool.h>

	/*-
	 | Support for x86 operations that are not exposed natively in C. Each of these
	 | is a fragment of inline-assembly (a way of injecting assembly code into the
	 | compiled program). Each one is wrapped in an inline procedure so that the
	 | code is inserted into the caller - hence the definition in a header. The
	 | __volatile__ tag roughly means "don't move this code around it affects the
	 | state of the machine". This stops the C compiler from attempting to optimize
	 | the program by moving the inlined code before or after statements in the
	 | calling code.
	-*/


	// Output a byte to an I/O port.
	inline void outb(uint16_t port, uint8_t value)
	{
	    __asm__("outb %0, %1" :: "a"(value), "d"(port));
	}

	// Input a byte from an I/O port.
	inline uint8_t inb (uint16_t port)
	{
	    uint8_t result;
	    __asm__ __volatile__("inb %1, %0" : "=a" (result) : "dN" (port));
	    return result;
	}

	inline uint32_t *getCR3()
		{
		uint32_t *cr3;
		    __asm__ __volatile__("movl %%cr3, %[val]" : [val] "=g" (cr3));
		    return cr3;
		}

		// Write to the control-register 3 (the address of the page-directory).
		inline void setCR3(uint32_t pagedir)
		{
		  __asm__ __volatile__("movl %[val], %%eax; \n\
		                        movl %%eax, %%cr3" : : [val] "g" (pagedir) : "%eax");
		}

		// Set the paging flag in the processor control register
		inline void activatePaging()
		{
		    __asm__ __volatile__("                                                  \
		     movl %%cr0, %%eax;       # Get the current control-flags             \n\
		     orl  $0x80000000, %%eax  # Switch on paging                          \n\
		     movl %%eax, %%cr0        # Set the control-flags " : : : "%eax" );
		}
