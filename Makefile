run: disk.iso
	qemu-system-i386 -serial file:serial.txt disk.iso
	cat serial.txt

disk.iso: prefix.iso bootp.iso
	rm -f disk.iso
	truncate -s16M disk.iso
	dd if=prefix.iso of=disk.iso count=2048 conv=notrunc
	dd if=bootp.iso of=disk.iso seek=2048 count=14336 conv=notrunc

# Requires root privleges, use the provided copy in the labs
prefix.iso:
	./makePrefix.bash

# Use a bash script as shell variables are awkward in make
bootp.iso: kernel.bin
	./makeBootp.bash

kernel.bin: linker.ld boot.o kernel.o serial.o mem.o ints.o initrd.o archive.o
	i686-elf-gcc -g -T linker.ld -o kernel.bin -ffreestanding -O2 -nostdlib boot.o kernel.o serial.o mem.o ints.o initrd.o archive.o -lgcc

boot.o: boot.s
	i686-elf-as boot.s -o boot.o

kernel.o: kernel.c
	i686-elf-gcc -g -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

clean:
	rm -f boot.o kernel.o disk.iso bootp.iso kernel.bin serial.o mem.o ints.o initrd.o archive.o

serial.o: serial.c serial.h
	i686-elf-gcc -g -c serial.c -o serial.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

mem.o: mem.c mem.h
	i686-elf-gcc -g -c mem.c -o mem.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

ints.o: ints.c ints.h
	i686-elf-gcc -g -c ints.c -o ints.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

dbg: disk.iso
	qemu-system-i386 -s -S disk.iso -serial file:serial.txt -monitor stdio

initrd.o: initrd.c initrd.h
	i686-elf-gcc -g -c initrd.c -o initrd.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

archive.tar: initrd/init
	tar cf archive.tar initrd/

archive.o: archive.tar
	i686-elf-objcopy -I binary -O elf32-i386 -B i386 archive.tar archive.o

initrd/init: init.c crt0.s
	i686-elf-gcc -g -o initrd/init init.c crt0.s -ffreestanding -O1 -nostdlib
