#define COM1 0x3f8

	void init_serial();
	void serialByte(uint8_t);
	void serialString(uint8_t *);
    void serialPrintf(const char *, ...);
