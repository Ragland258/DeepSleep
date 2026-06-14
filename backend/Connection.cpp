#include "Connection.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstdlib>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif
#include "ConnectionMgr.h"
#include "UserActor.h"

namespace {
bool FileExists(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    return file.good();
}

std::string JoinWindowsPath(const std::string& base, const std::string& child)
{
    if (base.empty())
    {
        return child;
    }

    if (base.back() == '/')
    {
        return base + child;
    }

    return base + "/" + child;
}

std::string UrlDecode(const std::string& input)
{
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] == '%' && i + 2 < input.size()
            && std::isxdigit(static_cast<unsigned char>(input[i + 1]))
            && std::isxdigit(static_cast<unsigned char>(input[i + 2])))
        {
            const std::string hex = input.substr(i + 1, 2);
            output.push_back(static_cast<char>(std::strtoul(hex.c_str(), nullptr, 16)));
            i += 2;
        }
        else if (input[i] == '+')
        {
            output.push_back(' ');
        }
        else
        {
            output.push_back(input[i]);
        }
    }

    return output;
}

void CloseHttpSocket(beast::websocket::stream<boost::asio::ip::tcp::socket>& ws)
{
    boost::system::error_code ignored_ec;
    if (ws.next_layer().is_open())
    {
        ws.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
        ws.next_layer().close(ignored_ec);
    }
}
}

Connection::Connection(asio::io_context& ioc)
    : _ioc(ioc), _ws_ptr(std::make_unique<beast::websocket::stream<asio::ip::tcp::socket>>(ioc))
{
    boost::uuids::random_generator generator;
    boost::uuids::uuid uuid(generator());
    _uuid = boost::uuids::to_string(uuid);
}

void Connection::AsyncAccpet()
{
    ReadHttpHeader();
}

void Connection::ReadHttpHeader()
{
    auto self = shared_from_this();

    _http_request = beast::http::request<beast::http::string_body>{};

    beast::http::async_read(
        _ws_ptr->next_layer(),
        _recv_buffer,
        _http_request,
        [this, self](beast::error_code ec, std::size_t bytes_used)
        {
            OnHttpHeader(ec, bytes_used);
        });
}

std::string Connection::ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return "";
    }

    return std::string(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
}

bool EndsWith(const std::string& str, const std::string& suffix)
{
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string Connection::GetContentType(const std::string& path)
{
    if (EndsWith(path, ".html")) return "text/html";
    if (EndsWith(path, ".css")) return "text/css";
    if (EndsWith(path, ".js")) return "application/javascript";
    if (EndsWith(path, ".json")) return "application/json";
    if (EndsWith(path, ".png")) return "image/png";
    if (EndsWith(path, ".jpg") || EndsWith(path, ".jpeg")) return "image/jpeg";
    if (EndsWith(path, ".gif")) return "image/gif";
    return "text/plain";
}

std::string Connection::ResolveFrontendRoot() const
{
    std::string exe_dir;
    std::string current_dir;

#ifdef _WIN32
    char exe_path[MAX_PATH] = { 0 };
    const DWORD length = GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        exe_dir.assign(exe_path, length);
        const size_t pos = exe_dir.find_last_of("\\/");
        if (pos != std::string::npos)
            exe_dir.erase(pos);
    }
    char cwd_buffer[MAX_PATH] = { 0 };
    if (_getcwd(cwd_buffer, MAX_PATH) != nullptr)
        current_dir = cwd_buffer;
#else
    char exe_path[PATH_MAX] = { 0 };
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0)
    {
        exe_path[len] = '\0';
        exe_dir.assign(exe_path, len);
        const size_t pos = exe_dir.find_last_of('/');
        if (pos != std::string::npos)
            exe_dir.erase(pos);
    }
    char cwd_buffer[PATH_MAX] = { 0 };
    if (getcwd(cwd_buffer, sizeof(cwd_buffer)) != nullptr)
        current_dir = cwd_buffer;
#endif

    const std::vector<std::string> candidates = {
        JoinWindowsPath(exe_dir, "frontend"),
        JoinWindowsPath(exe_dir, "../../../project-root/frontend"),
        JoinWindowsPath(current_dir, "frontend"),
        JoinWindowsPath(current_dir, "project-root/frontend"),
        "project-root/frontend"
    };

    for (const auto& candidate : candidates)
    {
        if (!candidate.empty() && FileExists(JoinWindowsPath(candidate, "index.html")))
        {
            return candidate;
        }
    }

    return "";
}

bool Connection::BuildSafeFilePath(const std::string& target, const std::string& root, std::string& file_path) const
{
    if (root.empty() || target.empty())
    {
        return false;
    }

    std::string resource = target;
    const size_t query_pos = resource.find_first_of("?#");
    if (query_pos != std::string::npos)
    {
        resource.erase(query_pos);
    }

    resource = UrlDecode(resource);
    if (resource.empty() || resource.front() != '/')
    {
        return false;
    }

    for (char& ch : resource)
    {
        if (ch == '\\')
        {
            ch = '/';
        }
    }

    std::vector<std::string> segments;
    std::stringstream stream(resource);
    std::string segment;
    while (std::getline(stream, segment, '/'))
    {
        if (segment.empty())
        {
            continue;
        }

        if (segment == "." || segment == ".." || segment.find(':') != std::string::npos
            || segment.find('\0') != std::string::npos)
        {
            return false;
        }

        segments.push_back(segment);
    }

    if (resource.back() == '/' || segments.empty())
    {
        segments.push_back("index.html");
    }

    file_path = root;
    for (const auto& part : segments)
    {
        file_path = JoinWindowsPath(file_path, part);
    }

    return true;
}

void Connection::OnHttpHeader(beast::error_code ec, std::size_t bytes_used)
{
    if (ec)
    {
        std::cout << "HTTP read error: " << ec.message() << std::endl;
        return;
    }

    auto self = shared_from_this();

    std::string method(_http_request.method_string());
    std::string target(_http_request.target());
    std::string upgrade = std::string(_http_request[beast::http::field::upgrade]);

    std::cout << "HTTP request: " << method << " " << target << std::endl;

    bool is_websocket = false;
    if (method == "GET" &&
        (upgrade.find("websocket") != std::string::npos ||
         upgrade.find("WebSocket") != std::string::npos))
    {
        is_websocket = true;
    }

    if (is_websocket)
    {
        std::cout << "WebSocket upgrade request detected" << std::endl;

        _ws_ptr->async_accept(_http_request,
            [this, self](beast::error_code accept_ec)
            {
                if (accept_ec)
                {
                    std::cout << "WebSocket handshake failed: " << accept_ec.message() << std::endl;
                    return;
                }

                std::cout << "WebSocket handshake successful, uuid: " << _uuid << std::endl;

                ConnectionMgr::GetInstance().AddConnection(self);
                Start();
            });
        return;
    }

    std::cout << "Serving HTTP request: " << target << std::endl;

    const std::string frontend_root = ResolveFrontendRoot();
    std::string file_path;
    const bool safe_path = BuildSafeFilePath(target, frontend_root, file_path);
    const std::string content = safe_path ? ReadFile(file_path) : "";

    auto res = std::make_shared<beast::http::response<beast::http::string_body>>();
    res->version(_http_request.version());
    res->set(beast::http::field::server, "DeepSleep Server");
    res->set(beast::http::field::content_type, "text/plain");

    if (!safe_path || content.empty())
    {
        if (!safe_path)
        {
            std::cout << "Rejected unsafe HTTP path: " << target << std::endl;
            res->result(beast::http::status::bad_request);
            res->body() = "400 Bad Request";
        }
        else
        {
            std::cout << "File not found: " << file_path << std::endl;
            res->result(beast::http::status::not_found);
            res->body() = "404 Not Found";
        }
    }
    else
    {
        std::cout << "Serving file: " << file_path << std::endl;
        res->result(beast::http::status::ok);
        res->set(beast::http::field::content_type, GetContentType(file_path));
        res->body() = content;
    }

    res->prepare_payload();

    beast::http::async_write(
        _ws_ptr->next_layer(),
        *res,
        [this, self, res](beast::error_code write_ec, std::size_t)
        {
            CloseHttpSocket(*_ws_ptr);
        });
}

void Connection::Start()
{
    auto self = shared_from_this();

    _ws_ptr->async_read(_recv_buffer,
        [this, self](beast::error_code ec, std::size_t bytes_transferred)
        {
            if (ec)
            {
                if (ec == beast::websocket::error::closed)
                {
                    std::cout << "WebSocket connection closed, uuid: " << _uuid << std::endl;
                }
                else
                {
                    std::cout << "WebSocket read error: " << ec.message() << std::endl;
                }
                ConnectionMgr::GetInstance().RmvConnection(_uuid);
                return;
            }

            std::string recv_data = beast::buffers_to_string(_recv_buffer.data());
            _recv_buffer.consume(bytes_transferred);

            std::cout << "Received from " << _uuid << ": " << recv_data << std::endl;

            Json::Value root;
            Json::Reader reader;
            if (reader.parse(recv_data, root))
            {
                // 处理心跳 ping 消息
                if (root.isMember("type") && root["type"].asString() == "ping")
                {
                    Json::Value pong;
                    pong["type"] = "pong";
                    pong["timestamp"] = static_cast<int64_t>(std::time(nullptr));
                    std::string pong_msg = pong.toStyledString();
                    std::cout << "[Heartbeat] Sending pong to " << _uuid << std::endl;
                    AsyncSend(pong_msg);
                }
                else if (root.isMember("id"))
                {
                    // 投递给自己的 Actor
                    if (_actor)
                    {
                        _actor->OnClientMessage(root);
                    }
                }
                else
                {
                    std::cout << "Missing 'id' field in message" << std::endl;
                }
            }
            else
            {
                std::cout << "JSON parse error: " << reader.getFormattedErrorMessages() << std::endl;
            }

            Start();
        });
}

void Connection::AsyncSend(std::string msg)
{
    {
        std::lock_guard<std::mutex> lock(_send_mtx);
        bool write_in_progress = !_send_queue.empty();
        _send_queue.push(msg);
        if (write_in_progress)
            return;
    }

    DoWrite();
}

void Connection::DoWrite()
{
    auto self = shared_from_this();

    _ws_ptr->text(true);
    _ws_ptr->async_write(
        asio::buffer(_send_queue.front()),
        [this, self](beast::error_code ec, std::size_t)
        {
            if (ec)
            {
                std::cout << "WebSocket write error: " << ec.message() << std::endl;
                // Clear the queue on error to prevent message blocking
                {
                    std::lock_guard<std::mutex> lock(_send_mtx);
                    while (!_send_queue.empty())
                        _send_queue.pop();
                }
                return;
            }

            {
                std::lock_guard<std::mutex> lock(_send_mtx);
                _send_queue.pop();
                if (_send_queue.empty())
                    return;
            }

            DoWrite();
        });
}
