void __attribute__((fastcall)) ignoreInterrupt(uint32_t gateNum);
void timer_phase(int hz);
void __attribute__((fastcall)) tickInterrupt(uint32_t gateNum);
void initInterrupts();
void setHandler(uint8_t index, void __attribute__((fastcall)) (*handler)(uint32_t));
