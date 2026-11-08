CC      ?= gcc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra
SRC      = src/cube.c src/solver.c src/api.c
WASM_OUT = ../web/public/solver.wasm

.PHONY: all test wasm clean

all: rubik

rubik: $(SRC) src/cli.c
	$(CC) $(CFLAGS) -o $@ $^

test_solver: $(SRC) tests/test_solver.c
	$(CC) $(CFLAGS) -o $@ $^

test: test_solver
	./test_solver

# Compile the same C code to WebAssembly (no Emscripten needed, just clang + wasm-ld).
wasm: $(SRC) src/wasm_support.c
	clang --target=wasm32 -O3 -std=c99 -ffreestanding -nostdlib -fno-builtin \
	  -Wl,--no-entry -Wl,--export-dynamic -Wl,--initial-memory=16777216 \
	  -Wl,--strip-all -o $(WASM_OUT) $^
	@echo "Built $(WASM_OUT)"

clean:
	rm -f rubik test_solver
