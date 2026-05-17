#pragma once

#include "Singleton.h"
#include <string>
#include <json/json.h>
#include <vector>
#include <memory>

// 用户数据结构
struct User {
    int id;
    std::string username;
    std::string password_hash;
    std::string created_at;
};

// 会话数据结构
struct Conversation {
    int id;
    int user_id;
    std::string title;
    std::string created_at;
    std::string updated_at;
};

// 消息数据结构
struct Message {
    int id;
    int conversation_id;
    std::string role;
    std::string content;
    std::string model_name;
    std::string references;
    std::string created_at;
};

// 数据库类：单例类，使用MySQL C API
class Database : public Singleton<Database>
{
    friend class Singleton<Database>;
public:
    ~Database();
    
    // 初始化数据库连接（自动创建数据库和表）
    bool Init(const std::string& host = "127.0.0.1", 
              const std::string& user = "root",
              const std::string& password = "root", 
              const std::string& dbname = "deepsleep",
              int port = 3306);
    
    // 检查连接是否有效
    bool IsConnected() const;
    
    // ========== 用户相关操作 ==========
    // 用户注册，返回用户ID（失败返回-1）
    int RegisterUser(const std::string& username, const std::string& password);
    // 用户登录，成功返回用户ID，失败返回-1
    int LoginUser(const std::string& username, const std::string& password);
    // 检查用户名是否存在
    bool UserExists(const std::string& username);
    
    // ========== 会话相关操作 ==========
    // 创建新会话，返回会话ID
    int CreateConversation(int user_id, const std::string& title = "New Chat");
    // 获取用户的所有会话
    std::vector<Conversation> GetUserConversations(int user_id);
    // 删除会话
    bool DeleteConversation(int conversation_id);
    // 更新会话标题
    bool UpdateConversationTitle(int conversation_id, const std::string& title);
    
    // ========== 消息相关操作 ==========
    // 添加消息
    bool AddMessage(int conversation_id, const std::string& role, const std::string& content, 
                    const std::string& model_name = "", const std::string& references = "");
    // 获取会话的所有消息
    std::vector<Message> GetConversationMessages(int conversation_id);
    // 获取最近N条消息
    std::vector<Message> GetRecentMessages(int conversation_id, int limit = 10);
    
    // ========== JSON接口（兼容原有代码） ==========
    Json::Value GetConversationListJson(int user_id);
    Json::Value GetConversationHistoryJson(int conversation_id);
    
    // ========== 系统提示词相关操作 ==========
    // 添加系统提示词，返回提示词ID
    int AddSystemPrompt(int user_id, const std::string& name, const std::string& content, bool is_default = false);
    // 更新系统提示词
    bool UpdateSystemPrompt(int prompt_id, const std::string& name, const std::string& content, bool is_default);
    // 删除系统提示词
    bool DeleteSystemPrompt(int prompt_id);
    // 获取用户的所有系统提示词
    Json::Value GetUserSystemPrompts(int user_id);
    // 获取默认系统提示词内容
    std::string GetDefaultSystemPrompt(int user_id);
    
    // ========== 模型配置相关操作 ==========
    // 添加模型提供商
    // 参数: user_id, name, type, api_key, base_url, model_name
    int AddModelProvider(int user_id, const std::string& name, const std::string& type,
                         const std::string& api_key, const std::string& base_url, 
                         const std::string& model_name = "default");
    // 获取用户的所有模型提供商
    Json::Value GetUserModelProviders(int user_id);
    // 更新模型提供商
    bool UpdateModelProvider(int provider_id, const std::string& name, const std::string& api_key,
                             const std::string& base_url, bool is_active);
    // 删除模型提供商
    bool DeleteModelProvider(int provider_id);
    // 添加提供商的模型
    int AddProviderModel(int provider_id, const std::string& model_name, const std::string& model_id);
    // 获取提供商的所有模型
    Json::Value GetProviderModels(int provider_id);
    // 删除提供商的模型
    bool DeleteProviderModel(int model_id);
    
private:
    Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    // MySQL连接句柄（使用void*避免头文件冲突）
    void* mysql_conn_;
    
    // 连接信息
    std::string host_;
    std::string user_;
    std::string password_;
    std::string dbname_;
    int port_;
    bool connected_;
    
    // 内部方法
    void Cleanup();
    bool ExecuteSQL(const std::string& sql);
    bool InitDatabase();  // 自动创建数据库和表
    std::string HashPassword(const std::string& password);
    std::string GetCurrentTimestamp();
};
