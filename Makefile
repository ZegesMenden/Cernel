
build:
	cd build && ninja

flash:
	cd build && ninja
	cd build && picotool load pico-kernel.elf -fx