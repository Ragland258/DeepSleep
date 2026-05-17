#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <json/json.h>

// 消息结构
struct ChatMessage 
{
    std::string role;    // "system", "user", "assistant"
    std::string content;
};

// 流式回调函数类型
using StreamCallback = std::function<void(const std::string& chunk)>;

// AI 模型接口基类
class AIModel 
{
public: 
    virtual ~AIModel() = default;
    
    // 获取模型名称
    virtual std::string GetName() const = 0;
    
    // 获取模型类型 ("openai", "ollama", "local", etc.)
    virtual std::string GetType() const = 0;
    
    // 检查模型是否可用
    virtual bool IsAvailable() const = 0;
    
    // 流式生成回复
    // messages: 对话历史
    // onChunk: 收到每个数据块的回调
    // 返回: 完整的回复内容
    virtual std::string GenerateStream(
        const std::vector<ChatMessage>& messages,
        StreamCallback onChunk
    ) = 0;
    //一开始调试的时候用的
    ////// 非流式生成
    ////virtual std::string Generate(const std::vector<ChatMessage>& messages) 
    ////{
    ////    std::string result;
    ////    GenerateStream(messages, [&result](const std::string& chunk) {
    ////        result += chunk;
    ////    });
    ////    return result;
    //}
};

