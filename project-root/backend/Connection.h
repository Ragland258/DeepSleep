#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <queue>
#include <thread>
#include <mutex>
#include <boost/uuid/uuid_generators.hpp>
#include <json/value.h>
#include <json/reader.h>
#include <json/json.h>
#include "LogicNode.h"
using namespace boost;

// WebSocket连接类
class Connection : public std::enable_shared_from_this<Connection>
{
public:
	Connection(asio::io_context& ioc);
	// 获取连接唯一ID
	std::string GetUid()
	{
		return _uuid;
	}
	// 获取底层TCP socket
	asio::ip::tcp::socket& GetSocket()
	{
		return beast::get_lowest_layer(*_ws_ptr).socket();
	}

	// 接受WebSocket握手
	void AsyncAccpet();
	// 开始接收消息
	void Start();
	// 异步发送消息
	void AsyncSend(std::string msg);
private:
	// 执行实际写入操作
	void DoWrite();
	// 读取HTTP请求
	void ReadHttpHeader();
	// 处理HTTP请求并升级到WebSocket
	void OnHttpHeader(beast::error_code ec, std::size_t bytes_used);
	// 读取文件内容
	std::string ReadFile(const std::string& path);
	// 根据文件扩展名获取Content-Type
	std::string GetContentType(const std::string& path);
	
	// WebSocket流
	std::unique_ptr<beast::websocket::stream<beast::tcp_stream>> _ws_ptr;
	// 连接唯一ID
	std::string _uuid;
	// IO上下文引用
	asio::io_context& _ioc;
	// 接收缓冲区
	beast::flat_buffer _recv_buffer;
	// 发送队列
	std::queue<std::string> _send_queue;
	// 发送队列互斥锁
	std::mutex _send_mtx;
	// HTTP请求对象
	beast::http::request<beast::http::string_body> _http_request;
};
