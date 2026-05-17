
#include "ConnectionMgr.h"

ConnectionMgr& ConnectionMgr::GetInstance()
{
	// TODO: �ڴ˴����� return ���
	static ConnectionMgr instance;
	return instance;

}

void ConnectionMgr::AddConnection(std::shared_ptr<Connection> con_ptr)
{
	_map_cons[con_ptr->GetUid()] = con_ptr;
}

void ConnectionMgr::RmvConnection(std::string id)
{
	_map_cons.erase(id);
}
