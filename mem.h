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
void forceFrameAsPage(uint32_t *pageDir, uint32_t frame);
void forceFramesAsPages(uint32_t *pageDir, uint32_t loAddress, uint32_t hiAddress);
void memset(uint8_t *target, uint8_t val, uint32_t size);
uint32_t  systemTSS[26] __attribute__((aligned(4096)));
