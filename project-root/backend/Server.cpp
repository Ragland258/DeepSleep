#include "Server.h"
#include <iostream>

Server::Server(boost::asio::io_context& ioc, unsigned short port)
	: _ioc(ioc), _acceptor(ioc, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
{
	std::cout << "server start." << std::endl;
}

void Server::Start()
{
	StartAccept();
}

void Server::StartAccept()
{
	auto con_ptr = std::make_shared<Connection>(_ioc);
	_acceptor.async_accept(con_ptr->GetSocket(),
		[this, con_ptr](system::error_code ec)
		{
			try
			{
				if (!ec)
					con_ptr->AsyncAccpet();
				else
				{

				}
				StartAccept();
			}
			catch (std::exception& exp)
			{

			}
		});
}
