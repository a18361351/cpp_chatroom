# CMake可选项：
# DUSE_IOCONTEXT_POOL=ON
# DUSE_IOTHREAD_POOL=ON
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)