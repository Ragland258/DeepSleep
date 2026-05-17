#include "LogicNode.h"

LogicNode::LogicNode(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
	: _con_ptr(con_ptr), _data(data)
{

}
