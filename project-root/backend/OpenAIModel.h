#pragma once
#include "AIModel.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace asio = boost::asio;
namespace http = beast::http;
using tcp = asio::ip::tcp;

// OpenAI API 兼容的模型实现
// 支持：DeepSeek、豆包、Kimi、LM Studio、Ollama 等
class OpenAIModel : public AIModel
{
public:
    // 构造函数
    // api_key: API密钥（本地模型如LM Studio可为空）
    // base_url: API基础URL，如 "https://api.deepseek.com/v1" 或 "http://localhost:1234/v1"
    // model_name: 模型名称，如 "deepseek-chat", "qwen2.5", "gpt-4"
    OpenAIModel(const std::string& api_key, const std::string& base_url, const std::string& model_name);
    
    ~OpenAIModel();

    // 获取模型名称
    std::string GetName() const override;
    
    // 获取模型类型
    std::string GetType() const override;
    
    // 检查模型是否可用
    bool IsAvailable() const override;
    
    // 流式生成回复
    std::string GenerateStream(
        const std::vector<ChatMessage>& messages,
        StreamCallback onChunk
    ) override;
    
    // HTTP流式生成
    std::string GenerateStreamHttp(
        const std::vector<ChatMessage>& messages,
        StreamCallback onChunk
    );
    
    // HTTPS流式生成
    std::string GenerateStreamHttps(
        const std::vector<ChatMessage>& messages,
        StreamCallback onChunk
    );

private:
    std::string api_key_;
    std::string base_url_;
    std::string model_name_;
    std::string host_;
    std::string path_;
    int port_;
    bool is_https_;
    
    // 解析URL
    void ParseUrl();
    
    // 构建请求体
    std::string BuildRequestBody(const std::vector<ChatMessage>& messages, bool stream) const;
    
    // 同步HTTP/HTTPS POST请求
    std::pair<int, std::string> HttpPost(const std::string& body);
    
    // 执行HTTPS请求
    std::pair<int, std::string> HttpsPost(const std::string& body);
    
    // 执行HTTP请求
    std::pair<int, std::string> HttpPostPlain(const std::string& body);
    
    // 解析响应中的content
    std::string ExtractContent(const std::string& json_str);
};
