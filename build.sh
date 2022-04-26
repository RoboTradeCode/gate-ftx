#!/bin/sh

#сборка шлюза
mkdir -p build/Debug
cd build/Debug || exit 1
cmake ../..
cmake --build .
