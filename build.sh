# CMake可选项：
# DUSING_IOCONTEXT_POOL=ON
# DUSING_IOTHREAD_POOL=ON
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make