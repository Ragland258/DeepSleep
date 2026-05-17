#pragma once
#include "Singleton.h"
#include <queue>
#include <thread>
#include <map>
#include <functional>
#include <memory>
#include <string>
#include "const.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "LogicNode.h"
#include "SearchTool.h"

// 前向声明
class Connection;
class LogicNode;

using std::function;
using std::string;

// 回调函数类型定义
typedef function<void(std::shared_ptr<Connection> conn, const Json::Value& data)> FunCallBcak;

// 逻辑系统：单例类
class LogicSystem : public Singleton<LogicSystem>
{
    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    // 将消息投递到逻辑队列
    void PostMsgToQueue(std::shared_ptr<LogicNode> msg);
private:
    LogicSystem();
    // 注册所有回调函数
    void RegisterCallBack();
    
    // 聊天请求回调
    void OnChatRequest(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 登录请求回调
    void OnLoginRequest(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 注册请求回调
    void OnRegisterRequest(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 获取会话列表回调
    void OnGetConversationList(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 创建会话回调
    void OnCreateConversation(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 获取历史消息回调
    void OnGetHistoryRequest(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 获取模型提供商列表回调
    void OnGetProviders(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 添加模型提供商回调
    void OnAddProvider(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 删除模型提供商回调
    void OnDeleteProvider(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 设置默认模型回调
    void OnSetDefaultModel(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 获取可用模型列表回调
    void OnGetModels(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 获取系统提示词列表回调
    void OnGetPrompts(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 添加系统提示词回调
    void OnAddPrompt(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 更新系统提示词回调
    void OnUpdatePrompt(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 删除系统提示词回调
    void OnDeletePrompt(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 设置默认提示词回调
    void OnSetDefaultPrompt(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
    // 删除会话回调
	void OnDeleteConversation(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
	// 测试模型连接回调
	void OnTestModel(std::shared_ptr<Connection> con_ptr, const Json::Value& data);
	
	// 工作线程处理消息
	void DealMsg();
private:
    // 消息队列
    std::queue<std::shared_ptr<LogicNode>> _msg_queue;
    // 互斥锁
    std::mutex _mutex;
    // 条件变量
    std::condition_variable _consume;
    // 工作线程
    std::thread _worker_thread;
    // 停止标志
    bool need_stop;
    // 消息ID到回调函数的映射
    std::map<short, FunCallBcak> _fun_callback;
    // 搜索工具
    SearchTool _search_tool;
};
