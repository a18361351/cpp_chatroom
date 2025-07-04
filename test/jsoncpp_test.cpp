#include <iostream>

#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/json.h>
#include <string>

using namespace std;

int main() {
    // Json -> str
    Json::Value root;
    Json::Value embed;
    embed["Embed"] = "http";
    embed["Embed2"] = "ws";
    Json::FastWriter fw;
    root["name"] = Json::Value("Jsoncpp_test");
    root["age"] = Json::Value(0);
    root["Embed"] = embed;
    string jsonstr = fw.write(root);
    cout << jsonstr << endl;

    // str -> Json
    Json::Reader rr;
    Json::Value readed;
    rr.parse(jsonstr, readed, false);
    cout << "root.name == " << readed["name"] << endl;
    return 0;
}