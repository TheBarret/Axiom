#!/bin/bash
set -e

SRC_DIR="."
BUILD_DIR="./build"
mkdir -p "$BUILD_DIR"

CFLAGS="-O3 -march=native -Wall -Wextra -DNDEBUG -std=c11"
LDFLAGS="-lm"

echo "Building Axiom runtime..."

gcc $CFLAGS -c "$SRC_DIR/gates.c" -o "$BUILD_DIR/gates.o"
gcc $CFLAGS -c "$SRC_DIR/adder.c" -o "$BUILD_DIR/adder.o"
gcc $CFLAGS -c "$SRC_DIR/alu.c" -o "$BUILD_DIR/alu.o"
gcc $CFLAGS -c "$SRC_DIR/ccel.c" -o "$BUILD_DIR/ccel.o"
gcc $CFLAGS -c "$SRC_DIR/bus.c" -o "$BUILD_DIR/bus.o"
gcc $CFLAGS -c "$SRC_DIR/cpu.c" -o "$BUILD_DIR/cpu.o"
gcc $CFLAGS -c "$SRC_DIR/main.c" -o "$BUILD_DIR/main.o"

gcc $CFLAGS "$BUILD_DIR/gates.o" "$BUILD_DIR/adder.o" "$BUILD_DIR/alu.o" "$BUILD_DIR/ccel.o" "$BUILD_DIR/bus.o" "$BUILD_DIR/cpu.o" "$BUILD_DIR/main.o" -o "$BUILD_DIR/axiom" $LDFLAGS

echo "Finished! ($BUILD_DIR/axiom)"
