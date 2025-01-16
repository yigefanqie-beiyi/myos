BUILD = ./build
BOOT = ./boot
KERNEL = ./kernel
DEVICE = ./device
LIB = ./lib 
USER = ./user 
ENTRY_POINT = 0xc0001500
INCLUDE = -I lib/ -I kernel/ -I device/ -I include/ -I lib/kernel/ -I fs/

CFLAGS:= -m32
CFLAGS+= -fno-builtin
CFLAGS+= -c
CFLAGS+= -fno-stack-protector

$(BUILD)/boot/%.bin: $(BOOT)/%.S 
	@nasm -I include/ -o $@ $<
$(BUILD)/kernel/%.o: $(KERNEL)/%.S 
	@nasm -f elf -o $@ $<
$(BUILD)/lib/%.o: lib/kernel/%.S
	@nasm -f elf -o $@ $<

$(BUILD)/kernel/%.o: $(KERNEL)/%.c 
	@gcc $(CFLAGS) $(INCLUDE) -o $@ $<

$(BUILD)/user/%.o: ./user/%.c 
	@gcc $(CFLAGS) $(INCLUDE) -o $@ $<

$(BUILD)/device/%.o: $(DEVICE)/%.c 
	@gcc $(CFLAGS) $(INCLUDE) -o $@ $<

$(BUILD)/fs/%.o: fs/%.c 
	@gcc $(CFLAGS) $(INCLUDE) -o $@ $<

$(BUILD)/lib/%.o: lib/%.c 
	@gcc $(CFLAGS) $(INCLUDE) -o $@ $<

$(BUILD)/lib/%.o: lib/kernel/%.c 
	@gcc $(CFLAGS) $(INCLUDE) -o $@ $<

$(BUILD)/kernel.bin: \
		$(BUILD)/kernel/main.o \
		$(BUILD)/kernel/init.o \
		$(BUILD)/kernel/interrupt.o \
		$(BUILD)/lib/print.o \
		$(BUILD)/kernel/kernel.o \
		$(BUILD)/device/timer.o \
		$(BUILD)/kernel/memory.o \
		$(BUILD)/kernel/debug.o \
		$(BUILD)/lib/string.o \
		$(BUILD)/lib/bitmap.o \
		$(BUILD)/kernel/thread.o \
		$(BUILD)/lib/list.o \
		$(BUILD)/kernel/switch.o \
		$(BUILD)/kernel/sync.o \
		$(BUILD)/device/console.o \
		$(BUILD)/device/keyboard.o \
		$(BUILD)/device/ioqueue.o \
		$(BUILD)/user/tss.o \
		$(BUILD)/user/process.o \
		$(BUILD)/user/syscall.o \
		$(BUILD)/user/syscall-init.o \
		$(BUILD)/lib/stdio.o \
		$(BUILD)/lib/stdio-kernel.o \
		$(BUILD)/device/ide.o \
		$(BUILD)/fs/fs.o \
		$(BUILD)/fs/inode.o \
		$(BUILD)/fs/file.o \
		$(BUILD)/fs/dir.o \
		$(BUILD)/user/fork.o \
		$(BUILD)/user/shell.o \
		$(BUILD)/user/buildin_cmd.o \

		@ld -m elf_i386 -Ttext $(ENTRY_POINT) -e main $^ -o $@ 
		dd if=/home/myos/build/kernel.bin of=/home/myos/master.img bs=512 count=200 seek=9 conv=notrunc

.PHONY: bochs
bochs:$(BUILD)/kernel.bin
	bochs -q

.PHONY: qemu
qemu:$(BUILD)/kernel.bin
	qemu-system-i386 \
	-m 32M \
	-boot c \
	-hda $<

.PHONY: clean
clean:
	@rm -f $(BUILD)/device/*.o
	@rm -f $(BUILD)/kernel/*.o
	@rm -f $(BUILD)/lib/*.o
	@rm -f $(BUILD)/user/*.o
	@rm -f $(BUILD)/*.bin
	@rm -f $(BUILD)/*.o

.PHONY: test
test:
	make build/boot/boot.bin
	make build/boot/loader.bin
	dd if=/home/myos/build/boot/boot.bin of=/home/myos/master.img bs=512 count=1 conv=notrunc
	dd if=/home/myos/build/boot/loader.bin of=/home/myos/master.img bs=512 count=4 seek=2 conv=notrunc