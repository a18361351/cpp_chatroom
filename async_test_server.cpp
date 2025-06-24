#include <unordered_map>
#include <iostream>
using namespace std;

#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "session.hpp"
#include "server_class.hpp"

using namespace boost::asio;
using errcode = boost::system::error_code;

int main() {
    io_context context;
    Server s1(context);

    ip::tcp::endpoint ep(ip::tcp::v4(), 1234);

    s1.Listen(ep);
    s1.StartAccept();

    context.run();
}