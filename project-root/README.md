# DeepSleep AI Chat

一个基于 C++ 和 WebSocket 的 AI 聊天应用，支持多种大语言模型接入。

完成于4/21日,由于自己画的流程图没人能看懂,就使用ai画了

## 项目简介

DeepSleep 是一个完整的大模型对话系统，覆盖从前端交互界面到后端服务、模型调用、数据持久化的完整链路。系统支持多用户隔离、流式对话输出、上下文记忆等功能。

## 技术栈

### 后端

- **语言**: C++14
- **网络**: Boost.Asio + Boost.Beast (WebSocket/HTTP)
- **数据库**: MySQL 8.0
- **JSON**: jsoncpp
- **架构**: 单例模式 + 异步回调机制

### 前端

- **HTML5 + CSS3 + JavaScript**
- **WebSocket** 实时通信
- **响应式设计**，支持移动端访问

## 系统架构

```C++
┌─────────────────┐     WebSocket      ┌─────────────────┐
│   前端页面       │ ←─────────────────→ │   后端服务       │
│  (HTML/CSS/JS)   │                    │   (C++/Boost)    │
└─────────────────┘                    └────────┬─────────┘
                                                │
                               ┌─────────────────┼─────────────────┐
                               │                 │                 │
                        ┌──────▼──────┐  ┌──────▼──────┐  ┌──────▼──────┐
                        │   MySQL     │  │  大模型API   │  │   用户认证   │
                        │  数据库      │  │ (DeepSeek等) │  │             │
                        └─────────────┘  └─────────────┘  └─────────────┘
```

## 功能列表

### 基础功能 ✅

- [x] 用户注册/登录（多用户隔离）
- [x] 与大语言模型进行多轮对话
- [x] 流式输出（实时逐字显示）
- [x] 上下文记忆与理解
- [x] 会话管理（新建/删除/历史）
- [x] 历史消息加载与展示
- [x] 模型选择功能

### 加分项

- [x] 联网搜索开关 ✅  //请使用科学上网,
- [x] 上下文工程优化 ✅ //具体查看project-root\docs\context-optimization.md
- [ ] 图片/文件解析识别 //这个和agent一样,好像会用到py但我还没学,就先不弄了,到时候有时间我会补上
- [ ] ReAct模式深度搜索
- [x] 回答引用信息展示 ✅

## 快速开始

### 环境要求

- Windows 10/11
- Visual Studio 2022
- MySQL 8.0
- Boost 1.87+
- OpenSSL 3.0+（用于HTTPS连接）

### 1. 数据库配置

```sql
-- 创建数据库
CREATE DATABASE deepsleep CHARACTER SET utf8mb4;
```

执行初始化脚本：

```bash
cd project-root/docs/sql
init_database.bat
```

或手动在 MySQL 中执行：

```sql
SOURCE path/to/schema.sql;
```

### 2. 修改数据库配置

编辑 `backend/Database.cpp` 中的连接信息：

```cpp
const std::string DB_HOST = "127.0.0.1";
const std::string DB_USER = "root";
const std::string DB_PASS = "your_password";
const std::string DB_NAME = "deepsleep";
```

### 3. 编译项目

```bash
# 使用 Visual Studio
msbuild DeepSleep.sln /p:Configuration=Debug /p:Platform=x64
```

### 4. 启动服务器

```bash
x64\Debug\DeepSleep.exe
```

### 5. 访问应用

打开浏览器访问：`http://localhost:8081`

### 6. 手机访问（同一WiFi）

获取电脑IP地址后访问：`http://192.168.x.x:8081`

## 配置 AI 模型

### LM Studio（本地模型）

1. 下载并安装 [LM Studio](https://lmstudio.ai/)
2. 启动 LM Studio 并加载模型
3. 在应用中添加提供商：
   - 类型：`lmstudio`
   - 基础URL：`http://localhost:1234`

### DeepSeek API

1. 注册 [DeepSeek](https://platform.deepseek.com/)
2. 创建 API Key
3. 添加提供商：
   - 类型：`openai`
   - 基础URL：`https://api.deepseek.com/v1`
   - 模型名称：`deepseek-chat`

### 豆包 API

1. 注册 [火山引擎](https://console.volcengine.com/)
2. 获取 Endpoint ID
3. 添加提供商：
   - 类型：`openai`
   - 基础URL：`https://ark.cn-beijing.volces.com/api/v3`
   - 模型名称：你的端点ID

### Kimi API

1. 注册 [Moonshot](https://platform.moonshot.cn/)
2. 创建 API Key
3. 添加提供商：
   - 类型：`openai`
   - 基础URL：`https://api.moonshot.cn/v1`
   - 模型名称：`kimi-k2.6` 或 `moonshot-v1-8k`

## 目录结构

```
DeepSleep/
├── backend/                    # 后端源码
│   ├── main.cpp               # 程序入口
│   ├── Server.cpp/h           # WebSocket 服务器
│   ├── Connection.cpp/h       # 连接管理
│   ├── ConnectionMgr.cpp/h    # 连接池
│   ├── Logic_System.cpp/h     # 业务逻辑
│   ├── LogicNode.h            # 逻辑节点
│   ├── Database.cpp/h         # 数据库操作
│   ├── AIModel.h              # AI 模型接口
│   ├── OpenAIModel.cpp/h      # OpenAI 兼容模型实现
│   ├── AIModelMgr.cpp/h       # 模型管理器
│   ├── SearchTool.h           # 联网搜索工具（DuckDuckGo）
│   ├── const.h                # 常量定义
│   └── Singleton.h            # 单例模板
├── frontend/                   # 前端源码
│   ├── index.html             # 主页面
│   ├── model-config.html      # 模型配置页面
│   ├── prompt-config.html     # 提示词配置页面
│   ├── styles.css             # 样式文件
│   ├── app.js                 # 主逻辑
│   └── websocket.js           # WebSocket 客户端
├── docs/                       # 项目文档
│   ├── api/                   # 接口文档
│   │   └── api.md             # API 接口说明
│   └── sql/                   # 数据库脚本
│       ├── schema.sql         # 建表 SQL
│       └── init_database.bat  # 初始化脚本
└── DeepSleep.sln              # Visual Studio 解决方案
```

## 数据库表结构

### users - 用户表

| 字段             | 类型           | 说明     |
| -------------- | ------------ | ------ |
| id             | INT          | 主键，自增  |
| username       | VARCHAR(50)  | 用户名，唯一 |
| password\_hash | VARCHAR(255) | 密码哈希   |
| created\_at    | TIMESTAMP    | 创建时间   |

### conversations - 会话表

| 字段          | 类型           | 说明      |
| ----------- | ------------ | ------- |
| id          | INT          | 主键，自增   |
| user\_id    | INT          | 用户ID，外键 |
| title       | VARCHAR(255) | 会话标题    |
| created\_at | TIMESTAMP    | 创建时间    |
| updated\_at | TIMESTAMP    | 更新时间    |

### messages - 消息表

| 字段               | 类型           | 说明                        |
| ---------------- | ------------ | ------------------------- |
| id               | INT          | 主键，自增                     |
| conversation\_id | INT          | 会话ID，外键                   |
| role             | ENUM         | 角色（user/assistant/system） |
| content          | TEXT         | 消息内容                      |
| model\_name      | VARCHAR(100) | AI模型名称                    |
| references       | TEXT         | 引用信息（联网搜索来源）              |
| created\_at      | TIMESTAMP    | 创建时间                      |

### model\_providers - 模型提供商表

| 字段          | 类型           | 说明                   |
| ----------- | ------------ | -------------------- |
| id          | INT          | 主键，自增                |
| user\_id    | INT          | 用户ID，外键              |
| name        | VARCHAR(50)  | 提供商名称                |
| type        | VARCHAR(20)  | 类型（openai/lmstudio等） |
| api\_key    | VARCHAR(255) | API密钥                |
| base\_url   | VARCHAR(255) | 基础URL                |
| model\_name | VARCHAR(100) | 模型名称                 |
| is\_active  | BOOLEAN      | 是否启用                 |
| created\_at | TIMESTAMP    | 创建时间                 |

### system\_prompts - 系统提示词表

| 字段          | 类型           | 说明      |
| ----------- | ------------ | ------- |
| id          | INT          | 主键，自增   |
| user\_id    | INT          | 用户ID，外键 |
| name        | VARCHAR(100) | 提示词名称   |
| content     | TEXT         | 提示词内容   |
| is\_default | BOOLEAN      | 是否为默认   |
| created\_at | TIMESTAMP    | 创建时间    |

