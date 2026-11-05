#!/bin/sh
# Builds and runs the pure-logic tests (blocklist, address bar). Works on any OS with a C++17
# compiler. The WebView2 and Win32 code is only built on Windows (see CMakeLists.txt).
set -e
cd "$(dirname "$0")/.."
mkdir -p build
${CXX:-c++} -std=c++17 -Wall -Wextra -Werror -Isrc tests/logic_test.cpp src/blocklist.cpp src/url.cpp -o build/logic_test
./build/logic_test
