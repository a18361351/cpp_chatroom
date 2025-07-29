#include <iostream>

#include <boost/mysql.hpp>

using namespace std;
using namespace boost::asio;
namespace mysql = boost::mysql;

const string mysql_addr = "192.168.56.101";
const int mysql_port = 3306;

const string username = "lance";
const string password = "123456";
const string db_name = "chat";

int main() {
    // create connection
    io_context ctx;
    mysql::tcp_connection conn(ctx.get_executor());

    ip::tcp::endpoint ep(ip::make_address(mysql_addr), mysql_port);
    mysql::handshake_params(username, password, db_name);

    cout << "Connecting\n";
    conn.connect(ep, mysql::handshake_params(username, password, db_name));

    // Test for connection
    const string test_sql = "SELECT 'hello world!'";
    mysql::results res;
    conn.query(test_sql, res);

    cout << "Hello world test: " << res.rows().at(0).at(0) << endl;

    conn.close();
    return 0;
}