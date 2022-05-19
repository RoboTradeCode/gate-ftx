#!/bin/sh

#сборка шлюза
mkdir -p build/Release
cd build/Release || exit 1
cmake ../..
cmake --build .
