.section .multiboot
	.align 4
	.long 0x1BADB002       // Magic number for grub
	.long 3                // Flags: align on page boundaries and provide memory map
	.long -(0x1BADB002+3)  // Checksum of magic and flags for grub

	.section .bss          // Reserve uninitialised space for stack
	.align 16
	.global stack_bottom, stack_top   // Make visible to C code
	stack_bottom:
	.skip 16384
	stack_top:

	.section .text
	.global _start                // Make symbol visible
	.global _stop
	.type _start, @function       // Help the debugger
	_start:
	        mov $stack_top, %esp  // Set a stack
	        call kernel_main      // Run the C code
	        // Should not return here... but in case...
	_stop:  cli                   // Disable interrupts
	        hlt                   // Halt the processor
	        jmp _stop             // NMI can resume processor
	
