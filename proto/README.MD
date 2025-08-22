# README

项目中用到的proto文件以及根据其生成的源文件、头文件。生成的过程实际上已经集成到了CMakefile里。

手动生成需要的命令：

```sh
protoc --cpp_out=./protocpp --grpc_out=./protocpp --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin status.proto
```