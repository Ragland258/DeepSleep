#pragma once
#include "Connection.h"
#include <boost/unordered_map.hpp>

// 连接管理类
class ConnectionMgr
{
public:
	// 获取单例
	static ConnectionMgr& GetInstance();
	// 添加连接
	void AddConnection(std::shared_ptr<Connection> con_ptr);
	// 移除连接
	void RmvConnection(std::string id);

private:
	ConnectionMgr() = default;
	ConnectionMgr(const ConnectionMgr&) = delete;
	ConnectionMgr& operator=(const ConnectionMgr&) = delete;
	// 连接映射表
	boost::unordered_map<std::string, std::shared_ptr<Connection>> _map_cons;
};
