CC = C:/Users/camku/.pico-sdk/toolchain/14_2_Rel1/bin/arm-none-eabi-gcc
CFLAGS = -mcpu=cortex-m33 -mthumb -ffreestanding -fno-lto -fno-builtin -fvisibility=hidden
SRC = -c function.c
OUT = -r plugin.o -o plugin.rel.o
PATHADD = C:/Users/camku/.pico-sdk/toolchain/RISCV_ZCB_RPI_2_2_0_3/bin

all:
	$(CC) $(CFLAGS) $(SRC) $(OUT)
