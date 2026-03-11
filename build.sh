#!/bin/bash

mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DFETCHCONTENT_QUIET=Off --verbose
cmake --build .
