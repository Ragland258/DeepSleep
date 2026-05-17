# DeepSleep 系统架构文档

## 1. 系统架构图

```mermaid
flowchart TB
    subgraph Client["客户端 (浏览器)"]
        WS_Client["WebSocket Client<br/>发送JSON消息"]
    end

    subgraph Server["Server (Boost.Asio)"]
        subgraph Connection["Connection.cpp"]
            Accept["async_accept<br/>接收连接"]
            Read["async_read<br/>读取WebSocket数据"]
            Parse["JSON.parse<br/>解析消息"]
            LogicNode["创建LogicNode<br/>投递到队列"]
            Send["AsyncSend<br/>发送响应"]
        end

        subgraph LogicSystem["LogicSystem.cpp"]
            DealMsg["DealMsg<br/>工作线程处理"]

            subgraph Callbacks["回调函数"]
                OnChat["OnChatRequest<br/>聊天请求"]
                OnLogin["OnLoginRequest<br/>登录"]
                OnRegister["OnRegisterRequest<br/>注册"]
                OnConv["OnGetConversationList<br/>获取会话"]
                OnHistory["OnGetHistoryRequest<br/>获取历史"]
                OnProvider["OnGetProviders<br/>获取模型"]
                OnTest["OnTestModel<br/>测试模型"]
                OnPrompt["OnGetPrompts<br/>获取提示词"]
                OnDelConv["OnDeleteConversation<br/>删除会话"]
            end

            DealMsg --> |"根据msgId"| Callbacks
        end

        subgraph Database["Database.cpp"]
            DB["MySQL<br/>增删改查"]
        end

        subgraph AIModel["AIModel"]
            AIM["AIModelManager<br/>模型管理"]
            Search["SearchTool<br/>联网搜索"]
        end
    end

    WS_Client --> |"WebSocket"| Read
    Read --> Parse
    Parse --> LogicNode
    LogicNode --> DealMsg

    OnChat --> |"保存消息"| DB
    OnChat --> AIM
    OnChat --> Search
    Search --> |"搜索结果"| OnChat
    AIM --> |"AI回复"| OnChat
    OnChat --> Send
    OnLogin --> DB
    OnRegister --> DB
    OnConv --> DB
    OnHistory --> DB
    OnProvider --> DB
    OnPrompt --> DB
    Send --> WS_Client
```

---

## 2. 详细时序图

```mermaid
sequenceDiagram
    participant Browser as 浏览器
    participant Server as Server.cpp
    participant Logic as LogicSystem
    participant DB as Database
    participant AIM as AIModel

    Browser->>Server: WebSocket连接
    Server-->>Browser: 握手成功

    Browser->>Server: {"id": 2001, "data": {...}}

    rect rgb(240, 248, 255)
        Note over Server: Connection::Start() 异步读取
    end

    Server->>Server: JSON.parse()
    Server->>Logic: PostMsgToQueue(LogicNode)

    rect rgb(255, 250, 240)
        Note over Logic: LogicSystem 工作线程
        Logic->Logic: DealMsg()
        Logic->Logic: 根据msgId路由
    end

    alt 聊天请求 (id=2001)
        Logic->DB: AddMessage()
        Logic->DB: GetConversationMessages()
        Logic->Logic: 动态上下文裁剪
        Logic->AIM: GenerateStream()
        AIM-->>Logic: 流式回复
        Logic-->>Server: 流式数据
        Server-->>Browser: {"id": 2011, "data": {"chunk": "..."}}
        Logic->DB: AddMessage() 保存回复
    else 登录请求 (id=2003)
        Logic->DB: VerifyUser()
        Logic-->>Server: {"id": 2004, "data": {"success": true}}
    else 其他请求
        Logic->DB: 对应操作
        Logic-->>Server: 响应
    end

    Server-->>Browser: WebSocket响应
```

---

## 3. OnLoginRequest 登录请求流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>username, password"]
    Parse --> Validate{"参数验证"}
    Validate -->|"空"| Error1["返回错误<br/>用户名不能为空"]
    Validate -->|"空"| Error2["返回错误<br/>密码不能为空"]
    Validate -->|"通过"| Query["查询用户<br/>Database::VerifyUser"]
    Query --> CheckResult{"用户存在?"}
    CheckResult -->|"否"| Error3["返回错误<br/>用户不存在"]
    CheckResult -->|"是"| CheckPwd{"密码正确?"}
    CheckPwd -->|"否"| Error4["返回错误<br/>密码错误"]
    CheckPwd -->|"是"| Success["返回成功<br/>LOGIN_RESPONSE"]
    Success --> End([结束])
    Error1 --> End
    Error2 --> End
    Error3 --> End
    Error4 --> End
```

---

## 4. OnRegisterRequest 注册请求流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>username, password"]
    Parse --> Validate{"参数验证"}
    Validate -->|"用户名为空"| Error1["返回错误<br/>用户名不能为空"]
    Validate -->|"密码为空"| Error2["返回错误<br/>密码不能为空"]
    Validate -->|"通过"| CheckUser["检查用户名是否存在<br/>Database::GetUserByUsername"]
    CheckUser --> Exists{"用户名已存在?"}
    Exists -->|"是"| Error3["返回错误<br/>用户名已存在"]
    Exists -->|"否"| Hash["密码加密<br/>bcrypt"]
    Hash --> Create["创建用户<br/>Database::CreateUser"]
    Create --> Success["返回成功<br/>REGISTER_RESPONSE"]
    Success --> End([结束])
    Error1 --> End
    Error2 --> End
    Error3 --> End
```

---

## 5. OnGetConversationList 获取会话列表流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>user_id"]
    Parse --> Query["查询用户会话列表<br/>Database::GetUserConversations"]
    Query --> Sort["按更新时间排序"]
    Sort --> Build["构建会话列表JSON"]
    Build --> Send["发送响应<br/>CONVERSATION_LIST_RESPONSE"]
    Send --> End([结束])
```

---

## 6. OnCreateConversation 创建会话流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>user_id, title"]
    Parse --> Check{"title是否为空?"}
    Check -->|"是"| AutoTitle["自动生成标题<br/>取首条消息前20字符"]
    Check -->|"否"| UseTitle["使用指定标题"]
    AutoTitle --> Create["创建会话<br/>Database::CreateConversation"]
    UseTitle --> Create
    Create --> Success["返回成功<br/>CREATE_CONVERSATION_RESPONSE<br/>包含新会话ID"]
    Success --> End([结束])
```

---

## 7. OnGetHistoryRequest 获取历史消息流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>conversation_id"]
    Parse --> Query["查询会话消息<br/>Database::GetConversationMessages"]
    Query --> Check{"会话存在?"}
    Check -->|"否"| Error["返回错误<br/>会话不存在"]
    Check -->|"是"| Build["构建消息列表JSON"]
    Build --> Send["发送响应<br/>HISTORY_RESPONSE"]
    Send --> End([结束])
    Error --> End
```

---

## 8. OnGetProviders 获取模型提供商流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>user_id"]
    Parse --> Query["查询用户模型配置<br/>Database::GetModelProviders"]
    Query --> Build["构建提供商列表JSON"]
    Build --> Send["发送响应<br/>PROVIDERS_RESPONSE"]
    Send --> End([结束])
```

---

## 9. OnAddProvider 添加模型提供商流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>user_id, name, type,<br/>api_key, base_url, model_name"]
    Parse --> Validate{"参数验证"}
    Validate -->|"失败"| Error["返回错误<br/>参数不完整"]
    Validate -->|"通过"| Check["检查名称是否重复"]
    Check --> Exists{"名称已存在?"}
    Exists -->|"是"| Error2["返回错误<br/>名称已存在"]
    Exists -->|"否"| Insert["插入记录<br/>Database::AddModelProvider"]
    Insert --> Success["返回成功<br/>ADD_PROVIDER_RESPONSE"]
    Success --> End([结束])
    Error --> End
    Error2 --> End
```

---

## 10. OnDeleteProvider 删除模型提供商流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>provider_id, user_id"]
    Parse --> Check["验证provider属于该用户"]
    Check --> Valid{"验证通过?"}
    Valid -->|"否"| Error["返回错误<br/>无权删除"]
    Valid -->|"是"| Delete["删除记录<br/>Database::DeleteModelProvider"]
    Delete --> Success["返回成功<br/>DELETE_PROVIDER_RESPONSE"]
    Success --> End([结束])
    Error --> End
```

---

## 11. OnTestModel 测试模型连接流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>provider_id, user_id"]
    Parse --> Load["加载模型配置<br/>LoadModelsFromDatabase"]
    Load --> Find["查找指定provider"]
    Find --> Found{"找到?"}
    Found -->|"否"| Error["返回错误<br/>提供商不存在"]
    Found -->|"是"| Create["创建模型实例<br/>AIModelManager::CreateModel"]
    Create --> Test["发送测试请求<br/>GenerateStream"]
    Test --> Check{"连接成功?"}
    Check -->|"是"| Success["返回成功<br/>TEST_MODEL_RESPONSE<br/>测试通过"]
    Check -->|"否"| Fail["返回失败<br/>TEST_MODEL_RESPONSE<br/>错误信息"]
    Success --> End([结束])
    Fail --> End
    Error --> End
```

---

## 12. OnGetPrompts 获取提示词流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>user_id"]
    Parse --> Query["查询用户提示词<br/>Database::GetSystemPrompts"]
    Query --> Build["构建提示词列表JSON"]
    Build --> Send["发送响应<br/>PROMPTS_RESPONSE"]
    Send --> End([结束])
```

---

## 13. OnAddPrompt 添加提示词流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>user_id, name, content"]
    Parse --> Validate{"参数验证"}
    Validate -->|"失败"| Error["返回错误<br/>参数不完整"]
    Validate -->|"通过"| Insert["插入记录<br/>Database::AddSystemPrompt"]
    Insert --> CheckDefault{"设置为默认?"}
    CheckDefault -->|"是"| SetDefault["更新默认标记<br/>Database::SetDefaultPrompt"]
    CheckDefault -->|"否"| Success["返回成功<br/>ADD_PROMPT_RESPONSE"]
    SetDefault --> Success2["返回成功<br/>ADD_PROMPT_RESPONSE"]
    Success --> End([结束])
    Success2 --> End
    Error --> End
```

---

## 14. OnDeletePrompt 删除提示词流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>prompt_id, user_id"]
    Parse --> Check["验证prompt属于该用户"]
    Check --> Valid{"验证通过?"}
    Valid -->|"否"| Error["返回错误<br/>无权删除"]
    Valid -->|"是"| CheckLast{"是最后一个默认?"}
    CheckLast -->|"是"| Prevent["返回错误<br/>至少保留一个默认提示词"]
    CheckLast -->|"否"| Delete["删除记录<br/>Database::DeleteSystemPrompt"]
    Delete --> Success["返回成功<br/>DELETE_PROMPT_RESPONSE"]
    Success --> End([结束])
    Error --> End
    Prevent --> End
```

---

## 15. OnDeleteConversation 删除会话流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>conversation_id, user_id"]
    Parse --> Check["验证会话属于该用户"]
    Check --> Valid{"验证通过?"}
    Valid -->|"否"| Error["返回错误<br/>无权删除"]
    Valid -->|"是"| DeleteMsg["删除会话消息<br/>Database::DeleteConversationMessages"]
    DeleteMsg --> DeleteConv["删除会话<br/>Database::DeleteConversation"]
    DeleteConv --> Success["返回成功<br/>DELETE_CONVERSATION_RESPONSE"]
    Success --> End([结束])
    Error --> End
```

---

## 16. OnChatRequest 聊天请求流程

```mermaid
flowchart TD
    Start([开始]) --> Parse["解析请求参数<br/>message, conversation_id, user_id,<br/>provider_name, enable_search"]

    Parse --> SaveUserMsg["保存用户消息到数据库"]
    SaveUserMsg --> LoadModels["加载用户模型配置<br/>LoadModelsFromDatabase"]

    LoadModels --> SelectModel{"选择模型"}

    SelectModel -->|"指定模型"| UseProvider["使用指定模型"]
    SelectModel -->|"无指定"| UseDefault["使用默认模型"]

    UseProvider --> SendStreamStart["发送流式开始标记<br/>MSG_CHAT_STREAM_START"]
    UseDefault --> SendStreamStart

    SendStreamStart --> CheckSearch{"enable_search?"}

    CheckSearch -->|"是"| DoSearch["执行联网搜索<br/>SearchTool.Search"]
    CheckSearch -->|"否"| NoSearch["跳过搜索"]

    DoSearch -->|"有结果"| BuildSearchPrompt["构建搜索提示词<br/>强制要求基于搜索结果回答"]
    DoSearch -->|"无结果"| UseDefaultPrompt["使用默认提示词"]

    BuildSearchPrompt --> GetHistory["获取历史消息"]
    UseDefaultPrompt --> GetHistory
    NoSearch --> GetHistory

    GetHistory --> DynamicLimit{"动态上下文限制"}

    DynamicLimit -->|"total > 20000"| Limit2["保留2条"]
    DynamicLimit -->|"total > 12000"| Limit4["保留4条"]
    DynamicLimit -->|"total > 8000"| Limit6["保留6条"]
    DynamicLimit -->|"total > 5000"| Limit8["保留8条"]
    DynamicLimit -->|"其他"| Limit10["保留10条"]

    Limit2 --> BuildMessages["构建消息列表"]
    Limit4 --> BuildMessages
    Limit6 --> BuildMessages
    Limit8 --> BuildMessages
    Limit10 --> BuildMessages

    BuildMessages --> CallModel["调用模型生成回复<br/>GenerateStream"]

    CallModel --> StreamChunk{"逐字接收"}

    StreamChunk -->|"有数据"| SendChunk["发送流式数据<br/>MSG_CHAT_STREAM_DATA"]
    SendChunk --> StreamChunk

    StreamChunk -->|"完成"| SaveReply["保存回复到数据库"]
    SaveReply --> SendEnd["发送流式结束标记<br/>MSG_CHAT_STREAM_END"]
    SendEnd --> End([结束])

    CallModel -->|"异常"| ErrorReply["发送错误信息"]
    ErrorReply --> SendEnd
```

---

## 17. 模块职责表

| 模块 | 文件 | 职责 |
|------|------|------|
| **Server** | Server.cpp | 启动服务，监听端口，建立连接 |
| **Connection** | Connection.cpp | WebSocket读写，JSON解析，投递LogicNode，发送响应 |
| **LogicSystem** | Logic_System.cpp | 消息队列，消费线程，路由到各回调函数 |
| **Database** | Database.cpp | MySQL操作（用户、会话、消息、模型配置） |
| **AIModel** | OpenAIModel.cpp | 调用大模型API，流式返回 |
| **SearchTool** | SearchTool.cpp | DuckDuckGo联网搜索 |

---

## 6. 消息ID对照表

### 客户端 → 服务器

| 消息ID | 名称 | 说明 |
|--------|------|------|
| 2001 | CHAT_REQUEST | 发送聊天消息 |
| 2003 | LOGIN_REQUEST | 登录请求 |
| 2005 | REGISTER_REQUEST | 注册请求 |
| 2007 | CONVERSATION_LIST | 获取会话列表 |
| 2008 | CREATE_CONVERSATION | 创建会话 |
| 2009 | HISTORY_REQUEST | 获取历史消息 |
| 2013 | GET_PROVIDERS | 获取模型提供商 |
| 2015 | ADD_PROVIDER | 添加提供商 |
| 2017 | DELETE_PROVIDER | 删除提供商 |
| 2025 | GET_PROMPTS | 获取系统提示词 |
| 2027 | ADD_PROMPT | 添加提示词 |
| 2029 | DELETE_PROMPT | 删除提示词 |
| 2030 | SET_DEFAULT_PROMPT | 设置默认提示词 |
| 2031 | DELETE_CONVERSATION | 删除会话 |
| 2032 | TEST_MODEL | 测试模型连接 |

### 服务器 → 客户端

| 消息ID | 名称 | 说明 |
|--------|------|------|
| 2002 | CHAT_RESPONSE | AI回复（完整） |
| 2004 | LOGIN_RESPONSE | 登录响应 |
| 2006 | REGISTER_RESPONSE | 注册响应 |
| 2010 | CHAT_STREAM_START | 流式输出开始 |
| 2011 | CHAT_STREAM_DATA | 流式数据 |
| 2012 | CHAT_STREAM_END | 流式输出结束 |
| 2014 | PROVIDERS_RESPONSE | 提供商列表 |
| 2026 | PROMPTS_RESPONSE | 提示词列表 |
| 2033 | TEST_MODEL_RESPONSE | 测试模型响应 |

---

## 7. 数据库ER图

```mermaid
erDiagram
    users ||--o{ conversations : has
    users ||--o{ model_providers : has
    users ||--o{ system_prompts : has
    conversations ||--o{ messages : has

    users {
        int id PK
        string username
        string password_hash
        timestamp created_at
    }

    conversations {
        int id PK
        int user_id FK
        string title
        timestamp created_at
        timestamp updated_at
    }

    messages {
        int id PK
        int conversation_id FK
        enum role
        text content
        string model_name
        text references
        timestamp created_at
    }

    model_providers {
        int id PK
        int user_id FK
        string name
        string type
        string api_key
        string base_url
        string model_name
        boolean is_active
        timestamp created_at
    }

    system_prompts {
        int id PK
        int user_id FK
        string name
        text content
        boolean is_default
        timestamp created_at
    }
```

---

## 20. 技术栈

### 后端
- **语言**: C++14
- **网络**: Boost.Asio + Boost.Beast (WebSocket/HTTP)
- **数据库**: MySQL 8.0
- **JSON**: jsoncpp

### 前端
- **HTML5 + CSS3 + JavaScript**
- **WebSocket** 实时通信
- **响应式设计**