CC = clang
CPP = clang++
TARGET = slime.wasm
SRC_DIR = src
BUILD_DIR = docs

# Use -O3 for optimization, -nostdlib because we don't have a standard library in native WASM
# -Wl,--no-entry because we don't have a main() in the traditional sense (we export functions)
# -Wl,--export-all to export all functions for now (can be restricted later)
# -Wl,--allow-undefined to allow unresolved symbols (we will import some from JS)
# CFLAGS = --target=wasm32 -O3 -nostdlib -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wall
CFLAGS = --target=wasm32 -std=c++17 -O3 -nostdlib -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined-file=imports.sym -Wall

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
