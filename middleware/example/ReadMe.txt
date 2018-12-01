How to build Example

1. ToolChain Setup

TOOLCHAIN_PATH=$(dirname `which aarch64-linux-gnu-gcc`)

export CROSS_COMPILE=$TOOLCHAIN_PATH/aarch64-linux-gnu-


2. Install include/library path setup

Change CMakeLists.txt file line 8, 9 to set the correct install path.

3. mkdir build && cd build && cmake ../ &&B make

