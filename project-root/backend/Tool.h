#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

// 工具参数定义
struct ToolParameter {
    std::string name;
    std::string type;      // "string", "number", "boolean"
    std::string description;
    bool required = true;
};

// 工具接口
class Tool {
public:
    virtual ~Tool() = default;
    
    // 工具名称
    virtual std::string GetName() const = 0;
    
    // 工具描述
    virtual std::string GetDescription() const = 0;
    
    // 参数列表
    virtual std::vector<ToolParameter> GetParameters() const = 0;
    
    // 执行工具
    // params: JSON 格式的参数
    // 返回: 执行结果
    virtual std::string Execute(const std::string& params) = 0;
};

// 工具工厂
class ToolFactory {
public:
    // 注册工具
    static void RegisterTool(const std::string& name, std::shared_ptr<Tool> tool);
    
    // 获取工具
    static std::shared_ptr<Tool> GetTool(const std::string& name);
    
    // 获取所有工具名称
    static std::vector<std::string> GetToolNames();
    
    // 初始化所有工具
    static void InitializeTools();
    
private:
    static std::map<std::string, std::shared_ptr<Tool>> tools_;
};
