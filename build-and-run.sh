#!/bin/bash

BUILD_DIR=debug-build

mkdir -p "$BUILD_DIR"

cmake -S . -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target HwInfo -j
./debug-build/HwInfo
