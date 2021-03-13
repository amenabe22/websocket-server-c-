#include "WebsocketServer.h"
#include <iostream>
#include <thread>

#include <asio/io_service.hpp>

// the port number the websocket server listens on
#define PORT_NUMBER 8080

int main(int argc, char *argv[])
{
    asio::io_service mainEventLoop;
    WbSockServ server;
    // handle connect
    server.connect([&mainEventLoop, &server](ClientConnection conn) {
        mainEventLoop.post([conn, &server]() {
            std::clog << "Amen's server started the connection for you... Blessing from the lord !!!!" << std::endl;
            std::clog << "AYEE we got like " << server.numConnections() << " People up in this bitch !!!" << std::endl;
            server.sendMessage(conn, "Hey Nigga", Json::Value());
        });
    });
    // handle disconnect
    server.disconnect([&mainEventLoop, &server](ClientConnection conn) {
        mainEventLoop.post([conn, &server]() {
            std::clog << "Connection is closed " << std::endl;
            std::clog << "There are now " << server.numConnections() << " open connections " << std::endl;
        });
    });
    server.message("message", [&mainEventLoop, &server](ClientConnection conn, const Json::Value &args) {
        mainEventLoop.post([conn, args, &server]() {
            std::clog << "Message handler on the main thread" << std::endl;
            std::clog << "Message payload:" << std::endl;
            for (auto key : args.getMemberNames())
            {
                std::clog << "\t" << key << ":" << args[key].asString() << std::endl;
            }
            server.sendMessage(conn, "message", args);
        });
    });
    // Start the networking thread
    std::thread serverThread([&server]() {
        server.run(PORT_NUMBER);
    });

    // start a keyboard input thread that reads from stdin
    std::thread inputThread([&server, &mainEventLoop]() {
        string input;
        while (1)
        {
            // read user input from stdin
            std::getline(std::cin, input);
            // Broadcast the input to all connected clients (is sennt on the network thread)
            Json::Value payload;
            payload["input"] = input;
            server.broadcastMessage("userInput", payload);
            mainEventLoop.post([]() {
                std::clog << "User input debug output on the main thread" << std::endl;
            });
        }
    });
    // start the even loop for the main thread
    asio::io_service::work work(mainEventLoop);
    mainEventLoop.run();
    return 0;
}