#!/bin/bash

cd build
rm CMakeCache.txt
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../install
make