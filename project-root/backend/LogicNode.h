#pragma once
#include <memory>
#include <json/json.h>
#include <json/value.h>

// 前向声明
class Connection;

// 逻辑节点：封装连接和数据
class LogicNode : public std::enable_shared_from_this<LogicNode>
{
	friend class LogicSystem;
public:
	LogicNode(std::shared_ptr<Connection> con_ptr, const Json::Value& data);

private:
	// 连接指针
	std::shared_ptr<Connection> _con_ptr;
	// JSON数据
	Json::Value _data;
};
