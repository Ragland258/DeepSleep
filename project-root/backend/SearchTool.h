#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <json/json.h>

#pragma comment(lib, "winhttp.lib")

struct SearchResult {
    std::string title;
    std::string snippet;
    std::string url;
};

class SearchTool {
public:
    SearchTool();
    ~SearchTool();

    std::vector<SearchResult> Search(const std::string& query, int max_results = 5);
    std::string BuildSearchContext(const std::vector<SearchResult>& results);
    std::vector<SearchResult> GetLastResults() const { return last_results_; }

private:
    std::wstring StringToWString(const std::string& str);
    std::string WStringToString(const std::wstring& wstr);
    std::vector<SearchResult> ParseJsonResults(const std::string& json_str, int max_results);
    std::vector<SearchResult> ParseHtmlResults(const std::string& html_str, int max_results);
    std::string StripHtmlTags(const std::string& html);
    std::vector<SearchResult> last_results_;
};