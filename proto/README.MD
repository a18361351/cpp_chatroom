项目中用到的proto文件以及根据其生成的源文件、头文件

protoc --cpp_out=./protocpp --grpc_out=./protocpp --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin status.proto