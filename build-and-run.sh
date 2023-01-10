#!/bin/bash

cmake --build debug-build --target HwInfo -j 12
./debug-build/HwInfo
