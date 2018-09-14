#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

const char msg[]="hello world\n";

void syscall(uint32_t number, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
    __asm__("int $0x80" : : "d" (number), "b" (arg0), "S" (arg1), "a" (arg2));
}

int main(int argc, char **argv)
{
    syscall(1, (uint32_t)msg, 0, 0);      // WRITE stdout
}
