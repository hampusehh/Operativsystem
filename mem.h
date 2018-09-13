#define DataType  2    // Read/Write, non-accessed, expand-up
#define StackType 6    // Read/Write, non-accessed, expand-down
#define TssType   9    // Initially set to not-busy
#define CodeType  10   // Execute/Read, non-accessed, non-conforming
uint32_t *freeList;
uint32_t numFree;
#define min(x,y) ((x)<(y)?(x):(y))
struct grubMMap {
	  uint32_t base;
	  uint32_t length;
	};
