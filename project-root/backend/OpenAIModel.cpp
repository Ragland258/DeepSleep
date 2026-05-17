#include "OpenAIModel.h"
#include <iostream>
#include <sstream>
#include <openssl/ssl.h>

OpenAIModel::OpenAIModel(const std::string& api_key, const std::string& base_url, const std::string& model_name)
    : api_key_(api_key), base_url_(base_url), model_name_(model_name), port_(80), is_https_(false)
{
    ParseUrl();
}

OpenAIModel::~OpenAIModel() = default;

std::string OpenAIModel::GetName() const
{
    return model_name_;
}

std::string OpenAIModel::GetType() const
{
    return "openai_compatible";
}

bool OpenAIModel::IsAvailable() const
{
    // 简单检查：尝试发送一个空请求看是否连接成功
    try
    {
        std::vector<ChatMessage> test_msg;
        test_msg.push_back({"user", "hi"});
        std::string body = BuildRequestBody(test_msg, false);
        auto result = const_cast<OpenAIModel*>(this)->HttpPost(body);
        return result.first == 200;
    }
    catch (...)
    {
        return false;
    }
}

void OpenAIModel::ParseUrl()
{
    // 解析URL，提取host, path, port, is_https
    std::string url = base_url_;
    
    // 检查协议
    if (url.find("https://") == 0)
    {
        is_https_ = true;
        port_ = 443;
        url = url.substr(8);
    }
    else if (url.find("http://") == 0)
    {
        is_https_ = false;
        port_ = 80;
        url = url.substr(7);
    }
    
    // 查找路径
    size_t path_pos = url.find('/');
    if (path_pos != std::string::npos)
    {
        host_ = url.substr(0, path_pos);
        path_ = url.substr(path_pos);
    }
    else
    {
        host_ = url;
        path_ = "";
    }
    
    // 检查端口
    size_t port_pos = host_.find(':');
    if (port_pos != std::string::npos)
    {
        port_ = std::stoi(host_.substr(port_pos + 1));
        host_ = host_.substr(0, port_pos);
    }
    
    // 确保path以 /chat/completions 结尾
    if (path_.empty() || path_ == "/")
    {
        path_ = "/v1/chat/completions";
    }
    else if (path_.find("/chat/completions") == std::string::npos)
    {
        if (path_.back() == '/')
        {
            path_ += "chat/completions";
        }
        else
        {
            path_ += "/chat/completions";
        }
    }
    
    std::cout << "[OpenAIModel] Parsed URL - Host: " << host_ 
              << ", Port: " << port_ 
              << ", Path: " << path_ 
              << ", HTTPS: " << (is_https_ ? "yes" : "no") << std::endl;
}

std::string OpenAIModel::BuildRequestBody(const std::vector<ChatMessage>& messages, bool stream) const
{
    Json::Value root;
    root["model"] = model_name_;
    root["stream"] = stream;
    
    Json::Value msgs(Json::arrayValue);
    for (const auto& msg : messages)
    {
        Json::Value m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        msgs.append(m);
    }
    root["messages"] = msgs;
    
    // 可选参数
    root["temperature"] = 0.7;
    root["max_tokens"] = 2048;
    
    return root.toStyledString();
}

std::pair<int, std::string> OpenAIModel::HttpPost(const std::string& body)
{
    if (is_https_)
    {
        return HttpsPost(body);
    }
    else
    {
        return HttpPostPlain(body);
    }
}

std::pair<int, std::string> OpenAIModel::HttpPostPlain(const std::string& body)
{
    try
    {
        asio::io_context ioc;
        
        // HTTP请求
        beast::tcp_stream stream(ioc);
        
        // 解析域名
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host_, std::to_string(port_));
        
        // 连接
        stream.connect(results);
        
        // 构建HTTP请求
        http::request<http::string_body> req{http::verb::post, path_, 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "DeepSleep/1.0");
        req.set(http::field::content_type, "application/json");
        if (!api_key_.empty())
        {
            req.set(http::field::authorization, "Bearer " + api_key_);
        }
        req.body() = body;
        req.prepare_payload();
        
        // 发送请求
        http::write(stream, req);
        
        // 接收响应
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        int status_code = res.result_int();
        std::string response_body = res.body();
        
        // 关闭连接
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        
        return {status_code, response_body};
    }
    catch (std::exception const& e)
    {
        std::cerr << "[OpenAIModel] HTTP POST error: " << e.what() << std::endl;
        return {-1, std::string("Error: ") + e.what()};
    }
}

std::pair<int, std::string> OpenAIModel::HttpsPost(const std::string& body)
{
    try
    {
        asio::io_context ioc;
        
        // SSL上下文
        asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
        ctx.set_default_verify_paths();
        
        // HTTPS流
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
        
        // 设置SNI（Server Name Indication）
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host_.c_str()))
        {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
            throw beast::system_error{ec};
        }
        
        // 解析域名
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host_, std::to_string(port_));
        
        // 连接
        beast::get_lowest_layer(stream).connect(results);
        
        // SSL握手
        stream.handshake(asio::ssl::stream_base::client);
        
        // 构建HTTP请求
        http::request<http::string_body> req{http::verb::post, path_, 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "DeepSleep/1.0");
        req.set(http::field::content_type, "application/json");
        if (!api_key_.empty())
        {
            req.set(http::field::authorization, "Bearer " + api_key_);
        }
        req.body() = body;
        req.prepare_payload();
        
        // 发送请求
        http::write(stream, req);
        
        // 接收响应
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        int status_code = res.result_int();
        std::string response_body = res.body();
        
        // 关闭连接
        beast::error_code ec;
        stream.shutdown(ec);
        
        return {status_code, response_body};
    }
    catch (std::exception const& e)
    {
        std::cerr << "[OpenAIModel] HTTPS POST error: " << e.what() << std::endl;
        return {-1, std::string("Error: ") + e.what()};
    }
}

std::string OpenAIModel::ExtractContent(const std::string& json_str)
{
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(json_str, root))
    {
        if (root.isMember("choices") && root["choices"].isArray() && !root["choices"].empty())
        {
            const Json::Value& choice = root["choices"][0];
            if (choice.isMember("message") && choice["message"].isMember("content"))
            {
                return choice["message"]["content"].asString();
            }
        }
    }
    return "";
}

// 解析SSE数据行，提取content
std::string ParseSSELine(const std::string& line)
{
    // SSE格式: data: {...}
    if (line.find("data: ") == 0)
    {
        std::string json_data = line.substr(6);
        
        // 检查是否是结束标记
        if (json_data == "[DONE]")
        {
            return "";
        }
        
        // 解析JSON
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(json_data, root))
        {
            if (root.isMember("choices") && root["choices"].isArray() && !root["choices"].empty())
            {
                const Json::Value& choice = root["choices"][0];
                if (choice.isMember("delta") && choice["delta"].isMember("content"))
                {
                    return choice["delta"]["content"].asString();
                }
            }
        }
    }
    return "";
}

std::string OpenAIModel::GenerateStream(
    const std::vector<ChatMessage>& messages,
    StreamCallback onChunk
)
{
    if (is_https_)
    {
        return GenerateStreamHttps(messages, onChunk);
    }
    else
    {
        return GenerateStreamHttp(messages, onChunk);
    }
}

std::string OpenAIModel::GenerateStreamHttp(
    const std::vector<ChatMessage>& messages,
    StreamCallback onChunk
)
{
    try
    {
        // 使用流式模式
        std::string body = BuildRequestBody(messages, true);
        
        asio::io_context ioc;
        beast::tcp_stream stream(ioc);
        
        // 解析域名
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host_, std::to_string(port_));
        
        // 连接
        stream.connect(results);
        
        // 构建HTTP请求（流式）
        http::request<http::string_body> req{http::verb::post, path_, 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "DeepSleep/1.0");
        req.set(http::field::content_type, "application/json");
        req.set(http::field::accept, "text/event-stream");
        if (!api_key_.empty())
        {
            req.set(http::field::authorization, "Bearer " + api_key_);
        }
        req.body() = body;
        req.prepare_payload();
        
        // 发送请求
        http::write(stream, req);
        
        // 读取流式响应头
        beast::flat_buffer buffer;
        http::response_parser<http::string_body> parser;
        parser.body_limit(std::numeric_limits<std::uint64_t>::max());
        
        // 读取头部
        http::read_header(stream, buffer, parser);
        
        std::string full_content;
        std::string leftover; // 用于存储不完整的数据行
        
        // 循环读取数据体
        while (!parser.is_done())
        {
            beast::error_code ec;
            
            // 读取一些数据
            auto bytes_transferred = http::read_some(stream, buffer, parser, ec);
            
            if (ec == http::error::need_buffer)
            {
                // 需要更多数据，继续读取
                continue;
            }
            else if (ec == asio::error::eof || ec == asio::error::connection_reset)
            {
                // 连接关闭，结束读取
                break;
            }
            else if (ec)
            {
                std::cerr << "[OpenAIModel] Read error: " << ec.message() << std::endl;
                break;
            }
            
            // 获取当前解析出的body内容
            auto& body = parser.get().body();
            
            // 将新数据与之前剩余的数据合并
            std::string new_data;
            if (bytes_transferred > 0 && body.length() >= static_cast<size_t>(bytes_transferred))
            {
                new_data = body.substr(body.length() - bytes_transferred);
            }
            else if (!body.empty())
            {
                new_data = body;
            }
            std::string data = leftover + new_data;
            
            // 解析SSE数据
            std::istringstream data_stream(data);
            std::string line;
            leftover.clear();
            
            while (std::getline(data_stream, line))
            {
                // 检查是否是最后一行（可能不完整）
                if (data_stream.eof() && !line.empty() && line.back() != '\n' && line.back() != '\r')
                {
                    leftover = line;
                    break;
                }
                
                // 移除可能的回车符
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                
                // 跳过空行
                if (line.empty())
                {
                    continue;
                }
                
                std::string content = ParseSSELine(line);
                if (!content.empty())
                {
                    full_content += content;
                    onChunk(content);
                }
            }
        }
        
        // 处理最后剩余的数据
        if (!leftover.empty())
        {
            std::string content = ParseSSELine(leftover);
            if (!content.empty())
            {
                full_content += content;
                onChunk(content);
            }
        }
        
        // 关闭连接
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        
        return full_content;
    }
    catch (std::exception const& e)
    {
        std::cerr << "[OpenAIModel] Stream error: " << e.what() << std::endl;
        std::string error_msg = "模型调用失败：" + std::string(e.what());
        onChunk(error_msg);
        return error_msg;
    }
}

std::string OpenAIModel::GenerateStreamHttps(
    const std::vector<ChatMessage>& messages,
    StreamCallback onChunk
)
{
    try
    {
        // 使用流式模式
        std::string body = BuildRequestBody(messages, true);
        
        asio::io_context ioc;
        
        // SSL上下文
        asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
        ctx.set_default_verify_paths();
        
        // HTTPS流
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
        
        // 设置SNI（Server Name Indication）
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host_.c_str()))
        {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
            throw beast::system_error{ec};
        }
        
        // 解析域名
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host_, std::to_string(port_));
        
        // 连接
        beast::get_lowest_layer(stream).connect(results);
        
        // SSL握手
        stream.handshake(asio::ssl::stream_base::client);
        
        // 构建HTTP请求（流式）
        http::request<http::string_body> req{http::verb::post, path_, 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, "DeepSleep/1.0");
        req.set(http::field::content_type, "application/json");
        req.set(http::field::accept, "text/event-stream");
        if (!api_key_.empty())
        {
            req.set(http::field::authorization, "Bearer " + api_key_);
        }
        req.body() = body;
        req.prepare_payload();
        
        // 发送请求
        http::write(stream, req);
        
        // 读取流式响应头
        beast::flat_buffer buffer;
        http::response_parser<http::string_body> parser;
        parser.body_limit(std::numeric_limits<std::uint64_t>::max());
        
        // 读取头部
        http::read_header(stream, buffer, parser);
        
        std::string full_content;
        std::string leftover; // 用于存储不完整的数据行
        
        // 循环读取数据体
        while (!parser.is_done())
        {
            beast::error_code ec;
            
            // 读取一些数据
            auto bytes_transferred = http::read_some(stream, buffer, parser, ec);
            
            if (ec == http::error::need_buffer)
            {
                // 需要更多数据，继续读取
                continue;
            }
            else if (ec == asio::error::eof || ec == asio::error::connection_reset)
            {
                // 连接关闭，结束读取
                break;
            }
            else if (ec)
            {
                std::cerr << "[OpenAIModel] HTTPS Read error: " << ec.message() << std::endl;
                break;
            }
            
            // 获取当前解析出的body内容
            auto& body = parser.get().body();
            
            // 将新数据与之前剩余的数据合并
            std::string new_data;
            if (bytes_transferred > 0 && body.length() >= static_cast<size_t>(bytes_transferred))
            {
                new_data = body.substr(body.length() - bytes_transferred);
            }
            else if (!body.empty())
            {
                new_data = body;
            }
            std::string data = leftover + new_data;
            
            // 解析SSE数据
            std::istringstream data_stream(data);
            std::string line;
            leftover.clear();
            
            while (std::getline(data_stream, line))
            {
                // 检查是否是最后一行（可能不完整）
                if (data_stream.eof() && !line.empty() && line.back() != '\n' && line.back() != '\r')
                {
                    leftover = line;
                    break;
                }
                
                // 移除可能的回车符
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                
                // 跳过空行
                if (line.empty())
                {
                    continue;
                }
                
                std::string content = ParseSSELine(line);
                if (!content.empty())
                {
                    full_content += content;
                    onChunk(content);
                }
            }
        }
        
        // 处理最后剩余的数据
        if (!leftover.empty())
        {
            std::string content = ParseSSELine(leftover);
            if (!content.empty())
            {
                full_content += content;
                onChunk(content);
            }
        }
        
        // 关闭连接
        beast::error_code ec;
        stream.shutdown(ec);
        
        return full_content;
    }
    catch (std::exception const& e)
    {
        std::cerr << "[OpenAIModel] HTTPS Stream error: " << e.what() << std::endl;
        std::string error_msg = "模型调用失败：" + std::string(e.what());
        onChunk(error_msg);
        return error_msg;
    }
}
