# DeepSleep AI Chat Server

自托管 AI 聊天应用，C++ 后端 + 原生 HTML/CSS/JS 前端。

## 特性

- **C++ 高性能后端**：基于 Boost.Beast/Asio，单机支持高并发
- **Actor 并发模型**：4 个 DB 线程 + 8 个 AI 线程，异步处理请求
- **流式输出**：实时逐字显示 AI 回复
- **多模型支持**：DeepSeek、豆包、Kimi、OpenAI、LM Studio、Ollama 等
- **联网搜索**：集成搜索工具，AI 可引用网络结果回答
- **Redis 缓存**：会话管理、消息缓存、系统提示词缓存
- **MySQL 持久化**：用户数据、会话历史、模型配置
- **心跳检测**：WebSocket 心跳保活，断线自动重连
- **响应式前端**：支持 PC 和移动端

## 快速开始

### 环境要求

- Linux (CentOS/RHEL/Ubuntu)
- MySQL 5.7+
- Redis 6.0+
- GCC 7+ (支持 C++17)
- Boost 1.66+
- jsoncpp, hiredis, openssl

### 编译

```bash
cd backend
make -j4
```

### 配置

修改 `main.cpp` 中的数据库连接：

```cpp
Database::GetInstance()->Init("127.0.0.1", "root", "password", "deepsleep");
RedisPool::GetInstance().Init("127.0.0.1", 6379, "password", 10);
```

### 运行

```bash
./deepsleep
```

服务启动后访问 `http://localhost:8081`

## 前端功能

- 用户注册/登录（支持邮箱验证码）
- 多会话管理
- 模型配置（添加自定义 AI 提供商）
- 系统提示词自定义
- 联网搜索开关
- 流式输出显示

## 目录结构

```
deepsleep/
├── backend/
│   ├── main.cpp              # 入口
│   ├── Server.cpp/h          # TCP 服务器
│   ├── Connection.cpp/h      # HTTP + WebSocket
│   ├── UserActor.cpp/h       # 业务逻辑
│   ├── AIWorker.cpp/h        # AI 线程
│   ├── Database.cpp/h        # MySQL 操作
│   ├── RedisClient.h         # Redis 客户端
│   ├── OpenAIModel.cpp/h     # OpenAI 兼容 API
│   ├── SearchTool.cpp/h      # 联网搜索
│   └── Makefile
├── frontend/
│   ├── index.html            # 主页面
│   ├── app.js                # 应用逻辑
│   ├── websocket.js          # WebSocket 管理
│   ├── styles.css            # 样式
│   └── model-config.html     # 模型配置
├── ARCHITECTURE.md           # 架构文档
└── README.md
```

## 许可证

MIT License
