
#include "Server.h"
#include "Database.h"
#include "Logic_System.h"
#include <iostream>



int main()
{
	SetConsoleOutputCP(65001);
	SetConsoleCP(65001);

	// 提前初始化单例，避免多线程问题
	LogicSystem::GetInstance();

	// 初始化数据库（自动创建数据库和表）
	if (!Database::GetInstance()->Init("127.0.0.1", "root", "root", "deepsleep"))
	{
		std::cerr << "[Main] Database init failed" << std::endl;
		return 1;
	}

	std::cout << "[Main] Server starting on port 8081..." << std::endl;

	boost::asio::io_context ioc;
	Server server(ioc, 8081);
	server.Start();
	ioc.run();

	return 0;
}
