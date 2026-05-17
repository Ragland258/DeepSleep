#include "AIModelMgr.h"
#include "OpenAIModel.h"
#include "Database.h"
#include <iostream>

AIModelManager& AIModelManager::GetInstance()
{
    static AIModelManager instance;
    return instance;
}

bool AIModelManager::Initialize(const std::string& configFile)
{
    // 可以在这里从配置文件加载模型
    // 目前留空，使用从数据库加载的方式
    return true;
}

bool AIModelManager::AddModel(const std::string& name, std::shared_ptr<AIModel> model)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = models_.find(name);
    if (it == models_.end())
    {
        models_[name] = model;
        std::cout << "[AIModelManager] Model added: " << name << std::endl;
        return true;
    }
    std::cout << "[AIModelManager] Model already exists: " << name << std::endl;
    return false;
}

std::shared_ptr<AIModel> AIModelManager::GetModel(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = models_.find(name);
    if (it != models_.end())
    {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<AIModel> AIModelManager::GetDefaultModel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!default_model_name_.empty())
    {
        auto it = models_.find(default_model_name_);
        if (it != models_.end())
        {
            return it->second;
        }
    }
    // 如果没有默认模型，返回第一个可用的
    if (!models_.empty())
    {
        return models_.begin()->second;
    }
    return nullptr;
}

void AIModelManager::SetDefaultModel(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    default_model_name_ = name;
    std::cout << "[AIModelManager] Default model set to: " << name << std::endl;
}

std::vector<std::string> AIModelManager::GetModelNames() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& pair : models_)
    {
        names.push_back(pair.first);
    }
    return names;
}

void AIModelManager::LoadModelsFromDatabase(int user_id)
{
    std::cout << "[AIModelManager] Loading models for user: " << user_id << std::endl;
    
    // 从数据库获取用户的模型配置
    Json::Value providers = Database::GetInstance()->GetUserModelProviders(user_id);
    
    if (!providers.isMember("providers") || !providers["providers"].isArray())
    {
        std::cout << "[AIModelManager] No providers found for user " << user_id << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    models_.clear(); // 清除旧模型
    
    for (const auto& provider : providers["providers"])
    {
        if (!provider.isMember("name") || !provider.isMember("base_url"))
        {
            continue;
        }
        
        std::string name = provider["name"].asString();
        std::string base_url = provider["base_url"].asString();
        std::string api_key = provider.isMember("api_key") ? provider["api_key"].asString() : "";
        
        // 从数据库获取模型名称
        std::string model_name = provider.isMember("model_name") ? provider["model_name"].asString() : "default";
        
        // 如果没有设置模型名称，根据提供商类型设置默认模型名
        if (model_name == "default" || model_name.empty())
        {
            std::string type = provider.isMember("type") ? provider["type"].asString() : "";
            if (type == "deepseek")
            {
                model_name = "deepseek-chat";
            }
            else if (type == "doubao")
            {
                model_name = "doubao-pro";
            }
            else if (type == "kimi")
            {
                model_name = "moonshot-v1-8k";
            }
            else if (type == "lmstudio")
            {
                model_name = "local-model";
            }
        }
        
        // 创建OpenAI兼容模型
        auto model = std::make_shared<OpenAIModel>(api_key, base_url, model_name);
        models_[name] = model;
        
        std::cout << "[AIModelManager] Loaded model: " << name 
                  << " (" << base_url << ")" << std::endl;
        
        // 第一个模型设为默认
        if (default_model_name_.empty())
        {
            default_model_name_ = name;
        }
    }
    
    std::cout << "[AIModelManager] Loaded " << models_.size() << " models" << std::endl;
}

std::string AIModelManager::GetDefaultModelName() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return default_model_name_;
}
