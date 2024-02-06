all: cdimage.iso

#ARCH=aarch64
ARCH=x86_64
CC=clang -target ${ARCH}-unknown-win32-coff
LD=lld-link
LDFLAGS=-flavor link -subsystem:efi_application -entry:efi_main

KUROKO_CS = builtins.c chunk.c compiler.c debug.c exceptions.c memory.c \
            obj_base.c obj_bytes.c obj_dict.c object.c obj_function.c obj_gen.c \
            obj_list.c obj_numeric.c obj_range.c obj_set.c obj_str.c obj_tuple.c \
            obj_typing.c obj_slice.c obj_long.c scanner.c table.c value.c vm.c \
            sys.c parseargs.c modules/module_dis.c modules/module_fileio.c

SRC = $(patsubst %,kuroko/src/%,${KUROKO_CS}) $(wildcard src/*.c)
OBJ = $(patsubst %.c,%.o,$(sort $(SRC)))

CFLAGS = -m64 -ffreestanding -D__efi -DKRK_STATIC_ONLY -DKRK_DISABLE_THREADS -DNDEBUG -DEFI_PLATFORM \
         -Isrc/ -Ikuroko/src/ -Iinclude -I/usr/include/efi -O2 -U_WIN32 \
         -fno-stack-protector -fshort-wchar -mno-red-zone

ifeq (${ARCH},x86_64)
  EFI_ARCH=x64
else ifeq (${ARCH},aarch64)
  EFI_ARCH=AA64
endif

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

kuroko.efi: $(OBJ)
	$(LD) $(LDFLAGS) -out:$@ $(OBJ)

stage:
	mkdir -p stage

stage/disk.img: kuroko.efi Makefile krk/*.krk test/* | stage
	-rm -f $@
	fallocate -l 4M $@
	mkfs.fat $@
	mmd -i $@ efi efi/boot krk test
	mcopy -i $@ kuroko.efi ::efi/boot/boot${EFI_ARCH}.efi
	mcopy -i $@ krk/*.krk ::krk/
	mcopy -i $@ test/* ::test/

cdimage.iso: stage/disk.img
	xorriso -as mkisofs -c bootcat -e disk.img -no-emul-boot -isohybrid-gpt-basdat -o $@ stage

.PHONY: clean
clean:
	-rm -rf src/*.o *.efi *.so kuroko/src/*.o kuroko/src/modules/*.o *.iso stage/disk.img

.PHONY: run

ifeq (${ARCH},x86_64)
run: cdimage.iso
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -cdrom $< -enable-kvm -net none -m 1G
else ifeq (${ARCH},aarch64)
run: cdimage.iso
	qemu-system-aarch64 -M virt -cpu cortex-a72 -bios /usr/share/AAVMF/AAVMF_CODE.fd -smp 4 -device ramfb -device qemu-xhci -device usb-kbd -cdrom $< -net none -m 1G
endif
