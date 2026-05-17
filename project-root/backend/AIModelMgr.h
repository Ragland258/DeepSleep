#pragma once
#include "AIModel.h"
#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

// 模型管理器 - 管理所有模型实例
class AIModelManager
{
public:
    static AIModelManager& GetInstance();

    // 初始化模型（从配置文件）
    bool Initialize(const std::string& configFile);

    // 添加模型
    bool AddModel(const std::string& name, std::shared_ptr<AIModel> model);

    // 获取模型
    std::shared_ptr<AIModel> GetModel(const std::string& name);

    // 获取默认模型
    std::shared_ptr<AIModel> GetDefaultModel();

    // 设置默认模型
    void SetDefaultModel(const std::string& name);

    // 获取所有可用模型名称
    std::vector<std::string> GetModelNames() const;
    
    // 从数据库加载用户配置的模型
    void LoadModelsFromDatabase(int user_id);
    
    // 获取当前默认模型名称
    std::string GetDefaultModelName() const;

private:
    AIModelManager() = default;
    ~AIModelManager() = default;
    AIModelManager(const AIModelManager&) = delete;
    AIModelManager& operator=(const AIModelManager&) = delete;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<AIModel>> models_;
    std::string default_model_name_;
};
