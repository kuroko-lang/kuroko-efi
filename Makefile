all: cdimage.iso

KUROKO_CS = builtins.c chunk.c compiler.c debug.c exceptions.c fileio.c memory.c \
            obj_base.c obj_bytes.c obj_dict.c object.c obj_function.c obj_gen.c \
            obj_list.c obj_numeric.c obj_range.c obj_set.c obj_str.c obj_tuple.c \
            obj_typing.c scanner.c table.c value.c vm.c

SRC = $(patsubst %,kuroko/src/%,${KUROKO_CS}) $(wildcard src/*.c)
OBJ = $(patsubst %.c,%.o,$(sort $(SRC)))

CFLAGS = -DSTATIC_ONLY -DSTRICTLY_NO_THREADS -DNDEBUG -DKRK_ENABLE_DEBUG \
         -ffreestanding -Isrc/ -Ikuroko/src/ -nostdinc -Iinclude -fno-stack-protector -fpic \
         -DEFI_PLATFORM -fshort-wchar -I/usr/include/efi -mno-red-zone \
         -I/usr/include/efi/x86_64 -DEFI_FUNCTION_WRAPPER -O2 -g

SECTIONS = -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

kuroko.so: $(OBJ)
	$(LD) $(OBJ) /usr/lib/crt0-efi-x86_64.o -nostdlib -znocombreloc -T /usr/lib/elf_x86_64_efi.lds -shared -Bsymbolic -L /usr/lib -lefi -lgnuefi -o $@

kuroko.efi: kuroko.so
	objcopy $(SECTIONS) --target=efi-app-x86_64 $< $@

stage:
	mkdir -p stage

stage/disk.img: kuroko.efi Makefile | stage
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
