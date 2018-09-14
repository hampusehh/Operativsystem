#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct _pcb
	{
	    uint32_t *pageDirectory;
	    uint32_t  pid;
	    uint8_t  *stackVirt, *stackPhys;
	} Pcb;
