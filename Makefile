all: cdimage.iso

CC=x86_64-w64-mingw32-gcc

KUROKO_CS = builtins.c chunk.c compiler.c debug.c exceptions.c fileio.c memory.c \
            obj_base.c obj_bytes.c obj_dict.c object.c obj_function.c obj_gen.c \
            obj_list.c obj_numeric.c obj_range.c obj_set.c obj_str.c obj_tuple.c \
            obj_typing.c scanner.c table.c value.c vm.c

SRC = $(patsubst %,kuroko/src/%,${KUROKO_CS}) $(wildcard src/*.c)
OBJ = $(patsubst %.c,%.o,$(sort $(SRC)))

CFLAGS = -m64 -ffreestanding -D__efi -DSTATIC_ONLY -DKRK_DISABLE_THREADS -DNDEBUG -DEFI_PLATFORM \
         -Isrc/ -Ikuroko/src/ -nostdinc -Iinclude -I/usr/include/efi -I/usr/include/efi/x86_64 -O2 -U_WIN32
LDFLAGS = -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

kuroko.efi: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

stage:
	mkdir -p stage

stage/disk.img: kuroko.efi Makefile krk/*.krk test/* | stage
	-rm -f $@
	fallocate -l 4M $@
	mkfs.fat $@
	mmd -i $@ efi efi/boot krk test
	mcopy -i $@ kuroko.efi ::efi/boot/bootx64.efi
	mcopy -i $@ krk/*.krk ::krk/
	mcopy -i $@ test/* ::test/

cdimage.iso: stage/disk.img
	xorriso -as mkisofs -c bootcat -e disk.img -no-emul-boot -isohybrid-gpt-basdat -o $@ stage

.PHONY: clean
clean:
	-rm -rf src/*.o *.efi *.so kuroko/src/*.o *.iso stage/disk.img

.PHONY: run

run: cdimage.iso
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -cdrom $< -enable-kvm -net none -m 1G
