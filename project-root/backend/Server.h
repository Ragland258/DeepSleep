#pragma once
#include "ConnectionMgr.h"
using namespace boost;

// 服务器类
class Server
{
	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;
public:
	Server(boost::asio::io_context& ioc, unsigned short port);
	// 开始接受连接
	void StartAccept();
	// 启动服务器
	void Start();
private:
	// TCP接受器
	asio::ip::tcp::acceptor _acceptor;
	// IO上下文引用
	asio::io_context& _ioc;
};
