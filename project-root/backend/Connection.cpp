#include "Connection.h"
#include <iostream>
#include <fstream>
#include "ConnectionMgr.h"
#include "Logic_System.h"
#include "LogicNode.h"

Connection::Connection(asio::io_context& ioc)
	: _ioc(ioc), _ws_ptr(std::make_unique<beast::websocket::stream<beast::tcp_stream>>(make_strand(ioc)))
{
	boost::uuids::random_generator generator;
	boost::uuids::uuid uuid(generator());
	_uuid = boost::uuids::to_string(uuid);
}

void Connection::AsyncAccpet()
{
	ReadHttpHeader();
}

void Connection::ReadHttpHeader()
{
	auto self = shared_from_this();
	
	_http_request = beast::http::request<beast::http::string_body>{};
	
	beast::http::async_read(
		_ws_ptr->next_layer(),
		_recv_buffer,
		_http_request,
		[this, self](beast::error_code ec, std::size_t bytes_used)
		{
			OnHttpHeader(ec, bytes_used);
		});
}

std::string Connection::ReadFile(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return "";
	}
	std::string content((std::istreambuf_iterator<char>(file)), 
	                   std::istreambuf_iterator<char>());
	return content;
}

// C++14 兼容的 ends_with 实现
bool EndsWith(const std::string& str, const std::string& suffix) {
	if (str.length() < suffix.length()) return false;
	return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string Connection::GetContentType(const std::string& path)
{
	if (EndsWith(path, ".html")) return "text/html";
	if (EndsWith(path, ".css")) return "text/css";
	if (EndsWith(path, ".js")) return "application/javascript";
	if (EndsWith(path, ".json")) return "application/json";
	if (EndsWith(path, ".png")) return "image/png";
	if (EndsWith(path, ".jpg") || EndsWith(path, ".jpeg")) return "image/jpeg";
	if (EndsWith(path, ".gif")) return "image/gif";
	return "text/plain";
}

void Connection::OnHttpHeader(beast::error_code ec, std::size_t bytes_used)
{
	if (ec)
	{
		std::cout << "HTTP read error: " << ec.message() << std::endl;
		return;
	}
	
	auto self = shared_from_this();
	
	std::string method(_http_request.method_string());
	std::string target(_http_request.target());
	std::string upgrade = std::string(_http_request[beast::http::field::upgrade]);
	std::string connection = std::string(_http_request[beast::http::field::connection]);
	
	std::cout << "HTTP request: " << method << " " << target << std::endl;
	
	// 检查是否是 WebSocket 升级请求
	bool is_websocket = false;
	if (method == "GET" && 
	    (upgrade.find("websocket") != std::string::npos || 
	     upgrade.find("WebSocket") != std::string::npos))
	{
		is_websocket = true;
	}
	
	if (is_websocket)
	{
		std::cout << "WebSocket upgrade request detected" << std::endl;
		
		// 执行 WebSocket 握手
		_ws_ptr->async_accept(_http_request,
			[this, self](beast::error_code ec)
			{
				if (ec)
				{
					std::cout << "WebSocket handshake failed: " << ec.message() << std::endl;
					return;
				}
				
				std::cout << "WebSocket handshake successful, uuid: " << _uuid << std::endl;
				
				_ws_ptr->set_option(beast::websocket::stream_base::timeout::suggested(
					beast::role_type::server));
				
				ConnectionMgr::GetInstance().AddConnection(self);
				Start();
			});
	}
	else
	{
		// 处理普通 HTTP 请求，提供前端页面
		std::cout << "Serving HTTP request: " << target << std::endl;
		
		std::string file_path = "c:\\Users\\17969\\source\\repos\\DeepSleep\\project-root\\frontend";
		
		// 默认返回 index.html
		if (target == "/")
		{
			file_path += "\\index.html";
		}
		else
		{
			// 替换 / 为 \\ 以适应 Windows 路径
			std::string normalized_target = target;
			for (size_t i = 0; i < normalized_target.size(); ++i)
			{
				if (normalized_target[i] == '/')
					normalized_target[i] = '\\';
			}
			file_path += normalized_target;
		}
		
		std::string content = ReadFile(file_path);
		
		if (content.empty())
		{
			std::cout << "File not found: " << file_path << std::endl;
			
			auto res = std::make_shared<beast::http::response<beast::http::string_body>>();
			res->version(_http_request.version());
			res->result(beast::http::status::not_found);
			res->set(beast::http::field::server, "DeepSleep Server");
			res->set(beast::http::field::content_type, "text/plain");
			res->body() = "404 Not Found";
			res->prepare_payload();
			
			beast::http::async_write(
				_ws_ptr->next_layer(),
				*res,
				[this, self, res](beast::error_code ec, std::size_t)
				{
					// 响应写完后关闭，不抛出异常
					boost::system::error_code ignored_ec;
					if (_ws_ptr && _ws_ptr->next_layer().socket().is_open())
					{
						_ws_ptr->next_layer().socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
						_ws_ptr->next_layer().socket().close(ignored_ec);
					}
				});
		}
		else
		{
			std::cout << "Serving file: " << file_path << std::endl;
			
			auto res = std::make_shared<beast::http::response<beast::http::string_body>>();
			res->version(_http_request.version());
			res->result(beast::http::status::ok);
			res->set(beast::http::field::server, "DeepSleep Server");
			res->set(beast::http::field::content_type, GetContentType(file_path));
			res->body() = content;
			res->prepare_payload();
			
			beast::http::async_write(
				_ws_ptr->next_layer(),
				*res,
				[this, self, res](beast::error_code ec, std::size_t)
				{
					// 响应写完后关闭，不抛出异常
					boost::system::error_code ignored_ec;
					if (_ws_ptr && _ws_ptr->next_layer().socket().is_open())
					{
						_ws_ptr->next_layer().socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
						_ws_ptr->next_layer().socket().close(ignored_ec);
					}
				});
		}
	}
}

void Connection::Start()
{
	auto self = shared_from_this();
	
	_ws_ptr->async_read(_recv_buffer,
		[this, self](beast::error_code ec, std::size_t bytes_transferred)
		{
			if (ec)
			{
				if (ec == beast::websocket::error::closed)
				{
					std::cout << "WebSocket connection closed, uuid: " << _uuid << std::endl;
				}
				else
				{
					std::cout << "WebSocket read error: " << ec.message() << std::endl;
				}
				ConnectionMgr::GetInstance().RmvConnection(_uuid);
				return;
			}
			
			std::string recv_data = beast::buffers_to_string(_recv_buffer.data());
			_recv_buffer.consume(bytes_transferred);
			
			std::cout << "Received from " << _uuid << ": " << recv_data << std::endl;
			
			Json::Value root;
			Json::Reader reader;
			if (reader.parse(recv_data, root))
			{
				if (root.isMember("id"))
				{
					auto node = std::make_shared<LogicNode>(self, root);
					LogicSystem::GetInstance()->PostMsgToQueue(node);
				}
				else
				{
					std::cout << "Missing 'id' field in message" << std::endl;
				}
			}
			else
			{
				std::cout << "JSON parse error: " << reader.getFormattedErrorMessages() << std::endl;
			}
			
			Start();
		});
}

void Connection::AsyncSend(std::string msg)
{
	{
		std::lock_guard<std::mutex> lock(_send_mtx);
		bool write_in_progress = !_send_queue.empty();
		_send_queue.push(msg);
		if (write_in_progress)
			return;
	}
	
	DoWrite();
}

void Connection::DoWrite()
{
	auto self = shared_from_this();
	
	_ws_ptr->text(true);
	_ws_ptr->async_write(
		asio::buffer(_send_queue.front()),
		[this, self](beast::error_code ec, std::size_t)
		{
			if (ec)
			{
				std::cout << "WebSocket write error: " << ec.message() << std::endl;
				return;
			}
			
			{
				std::lock_guard<std::mutex> lock(_send_mtx);
				_send_queue.pop();
				if (_send_queue.empty())
					return;
			}
			
			DoWrite();
		});
}
