#include "SearchTool.h"
#include <iostream>

SearchTool::SearchTool() {
}

SearchTool::~SearchTool() {
}

std::wstring SearchTool::StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string SearchTool::WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::vector<SearchResult> SearchTool::Search(const std::string& query, int max_results) {
    std::vector<SearchResult> results;

    if (query.empty()) {
        return results;
    }

    // 使用 DuckDuckGo HTML 搜索（对中文支持更好）
    std::wstring host = L"html.duckduckgo.com";
    
    // URL 编码查询字符串
    std::wstring encodedQuery;
    for (size_t i = 0; i < query.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(query[i]);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encodedQuery += static_cast<wchar_t>(c);
        } else if (c == ' ') {
            encodedQuery += L'+';
        } else {
            // 转换为 %XX 格式
            wchar_t buf[4];
            swprintf_s(buf, L"%%%02X", c);
            encodedQuery += buf;
        }
    }
    
    std::wstring path = L"/html/?q=" + encodedQuery + L"&kl=cn-zh";

    // 尝试获取系统代理设置
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig = {0};
    BOOL hasProxy = FALSE;
    std::wstring proxyServer;
    
    if (WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig)) {
        if (proxyConfig.lpszProxy) {
            proxyServer = proxyConfig.lpszProxy;
            hasProxy = TRUE;
            std::wcout << L"[Search] System proxy detected: " << proxyServer << std::endl;
        } else if (proxyConfig.lpszAutoConfigUrl) {
            std::wcout << L"[Search] Auto proxy config detected: " << proxyConfig.lpszAutoConfigUrl << std::endl;
        }
        
        if (proxyConfig.lpszProxyBypass) {
            GlobalFree(proxyConfig.lpszProxyBypass);
        }
        if (proxyConfig.lpszProxy) {
            GlobalFree(proxyConfig.lpszProxy);
        }
        if (proxyConfig.lpszAutoConfigUrl) {
            GlobalFree(proxyConfig.lpszAutoConfigUrl);
        }
    }

    HINTERNET hSession = NULL;
    
    if (hasProxy && !proxyServer.empty()) {
        // 使用系统代理
        WINHTTP_PROXY_INFO proxyInfo;
        proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        proxyInfo.lpszProxy = const_cast<LPWSTR>(proxyServer.c_str());
        proxyInfo.lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
        
        hSession = WinHttpOpen(L"DeepSleep/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
            
        if (hSession) {
            WinHttpSetOption(hSession, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo));
            std::cout << "[Search] Using system proxy" << std::endl;
        }
    } else {
        // 不使用代理
        hSession = WinHttpOpen(L"DeepSleep/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        std::cout << "[Search] No system proxy detected, using direct connection" << std::endl;
    }

    if (!hSession) {
        std::cerr << "[Search] WinHttpOpen failed" << std::endl;
        return results;
    }

    DWORD dwTimeout = 30000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout));

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        std::cerr << "[Search] WinHttpConnect failed" << std::endl;
        WinHttpCloseHandle(hSession);
        return results;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);

    if (!hRequest) {
        std::cerr << "[Search] WinHttpOpenRequest failed" << std::endl;
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return results;
    }

    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout));

    BOOL bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    std::string response_data;

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
        if (bResults) {
            DWORD dwSize = 0;
            while (true) {
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) {
                    break;
                }

                std::vector<char> buffer(dwSize + 1);
                DWORD dwRead = 0;

                if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwRead)) {
                    buffer[dwRead] = '\0';
                    response_data.append(&buffer[0], dwRead);
                } else {
                    break;
                }
            }
        }
    }

    DWORD dwError = GetLastError();

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (!bResults) {
        std::cerr << "[Search] WinHttpSendRequest failed. Error: " << dwError << std::endl;
        return results;
    }

    std::cout << "[Search] Searching for: " << query << std::endl;
    std::cout << "[Search] Response length: " << response_data.length() << std::endl;
    
    // 输出 HTML 前 1000 字符用于调试
    std::cout << "[Search] HTML preview: " << response_data.substr(0, 1000) << std::endl;

    // 使用 HTML 解析
    results = ParseHtmlResults(response_data, max_results);
    last_results_ = results;

    std::cout << "[Search] Found " << results.size() << " results" << std::endl;

    return results;
}

std::vector<SearchResult> SearchTool::ParseJsonResults(const std::string& json_str, int max_results) {
    std::vector<SearchResult> results;

    if (json_str.empty()) {
        return results;
    }

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(json_str, root)) {
        std::cerr << "[Search] JSON parse error: " << reader.getFormattedErrorMessages() << std::endl;
        return results;
    }

    if (root.isMember("AbstractText") && !root["AbstractText"].asString().empty()) {
        SearchResult result;
        result.title = root.isMember("Heading") ? root["Heading"].asString() : "Wikipedia";
        result.snippet = root["AbstractText"].asString();
        result.url = root.isMember("AbstractURL") ? root["AbstractURL"].asString() : "";
        results.push_back(result);
        std::cout << "[Search] Abstract found: " << result.title << std::endl;
    }

    if (root.isMember("RelatedTopics") && root["RelatedTopics"].isArray()) {
        for (const auto& topic : root["RelatedTopics"]) {
            if ((int)results.size() >= max_results) {
                break;
            }

            if (topic.isMember("Text") && !topic["Text"].asString().empty()) {
                SearchResult result;
                result.title = topic["Text"].asString();

                if (result.title.length() > 200) {
                    result.snippet = result.title.substr(0, 200) + "...";
                } else {
                    result.snippet = result.title;
                }

                result.url = topic.isMember("FirstURL") ? topic["FirstURL"].asString() : "";

                if (!result.title.empty()) {
                    results.push_back(result);
                }
            }
        }
    }

    if (root.isMember("Results") && root["Results"].isArray()) {
        for (const auto& result_item : root["Results"]) {
            if ((int)results.size() >= max_results) {
                break;
            }

            if (result_item.isMember("Text") && !result_item["Text"].asString().empty()) {
                SearchResult result;
                result.title = result_item["Text"].asString();
                result.snippet = result.title;
                result.url = result_item.isMember("FirstURL") ? result_item["FirstURL"].asString() : "";

                if (!result.title.empty()) {
                    results.push_back(result);
                }
            }
        }
    }

    return results;
}

std::vector<SearchResult> SearchTool::ParseHtmlResults(const std::string& html_str, int max_results) {
    std::vector<SearchResult> results;

    if (html_str.empty()) {
        return results;
    }

    // 简单的 HTML 解析，提取搜索结果
    // DuckDuckGo HTML 结果格式可能变化，尝试多种匹配方式
    
    size_t pos = 0;
    int count = 0;
    
    // 尝试多种可能的 class 名称
    std::vector<std::string> linkPatterns = {
        "class=\"result__a\"",
        "class='result__a'",
        "class=\"result-title\"",
        "class='result-title'",
        "class=\"resultLink\"",
        "class='resultLink'"
    };
    
    std::vector<std::string> snippetPatterns = {
        "class=\"result__snippet\"",
        "class='result__snippet'",
        "class=\"result-snippet\"",
        "class='result-snippet'",
        "class=\"resultDescription\"",
        "class='resultDescription'"
    };
    
    while (pos < html_str.length() && count < max_results) {
        size_t linkStart = std::string::npos;
        std::string matchedPattern;
        
        // 尝试所有链接模式
        for (const auto& pattern : linkPatterns) {
            size_t found = html_str.find(pattern, pos);
            if (found != std::string::npos && (linkStart == std::string::npos || found < linkStart)) {
                linkStart = found;
                matchedPattern = pattern;
            }
        }
        
        if (linkStart == std::string::npos) {
            // 如果没找到标准模式，尝试通用 <a href= 模式
            linkStart = html_str.find("<a href=\"//", pos);
            if (linkStart == std::string::npos) break;
        }
        
        size_t hrefStart = html_str.find("href=\"", linkStart);
        if (hrefStart == std::string::npos) break;
        hrefStart += 6;
        
        size_t hrefEnd = html_str.find("\"", hrefStart);
        if (hrefEnd == std::string::npos) break;
        
        std::string url = html_str.substr(hrefStart, hrefEnd - hrefStart);
        // 处理相对 URL
        if (url.find("//") == 0) {
            url = "https:" + url;
        } else if (url.find("http") != 0) {
            url = "https://duckduckgo.com" + url;
        }
        
        // 查找标题
        size_t titleStart = html_str.find(">", hrefEnd) + 1;
        size_t titleEnd = html_str.find("</a>", titleStart);
        if (titleEnd == std::string::npos) break;
        
        std::string title = html_str.substr(titleStart, titleEnd - titleStart);
        title = StripHtmlTags(title);
        
        // 查找摘要 - 尝试多种模式
        std::string snippet;
        for (const auto& pattern : snippetPatterns) {
            size_t snippetStart = html_str.find(pattern, titleEnd);
            if (snippetStart != std::string::npos) {
                size_t snippetHrefEnd = html_str.find(">", snippetStart) + 1;
                size_t snippetEnd = html_str.find("</a>", snippetHrefEnd);
                if (snippetEnd != std::string::npos) {
                    snippet = html_str.substr(snippetHrefEnd, snippetEnd - snippetHrefEnd);
                    snippet = StripHtmlTags(snippet);
                    break;
                }
            }
        }
        
        // 如果没找到摘要，尝试查找 <div class="result__snippet">
        if (snippet.empty()) {
            size_t divSnippetStart = html_str.find("class=\"result__snippet\">", titleEnd);
            if (divSnippetStart != std::string::npos) {
                divSnippetStart = html_str.find(">", divSnippetStart) + 1;
                size_t divSnippetEnd = html_str.find("</div>", divSnippetStart);
                if (divSnippetEnd != std::string::npos) {
                    snippet = html_str.substr(divSnippetStart, divSnippetEnd - divSnippetStart);
                    snippet = StripHtmlTags(snippet);
                }
            }
        }
        
        if (!title.empty() && title.length() > 5) {  // 过滤太短的标题
            SearchResult result;
            result.title = title;
            result.snippet = snippet.empty() ? title : snippet;
            result.url = url;
            results.push_back(result);
            count++;
            std::cout << "[Search] Found result " << count << ": " << title.substr(0, 50) << "..." << std::endl;
        }
        
        pos = titleEnd + 1;
    }

    return results;
}

std::string SearchTool::StripHtmlTags(const std::string& html) {
    std::string result;
    bool inTag = false;
    
    for (char c : html) {
        if (c == '<') {
            inTag = true;
        } else if (c == '>') {
            inTag = false;
        } else if (!inTag) {
            result += c;
        }
    }
    
    // 解码 HTML 实体
    size_t pos = 0;
    while ((pos = result.find("&quot;", pos)) != std::string::npos) {
        result.replace(pos, 6, "\"");
    }
    pos = 0;
    while ((pos = result.find("&amp;", pos)) != std::string::npos) {
        result.replace(pos, 5, "&");
    }
    pos = 0;
    while ((pos = result.find("&lt;", pos)) != std::string::npos) {
        result.replace(pos, 4, "<");
    }
    pos = 0;
    while ((pos = result.find("&gt;", pos)) != std::string::npos) {
        result.replace(pos, 4, ">");
    }
    
    return result;
}

std::string SearchTool::BuildSearchContext(const std::vector<SearchResult>& results) {
    if (results.empty()) {
        return "";
    }

    std::string context = "【重要】你已经通过联网搜索获取到了以下实时信息，请基于这些信息回答用户的问题。不要说你无法访问互联网或实时数据。\n\n";
    context += "搜索结果如下：\n\n";

    int index = 1;
    for (const auto& r : results) {
        context += "【来源 " + std::to_string(index) + "】\n";
        context += "标题：" + r.title + "\n";
        context += "摘要：" + r.snippet + "\n";
        if (!r.url.empty()) {
            context += "链接：" + r.url + "\n";
        }
        context += "\n";
        ++index;
    }

    context += "\n【要求】\n";
    context += "1. 基于上述搜索结果回答用户问题\n";
    context += "2. 不要说你无法访问互联网或实时数据\n";
    context += "3. 用 [1][2] 等序号标注你引用的来源\n";
    context += "4. 如果搜索结果不足以回答问题，请说明\"根据搜索结果，我了解到...\"\n";

    return context;
}