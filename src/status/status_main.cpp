#include "status/status_class.hpp"

// TODO(user): 为各个组件做一个读取config的功能

int main(int argc, char** argv) {
    // auto redis = make_shared<sw::redis::Redis>();
    
    chatroom::StatusServer srv(nullptr);    // FIXME: 现在暂时用不到redis所以可以这样传，未来进行修改
    srv.RunStatusServer("0.0.0.0:3000");
    return 0;
}