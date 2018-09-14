#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
extern char _binary_archive_tar_start, _binary_archive_tar_end;

	// Different contract to strcmp: true means they match, false means they differ
	bool strSame(uint8_t *a, uint8_t *b)
	{
	  while(*a != 0)
	      if(*(a++) != *(b++))
	          return false;
	  return true;
	}

	uint32_t decodeOctal11(uint8_t *field)
	{
	uint32_t result = 0;
	    for(int i=0; i<11; i++)
	    {
	        result *= 8;
	        result += field[i] - '0';
	    }
	    return result;
	}

	uint32_t loadInitRd(const char *filename, uint32_t *target)
	{
	uint8_t *initrd    = (uint8_t*)&_binary_archive_tar_start;
	uint8_t *initrdEnd = (uint8_t*)&_binary_archive_tar_end;
	uint8_t *record = initrd;
	    while(record < initrdEnd)
	    {
	        if( record[156] == '5' )
	            record += 512;
	        else if( record[156] == '0' )
	        {
	            uint32_t dataSize = decodeOctal11(record+124);
	            if( strSame(record, (uint8_t*)filename) )
	            {
	                *target = (uint32_t)(record+512);
	                return dataSize;
	            }
	            record += (512 + (dataSize + 511)) & 0xfffffe00;
	        }
	        else
	            record += 512;
	    }
	    return 0;
	}
