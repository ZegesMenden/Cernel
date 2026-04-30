TOOLCHAIN_DIR ?= C:/Users/camku/.pico-sdk/toolchain/RISCV_ZCB_RPI_2_2_0_3/bin

CC      ?= $(TOOLCHAIN_DIR)/riscv32-unknown-elf-gcc.exe
# On Windows, `python` may resolve to the Microsoft Store alias. Prefer the
# Python launcher which will pick an installed Python.
PYTHON  ?= py -3.14

SRC     := function.c
OBJ     := plugin.o
ELF     := plugin.rel.o
HDR     := ../plugin.rel.o.h

# Match the Pico SDK RISC-V build flags (see build/compile_commands.json)
ARCH_FLAGS := -march=rv32ima_zicsr_zifencei_zba_zbb_zbs_zbkb_zca_zcb_zcmp -mabi=ilp32

# This toolchain's ld does not support producing ET_DYN (shared/PIE) images.
# Instead, we produce an ET_EXEC image linked at VMA 0 and compiled with
# PC-relative addressing (medany) and with small-data disabled so it can be
# loaded at any base address without runtime relocations.
PLUGIN_CODEMODEL_FLAGS := -mcmodel=medany -msmall-data-limit=0

CFLAGS  := $(ARCH_FLAGS) $(PLUGIN_CODEMODEL_FLAGS) -O2 -ffreestanding -fno-builtin -fno-lto -fvisibility=hidden \
	-ffunction-sections -fdata-sections -g0

# Build a loadable executable ELF (ET_EXEC) linked at 0.
# Entry point is set to the exported function so loader_load_elf() returns a
# callable address.
LDFLAGS := $(ARCH_FLAGS) -nostdlib -Wl,--gc-sections -Wl,--no-undefined \
	-Wl,-T,plugin.ld -Wl,-e,calculate_sqrt -lgcc

.PHONY: all clean

all: $(HDR)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@


$(ELF): $(OBJ) plugin.ld
	$(CC) $(LDFLAGS) $< -o $@

$(HDR): $(ELF) bin2h.py
	$(PYTHON) bin2h.py $(ELF) -o $(HDR) --name plugin_rel

clean:
	$(RM) $(OBJ) $(ELF)
