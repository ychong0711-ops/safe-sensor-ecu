# Top-Level Build & Verification Automation (`safe-sensor-ecu`)
# Targets:
#   make codegen   -> Auto-generate C/Rust RTE FFI + Train & Quantize 6->32->3 Q2.13 FNN + IBP Bounds
#   make rust-host -> Compile `#![no_std]` + `#![forbid(unsafe_code)]` Rust staticlib (`rustc`)
#   make arm-check -> Cross-compile bare-metal Cortex-M33 PMSAv8 partition manager (`arm-none-eabi-gcc`)
#   make hil-test  -> Run 11,500-cycle HIL + Magnetic Spoofing + Hardware MMU Fault-Injection Suite
#   make all       -> End-to-end build, cross-compilation verification, and HIL safety validation

PYTHON      := python3
CC_HOST     := gcc
CC_ARM      := arm-none-eabi-gcc
SIZE_ARM    := arm-none-eabi-size
CARGO       := cargo

BUILD_DIR   := build
RUST_LIB    := target/release/libswc_rust_rte_entry.a

.PHONY: all codegen rust-host arm-check hil-test clean

all: codegen rust-host arm-check hil-test

codegen:
	$(PYTHON) rte_gen/generate_rte.py
	$(PYTHON) sim/train_and_quantize.py

rust-host:
	$(CARGO) build --release

arm-check: $(BUILD_DIR)
	$(CC_ARM) -mcpu=cortex-m33 -mthumb -Os -g3 \
		-Wall -Wextra -Werror -ffunction-sections -fdata-sections \
		-Ibsw_c/include -c bsw_c/src/mpu_stm32h5_pmsav8.c \
		-o $(BUILD_DIR)/mpu_stm32h5_pmsav8.o
	@echo "--- Bare-Metal ARMv8-M (Cortex-M33) Partition Manager Footprint ---"
	$(SIZE_ARM) $(BUILD_DIR)/mpu_stm32h5_pmsav8.o

hil-test: $(BUILD_DIR) rust-host
	$(CC_HOST) -O2 -Wall -Wextra -Werror -Ibsw_c/include \
		hil_sim/hil_runner.c $(RUST_LIB) \
		-Wl,--gc-sections -ldl -lpthread -o $(BUILD_DIR)/hil_runner
	@echo "--- Executing 11,500-Cycle HIL & Hardware MPU Fault-Injection Suite ---"
	./$(BUILD_DIR)/hil_runner | tee $(BUILD_DIR)/hil_results.json

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) target
