#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>

#include "client/client_class.hpp"
#include "common/msgnode.hpp"
#include "log/log_manager.hpp"
#include "utils/field_op.hpp"
#include "utils/util_class.hpp"

using errcode = boost::system::error_code;

using namespace boost::asio;
using namespace std;
using namespace chatroom;
namespace beast = boost::beast;
namespace http = beast::http;




int main() {
    boost::asio::io_context ctx;
    client::GatewayHelper gate(ctx);

    string host, port;
    cout << "host: ";
    cin >> host;
    cout << "port: ";
    cin >> port;

    cout << "Remote host set, ";
    gate.SetRemoteAddress(host, port);
    string token, server_addr;
    uint64_t uid;
    while (true) {
        string inp;
        cout << "Input command>";
        cin >> inp;
        if (inp == "login") {
            string username, passcode;
            cout << "Username: ";
            cin >> username;
            cout << "Passcode: ";
            cin >> passcode;
            if (gate.Login(username, passcode, token, server_addr, uid)) {
                cout << "Login success\n";
                cout << "Token: " << token << "\n";
                cout << "Server address: " << server_addr << "\n";
                break;  // 进入下一步，向对应的后台服务器发起登陆请求
            } else {
                cout << "Login failed\n";
            }
        } else if (inp == "register") {
            string username, passcode;
            cout << "Username: ";
            cin >> username;
            cout << "Passcode: ";
            cin >> passcode;
            if (gate.Register(username, passcode)) {
                cout << "Register success\n";
            } else {
                cout << "Register failed\n";
            }
        } else if (inp == "exit") {
            cout << "bye\n";
            return 0;
        } else {
            cout << "Unknown command, available commands: login, register, exit\n";
        }
    }

    // try {
    auto sess = make_shared<client::AsyncClient>(ctx);
    assert(sess);
    // 指定远程端口
    spdlog::info("Connecting to remote server {}", server_addr);
    // 得到的server_addr是x.x.x.x:xxxx格式
    string server_host, server_port;
    size_t pos = server_addr.find(':');
    if (pos != string::npos) {
        server_host = server_addr.substr(0, pos);
        server_port = server_addr.substr(pos + 1);
    } else {
        spdlog::error("Invalid server address format: {}", server_addr);
        return -1;
    }

    ip::tcp::endpoint remote(ip::make_address(server_host), stoi(server_port));
    // 使用异步线程来接收消息，简单处理
    std::thread receiver([sess, remote]() {
        sess->Start(remote);
    });
    sess->Verify(uid, token);
    std::array<char, 255> inp;
    size_t end_pos;
    while (true) {
        cin.getline(inp.data(), inp.size());
        end_pos = strlen(inp.data());
        if (end_pos == 0) continue;
        if (!sess->Running()) {
            cout << "Connection closed\n";
            break;
        }
        if (inp[0] == '\\') {
            // command
            string command; vector<string> args;
            auto sep = find(inp.begin(), inp.begin() + end_pos, ' ');
            if (sep < inp.begin() + end_pos) {
                command = string(inp.begin() + 1, sep);
                // args = std::split(string(sep + 1, inp.end()), ' ');
                string arg_str(sep + 1, inp.begin() + end_pos);
                size_t start = 0, end;
                while ((end = arg_str.find(' ', start)) != string::npos) {
                    args.push_back(arg_str.substr(start, end - start));
                    start = end + 1;
                }
                if (start < arg_str.size()) {
                    args.push_back(arg_str.substr(start));
                }
            } else {
                command = string(inp.begin() + 1, inp.begin() + end_pos);
            }
            if (command == "send") {
                if (args.size() != 2) {
                    cout << "usage: send [whom by uid] [message]\n";
                } else {
                    uint64_t whom_uid;
                    try {
                        whom_uid = stoull(args[0]);
                    } catch (...) {
                        cout << "Invalid UID: " << args[0] << "\n";
                        continue;
                    }
                    string msg_data(args[1].size() + sizeof(uint64_t), 0);
                    WriteNetField64(msg_data.data(), whom_uid);
                    memcpy(msg_data.data() + sizeof(uint64_t), args[1].c_str(), args[1].size());
                    // 发送消息
                    sess->Send(msg_data.data(), msg_data.size(), CHAT_MSG);
                    cout << "Message sent to user with UID: " << whom_uid << "\n";
                }
            } else if (command == "exit") {
                cout << "bye\n";
                break;
            } else {
                cout << "Unknown command: " << command << "\n";
            }
        } else {
            sess->Send(inp.data(), end_pos, DEBUG);
        }
    }
    printf("Closing\n");
    sess->Close();
    sess->WorkerJoin();
    receiver.join();
    // } catch (std::exception& e) {
    //     cerr << "Exception: " << e.what() << endl;
    // }
}
