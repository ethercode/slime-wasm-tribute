CC = clang
CPP = clang++
TARGET = slime.wasm
SRC_DIR = src
BUILD_DIR = docs

# WASM-optimised flags:
#   -Oz             : Optimise for size (negligible perf difference at this scale)
#   -flto / --lto-O3: Link-time optimisation across translation units
#   -fno-exceptions : No C++ exception metadata (we don't use exceptions)
#   -fno-rtti       : No runtime type info (we don't use dynamic_cast/typeid)
#   -mbulk-memory   : WASM bulk-memory ops (memory.fill / memory.copy)
#   -nostdlib        : No standard library (we provide our own minimal replacements)
#   --no-entry       : No main() entry point (we export functions instead)
#   --export-all     : Export all symbols for JS access
#   --strip-all      : Remove debug info and symbol names from binary
CFLAGS = --target=wasm32 -std=c++17 -Oz -flto \
         -fno-exceptions -fno-rtti \
         -mbulk-memory \
         -nostdlib \
         -Wl,--no-entry -Wl,--export-all \
         -Wl,--allow-undefined-file=imports.sym \
         -Wl,--strip-all -Wl,--lto-O3 \
         -Wall

# Source files
SRCS = src/button.cpp src/main.cpp src/mouse.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(BUILD_DIR)
	$(CPP) $(CFLAGS) -o $(BUILD_DIR)/$(TARGET) $(SRCS)

clean:
	rm -f $(BUILD_DIR)/$(TARGET)

serve:
	python3 -m http.server 8000 -d $(BUILD_DIR)
