#include "Logic_System.h"
#include "Connection.h"
#include "Database.h"
#include "AIModelMgr.h"
#include "AIModel.h"
#include <iostream>

LogicSystem::LogicSystem() 
    : need_stop(false)
{
    RegisterCallBack();
    _worker_thread = std::thread(&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem()
{
    need_stop = true;
    _consume.notify_all();
    if (_worker_thread.joinable())
    {
        _worker_thread.join();
    }
}

void LogicSystem::RegisterCallBack()
{
    _fun_callback[MSG_CHAT_REQUEST] = std::bind(&LogicSystem::OnChatRequest, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_LOGIN_REQUEST] = std::bind(&LogicSystem::OnLoginRequest, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_REGISTER_REQUEST] = std::bind(&LogicSystem::OnRegisterRequest, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_CONVERSATION_LIST] = std::bind(&LogicSystem::OnGetConversationList, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_CREATE_CONVERSATION] = std::bind(&LogicSystem::OnCreateConversation, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_HISTORY_REQUEST] = std::bind(&LogicSystem::OnGetHistoryRequest, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_GET_PROVIDERS] = std::bind(&LogicSystem::OnGetProviders, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_ADD_PROVIDER] = std::bind(&LogicSystem::OnAddProvider, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_DELETE_PROVIDER] = std::bind(&LogicSystem::OnDeleteProvider, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_SET_DEFAULT_MODEL] = std::bind(&LogicSystem::OnSetDefaultModel, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_GET_MODELS] = std::bind(&LogicSystem::OnGetModels, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_GET_PROMPTS] = std::bind(&LogicSystem::OnGetPrompts, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_ADD_PROMPT] = std::bind(&LogicSystem::OnAddPrompt, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_UPDATE_PROMPT] = std::bind(&LogicSystem::OnUpdatePrompt, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_DELETE_PROMPT] = std::bind(&LogicSystem::OnDeletePrompt, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_SET_DEFAULT_PROMPT] = std::bind(&LogicSystem::OnSetDefaultPrompt, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_DELETE_CONVERSATION] = std::bind(&LogicSystem::OnDeleteConversation, this, std::placeholders::_1, std::placeholders::_2);
    _fun_callback[MSG_TEST_MODEL] = std::bind(&LogicSystem::OnTestModel, this, std::placeholders::_1, std::placeholders::_2);
}

void LogicSystem::PostMsgToQueue(std::shared_ptr<LogicNode> msg)
{
    std::unique_lock<std::mutex> lock(_mutex);
    _msg_queue.push(msg);
    _consume.notify_one();
}

void LogicSystem::DealMsg()
{
    while (true)
    {
        std::shared_ptr<LogicNode> msg;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _consume.wait(lock, [this] { 
                return !_msg_queue.empty() || need_stop; 
            });

            if (need_stop && _msg_queue.empty())
            {
                break;
            }

            msg = _msg_queue.front();
            _msg_queue.pop();
        }

        if (msg)
        {
            short id = msg->_data["id"].asInt();
            std::cout << "处理消息 ID: " << id << std::endl;
            auto it = _fun_callback.find(id);
            if (it != _fun_callback.end())
            {
                it->second(msg->_con_ptr, msg->_data);
            }
        }
    }
}

void LogicSystem::OnChatRequest(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    std::string user_msg = data["data"]["message"].asString();
    int conversation_id = data["data"]["conversation_id"].asInt();
    int user_id = data["data"]["user_id"].asInt();
    std::string provider_name = data["data"].isMember("provider_name") ? data["data"]["provider_name"].asString() : "";
    bool enable_search = data["data"].isMember("enable_search") ? data["data"]["enable_search"].asBool() : false;

    std::cout << "收到聊天消息：用户 ID = " << user_id
              << ", 会话 ID = " << conversation_id
              << ", 提供商 = " << provider_name
              << ", 联网搜索 = " << (enable_search ? "开启" : "关闭")
              << ", 内容 = " << user_msg << std::endl;

    Database::GetInstance()->AddMessage(conversation_id, "user", user_msg);

    AIModelManager::GetInstance().LoadModelsFromDatabase(user_id);

    std::shared_ptr<AIModel> model;
    if (!provider_name.empty()) {
        model = AIModelManager::GetInstance().GetModel(provider_name);
        std::cout << "[LogicSystem] 使用指定模型: " << provider_name << std::endl;
    }
    if (!model) {
        model = AIModelManager::GetInstance().GetDefaultModel();
        std::cout << "[LogicSystem] 使用默认模型" << std::endl;
    }

    if (model) {
        Json::Value start_response;
        start_response["id"] = MSG_CHAT_STREAM_START;
        start_response["data"]["conversation_id"] = conversation_id;
        start_response["data"]["model_name"] = model->GetName();
        con_ptr->AsyncSend(start_response.toStyledString());

        std::vector<ChatMessage> messages;

        std::string system_prompt = Database::GetInstance()->GetDefaultSystemPrompt(user_id);
        std::string search_references;

        if (enable_search) {
            std::cout << "[LogicSystem] 执行联网搜索..." << std::endl;
            auto search_results = _search_tool.Search(user_msg, 5);

            if (!search_results.empty()) {
                search_references = _search_tool.BuildSearchContext(search_results);
                std::cout << "[LogicSystem] 搜索到 " << search_results.size() << " 条结果" << std::endl;

                // 构建强制使用搜索结果的系统提示词
                std::string search_prompt = "【系统指令】你是一个基于搜索结果回答问题的AI助手。\n\n";
                search_prompt += search_references;
                search_prompt += "\n\n【强制要求】\n";
                search_prompt += "1. 你必须基于上述搜索结果回答用户问题\n";
                search_prompt += "2. 禁止说你无法访问互联网、无法获取实时数据或你的知识截止于某时间\n";
                search_prompt += "3. 回答开头必须说明\"根据搜索结果...\"\n";
                search_prompt += "4. 使用 [1][2] 等序号标注信息来源\n";
                search_prompt += "5. 如果搜索结果信息不足，基于已有信息回答，不要编造\n";
                
                messages.push_back({"system", search_prompt});
            } else {
                std::cout << "[LogicSystem] 搜索无结果，使用默认提示词" << std::endl;
                if (system_prompt.empty()) {
                    system_prompt = "你是一个有用的AI助手。请用中文回答用户的问题。";
                }
                messages.push_back({"system", system_prompt});
            }
        } else {
            if (system_prompt.empty()) {
                system_prompt = "你是一个有用的AI助手。请用中文回答用户的问题。";
            }
            messages.push_back({"system", system_prompt});
        }

        // 动态上下文限制 - 根据消息总长度调整保留数量
        auto history = Database::GetInstance()->GetConversationMessages(conversation_id);
        
        // 计算消息总长度（粗略估算token）
        size_t totalLength = 0;
        for (const auto& msg : history) {
            totalLength += msg.content.length();
        }
        
        // 动态调整保留的消息数
        int maxMessages = 10;
        if (totalLength > 5000) {
            maxMessages = 8;
            std::cout << "[LogicSystem] Context length: " << totalLength << ", reducing to " << maxMessages << " messages" << std::endl;
        }
        if (totalLength > 8000) {
            maxMessages = 6;
            std::cout << "[LogicSystem] Context length: " << totalLength << ", reducing to " << maxMessages << " messages" << std::endl;
        }
        if (totalLength > 12000) {
            maxMessages = 4;
            std::cout << "[LogicSystem] Context length: " << totalLength << ", reducing to " << maxMessages << " messages" << std::endl;
        }
        if (totalLength > 20000) {
            maxMessages = 2;
            std::cout << "[LogicSystem] Context length: " << totalLength << ", reducing to " << maxMessages << " messages" << std::endl;
        }
        
        // 只保留最近的消息
        if (history.size() > static_cast<size_t>(maxMessages)) {
            history.erase(history.begin(), history.end() - maxMessages);
        }
        
        for (const auto& msg : history) {
            messages.push_back({msg.role, msg.content});
        }

        messages.push_back({"user", user_msg});

        std::cout << "[LogicSystem] Using model: " << model->GetName()
                  << ", Context messages: " << messages.size() << std::endl;

        std::string model_name_used = model->GetName();

        std::string full_reply;
        try {
            model->GenerateStream(messages, [&con_ptr, &full_reply, conversation_id](const std::string& chunk) {
                if (!chunk.empty()) {
                    full_reply += chunk;

                    Json::Value stream_response;
                    stream_response["id"] = MSG_CHAT_STREAM_DATA;
                    stream_response["data"]["chunk"] = chunk;
                    stream_response["data"]["conversation_id"] = conversation_id;
                    con_ptr->AsyncSend(stream_response.toStyledString());
                }
            });
        } catch (const std::exception& e) {
            std::cerr << "[LogicSystem] Model error: " << e.what() << std::endl;
            full_reply = "抱歉，调用AI模型时出错：" + std::string(e.what());

            Json::Value error_response;
            error_response["id"] = MSG_CHAT_STREAM_DATA;
            error_response["data"]["chunk"] = full_reply;
            error_response["data"]["conversation_id"] = conversation_id;
            con_ptr->AsyncSend(error_response.toStyledString());
        }

        if (!full_reply.empty()) {
            Database::GetInstance()->AddMessage(conversation_id, "assistant", full_reply, model_name_used, search_references);
        }

        Json::Value end_response;
        end_response["id"] = MSG_CHAT_STREAM_END;
        end_response["data"]["conversation_id"] = conversation_id;
        end_response["data"]["full_message"] = full_reply;
        if (!search_references.empty()) {
            end_response["data"]["has_references"] = true;
            auto last_results = _search_tool.GetLastResults();
            Json::Value refs_json = Json::arrayValue;
            for (const auto& r : last_results) {
                Json::Value ref;
                ref["title"] = r.title;
                ref["snippet"] = r.snippet;
                ref["url"] = r.url;
                refs_json.append(ref);
            }
            end_response["data"]["references_json"] = refs_json;
        }
        con_ptr->AsyncSend(end_response.toStyledString());
    } else {
        std::string error_msg = "请先配置AI模型。请在模型配置页面添加模型提供商（如LM Studio、DeepSeek等）。";

        Json::Value stream_response;
        stream_response["id"] = MSG_CHAT_STREAM_DATA;
        stream_response["data"]["chunk"] = error_msg;
        stream_response["data"]["conversation_id"] = conversation_id;
        con_ptr->AsyncSend(stream_response.toStyledString());

        Json::Value end_response;
        end_response["id"] = MSG_CHAT_STREAM_END;
        end_response["data"]["conversation_id"] = conversation_id;
        end_response["data"]["full_message"] = error_msg;
        con_ptr->AsyncSend(end_response.toStyledString());
    }
}

void LogicSystem::OnLoginRequest(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    std::string username = data["data"]["username"].asString();
    std::string password = data["data"]["password"].asString();

    std::cout << "登录请求：用户名 = " << username << std::endl;

    Json::Value response;
    response["id"] = MSG_LOGIN_RESPONSE;
    
    int user_id = Database::GetInstance()->LoginUser(username, password);
    if (user_id > 0)
    {
        // 登录成功后加载用户的模型配置
        AIModelManager::GetInstance().LoadModelsFromDatabase(user_id);
        
        response["data"]["success"] = true;
        response["data"]["user_id"] = user_id;
        response["data"]["message"] = "登录成功";
    }
    else
    {
        response["data"]["success"] = false;
        response["data"]["message"] = "用户名或密码错误";
    }

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnRegisterRequest(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    std::string username = data["data"]["username"].asString();
    std::string password = data["data"]["password"].asString();

    std::cout << "注册请求：用户名 = " << username << std::endl;

    Json::Value response;
    response["id"] = MSG_REGISTER_RESPONSE;
    
    int user_id = Database::GetInstance()->RegisterUser(username, password);
    if (user_id > 0)
    {
        response["data"]["success"] = true;
        response["data"]["user_id"] = user_id;
        response["data"]["message"] = "注册成功";
    }
    else
    {
        response["data"]["success"] = false;
        response["data"]["message"] = "用户名已存在";
    }

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnGetConversationList(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int user_id = data["data"]["user_id"].asInt();

    std::cout << "获取会话列表：用户 ID = " << user_id << std::endl;

    Json::Value response;
    response["id"] = MSG_CONVERSATION_LIST;
    
    Json::Value result = Database::GetInstance()->GetConversationListJson(user_id);
    response["data"] = result["conversations"];
    
    std::cout << "返回会话数量: " << result["conversations"].size() << std::endl;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnCreateConversation(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int user_id = data["data"]["user_id"].asInt();
    std::string title = data["data"]["title"].asString();

    std::cout << "创建会话：用户 ID = " << user_id << ", 标题 = " << title << std::endl;

    Json::Value response;
    response["id"] = MSG_CREATE_CONVERSATION;
    
    int conv_id = Database::GetInstance()->CreateConversation(user_id, title);
    response["data"]["success"] = (conv_id > 0);
    response["data"]["conversation_id"] = conv_id;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnGetHistoryRequest(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int conversation_id = data["data"]["conversation_id"].asInt();

    std::cout << "获取历史消息：会话 ID = " << conversation_id << std::endl;

    Json::Value response;
    response["id"] = MSG_HISTORY_REQUEST;

    Json::Value result = Database::GetInstance()->GetConversationHistoryJson(conversation_id);
    response["data"] = result["messages"];

    std::cout << "返回历史消息数量: " << result["messages"].size() << std::endl;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnGetProviders(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int user_id = data["data"]["user_id"].asInt();

    std::cout << "获取模型提供商列表：用户 ID = " << user_id << std::endl;

    Json::Value response;
    response["id"] = MSG_PROVIDERS_RESPONSE;

    Json::Value result = Database::GetInstance()->GetUserModelProviders(user_id);
    response["data"] = result["providers"];

    std::cout << "返回提供商数量: " << result["providers"].size() << std::endl;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnAddProvider(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int user_id = data["data"]["user_id"].asInt();
    std::string name = data["data"]["name"].asString();
    std::string type = data["data"]["type"].asString();
    std::string api_key = data["data"]["api_key"].asString();
    std::string base_url = data["data"]["base_url"].asString();
    std::string model_name = data["data"].isMember("model_name") ? data["data"]["model_name"].asString() : "default";

    std::cout << "添加模型提供商：用户 ID = " << user_id << ", 名称 = " << name << ", 模型 = " << model_name << std::endl;

    int provider_id = Database::GetInstance()->AddModelProvider(user_id, name, type, api_key, base_url, model_name);

    Json::Value response;
    response["id"] = MSG_ADD_PROVIDER;
    response["data"]["success"] = (provider_id > 0);
    response["data"]["provider_id"] = provider_id;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnDeleteProvider(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int provider_id = data["data"]["provider_id"].asInt();

    std::cout << "删除模型提供商：ID = " << provider_id << std::endl;

    bool success = Database::GetInstance()->DeleteModelProvider(provider_id);

    Json::Value response;
    response["id"] = MSG_DELETE_PROVIDER;
    response["data"]["success"] = success;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnDeleteConversation(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int conversation_id = data["data"]["conversation_id"].asInt();

    std::cout << "删除会话：ID = " << conversation_id << std::endl;

    bool success = Database::GetInstance()->DeleteConversation(conversation_id);

    Json::Value response;
    response["id"] = MSG_DELETE_CONVERSATION;
    response["data"]["success"] = success;
    response["data"]["conversation_id"] = conversation_id;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnSetDefaultModel(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    std::string model_name = data["data"]["model_name"].asString();
    
    std::cout << "设置默认模型：" << model_name << std::endl;
    
    AIModelManager::GetInstance().SetDefaultModel(model_name);
    
    Json::Value response;
    response["id"] = MSG_SET_DEFAULT_MODEL;
    response["data"]["success"] = true;
    response["data"]["model_name"] = model_name;
    
    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnGetModels(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int user_id = data["data"]["user_id"].asInt();
    
    std::cout << "获取可用模型列表：用户 ID = " << user_id << std::endl;
    
    // 加载用户的模型
    AIModelManager::GetInstance().LoadModelsFromDatabase(user_id);
    
    Json::Value response;
    response["id"] = MSG_MODELS_RESPONSE;
    
    auto model_names = AIModelManager::GetInstance().GetModelNames();
    Json::Value models(Json::arrayValue);
    for (const auto& name : model_names) {
        Json::Value model;
        model["name"] = name;
        model["is_default"] = (name == AIModelManager::GetInstance().GetDefaultModelName());
        models.append(model);
    }
    
    response["data"]["models"] = models;
    response["data"]["default_model"] = AIModelManager::GetInstance().GetDefaultModelName();
    
    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnGetPrompts(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int user_id = data["data"]["user_id"].asInt();

    std::cout << "获取系统提示词列表：用户 ID = " << user_id << std::endl;

    Json::Value response;
    response["id"] = MSG_PROMPTS_RESPONSE;

    Json::Value result = Database::GetInstance()->GetUserSystemPrompts(user_id);
    response["data"] = result["prompts"];

    std::cout << "返回提示词数量: " << result["prompts"].size() << std::endl;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnAddPrompt(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int user_id = data["data"]["user_id"].asInt();
    std::string name = data["data"]["name"].asString();
    std::string content = data["data"]["content"].asString();
    bool is_default = data["data"].isMember("is_default") ? data["data"]["is_default"].asBool() : false;

    std::cout << "添加系统提示词：用户 ID = " << user_id << ", 名称 = " << name << std::endl;

    int prompt_id = Database::GetInstance()->AddSystemPrompt(user_id, name, content, is_default);

    Json::Value response;
    response["id"] = MSG_ADD_PROMPT;
    response["data"]["success"] = (prompt_id > 0);
    response["data"]["prompt_id"] = prompt_id;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnUpdatePrompt(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int prompt_id = data["data"]["prompt_id"].asInt();
    std::string name = data["data"]["name"].asString();
    std::string content = data["data"]["content"].asString();
    bool is_default = data["data"].isMember("is_default") ? data["data"]["is_default"].asBool() : false;

    std::cout << "更新系统提示词：ID = " << prompt_id << std::endl;

    bool success = Database::GetInstance()->UpdateSystemPrompt(prompt_id, name, content, is_default);

    Json::Value response;
    response["id"] = MSG_UPDATE_PROMPT;
    response["data"]["success"] = success;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnDeletePrompt(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
    int prompt_id = data["data"]["prompt_id"].asInt();

    std::cout << "删除系统提示词：ID = " << prompt_id << std::endl;

    bool success = Database::GetInstance()->DeleteSystemPrompt(prompt_id);

    Json::Value response;
    response["id"] = MSG_DELETE_PROMPT;
    response["data"]["success"] = success;

    con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnSetDefaultPrompt(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
	int prompt_id = data["data"]["prompt_id"].asInt();
	std::string name = data["data"]["name"].asString();
	std::string content = data["data"]["content"].asString();

	std::cout << "设置默认提示词：ID = " << prompt_id << std::endl;

	bool success = Database::GetInstance()->UpdateSystemPrompt(prompt_id, name, content, true);

	Json::Value response;
	response["id"] = MSG_SET_DEFAULT_PROMPT;
	response["data"]["success"] = success;

	con_ptr->AsyncSend(response.toStyledString());
}

void LogicSystem::OnTestModel(std::shared_ptr<Connection> con_ptr, const Json::Value& data)
{
	int user_id = data["data"]["user_id"].asInt();
	std::string provider_name = data["data"]["provider_name"].asString();

	std::cout << "测试模型连接：用户 ID = " << user_id << ", 提供商 = " << provider_name << std::endl;

	// 加载用户的模型配置
	AIModelManager::GetInstance().LoadModelsFromDatabase(user_id);

	// 获取要测试的模型
	std::shared_ptr<AIModel> model = AIModelManager::GetInstance().GetModel(provider_name);

	Json::Value response;
	response["id"] = MSG_TEST_MODEL_RESPONSE;
	response["data"]["provider_name"] = provider_name;

	if (!model) {
		response["data"]["success"] = false;
		response["data"]["message"] = "未找到指定的模型提供商";
		con_ptr->AsyncSend(response.toStyledString());
		return;
	}

	// 发送测试请求
	std::vector<ChatMessage> test_messages;
	test_messages.push_back({"user", "Hello, this is a test message. Please reply with 'OK'."});

	try {
		std::string result;
		model->GenerateStream(test_messages, [&result](const std::string& chunk) {
			result += chunk;
		});

		if (!result.empty() && result.find("Error") != 0 && result.find("失败") == std::string::npos) {
			response["data"]["success"] = true;
			response["data"]["message"] = "连接成功";
			response["data"]["response"] = result;
		} else {
			response["data"]["success"] = false;
			response["data"]["message"] = "模型返回错误: " + result;
		}
	} catch (const std::exception& e) {
		response["data"]["success"] = false;
		response["data"]["message"] = std::string("连接失败: ") + e.what();
	}

	con_ptr->AsyncSend(response.toStyledString());
}
