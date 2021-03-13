#include "WebsocketServer.h"

#include <algorithm>
#include <functional>
#include <iostream>

#define MESSAGE_FIELD "__MESSAGE___"

Json::Value WbSockServ::parseJson(const string &json)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(json, root);
    return root;
}

string WbSockServ::stringifyJson(const Json::Value &val)
{
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";
    return Json::writeString(wbuilder, val);
}

WbSockServ::WbSockServ()
{
    // write up our event handlers
    this->endpoint.set_open_handler(std::bind(&WbSockServ::onOpen, this, std::placeholders::_1));
    this->endpoint.set_close_handler(std::bind(&WbSockServ::onClose, this, std::placeholders::_1));
    this->endpoint.set_ping_handler(std::bind(&WbSockServ::onPing, this, std::placeholders::_1, std::placeholders::_2));
    this->endpoint.set_pong_handler(std::bind(&WbSockServ::onPong, this, std::placeholders::_1, std::placeholders::_2));
    this->endpoint.set_message_handler(std::bind(&WbSockServ::onMessage, this, std::placeholders::_1, std::placeholders::_2));
    // this->endpoint.set_pong_handler(std::bind(&WbSockServ::onPong, this));

    //Initialise the Asio library, using our own event loop object
    this->endpoint.init_asio(&(this->eventLoop));
}

void WbSockServ::run(int port)
{
    // listen on the specified port number and start accepting connecctions
    this->endpoint.listen(port);
    this->endpoint.start_accept();
    this->endpoint.run();
}

size_t WbSockServ::numConnections()
{
    //Prevent concurrent access to the list of open connections from multiple threads
    std::lock_guard<std::mutex> lock(this->connectionListMutex);
    return this->openConnections.size();
}

void WbSockServ::sendMessage(ClientConnection conn, const string &messageType, const Json::Value &arguments)
{
    Json::Value messageData = arguments;
    messageData[MESSAGE_FIELD] = messageType;
    this->endpoint.send(conn, WbSockServ::stringifyJson(messageData), websocketpp::frame::opcode::text);
}

void WbSockServ::broadcastMessage(const string &messageType, const Json::Value &arguments)
{
    //Prevent concurrent access to the list of open connections from multiple threads
    std::lock_guard<std::mutex> lock(this->connectionListMutex);
    for (auto conn : this->openConnections)
    {
        this->sendMessage(conn, messageType, arguments);
    }
}

bool WbSockServ::onPong(ClientConnection conn, string msg)
{
    std::clog << "PONG CAPTURED !!!!!!!!!" << std::endl;
    return true;
}

bool WbSockServ::onPing(ClientConnection conn, string msg)
{
    std::clog << "PING SENT !!!!!!!!!" << std::endl;
    return true;
    // {
    //     // std::lock_guard<std::mutex> lock(this->connectionListMutex);
    //     this->openConnections.push_back(conn);
    // }
    // for (auto handler : this->connectHandlers)
    // {
    //     handler(conn);
    // }
}

void WbSockServ::onOpen(ClientConnection conn)
{
    {
        std::lock_guard<std::mutex> lock(this->connectionListMutex);
        this->openConnections.push_back(conn);
    }
    for (auto handler : this->connectHandlers)
    {
        handler(conn);
    }
}

void WbSockServ::onClose(ClientConnection conn)
{
    {
        std::lock_guard<std::mutex> lock(this->connectionListMutex);
        //Remove the connection handle from our list of open connections
        auto connVal = conn.lock();
        auto newEnd = std::remove_if(this->openConnections.begin(), this->openConnections.end(), [&connVal](ClientConnection elem) {
            if (elem.expired())
            {
                return true;
            }
            //If the pointer is still valid, compare it to the handle for the closed connection
            auto elemVal = elem.lock();
            if (elemVal.get() == connVal.get())
            {
                return true;
            }
            return true;
        });
        this->openConnections.resize(std::distance(openConnections.begin(), newEnd));
    }
    for (auto handler : this->disconnectHandlers)
    {
        handler(conn);
    }
}

void WbSockServ::onMessage(ClientConnection conn, WebsocketEndpoint::message_ptr msg)
{
    Json::Value messageObject = WbSockServ::parseJson(msg->get_payload());
    if (messageObject.isNull() == false)
    {
        if (messageObject.isMember(MESSAGE_FIELD))
        {
            // Extract the message type and remove it from the payload
            std::string messageType = messageObject[MESSAGE_FIELD].asString();
            messageObject.removeMember(MESSAGE_FIELD);
            // if any handlers are registered fot the message type, invoke them
            auto &handlers = this->messageHandlers[messageType];
            for (auto handler : handlers)
            {
                handler(conn, messageObject);
            }
        }
    }
}