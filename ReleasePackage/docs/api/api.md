# DeepSleep API 接口文档

## 概述

DeepSleep 采用 WebSocket 协议进行通信，支持实时双向数据传输。所有消息采用 JSON 格式。

## 消息格式

### 请求消息格式
```json
{
  "id": 消息ID,
  "data": {
    // 业务数据
  }
}
```

### 响应消息格式
```json
{
  "id": 消息ID,
  "data": {
    // 响应数据
  }
}
```

## 通用返回结构

### 成功响应
```json
{
  "id": 对应的响应消息ID,
  "data": {
    "success": true,
    "message": "操作成功"
  }
}
```

### 失败响应
```json
{
  "id": 对应的响应消息ID,
  "data": {
    "success": false,
    "message": "错误描述"
  }
}
```

---

## 1. 用户认证

### 1.1 用户注册

**请求**
```json
{
  "id": 2005,
  "data": {
    "username": "用户名",
    "password": "密码"
  }
}
```

**响应**
```json
{
  "id": 2006,
  "data": {
    "success": true,
    "user_id": 1,
    "message": "注册成功"
  }
}
```

### 1.2 用户登录

**请求**
```json
{
  "id": 2003,
  "data": {
    "username": "用户名",
    "password": "密码"
  }
}
```

**响应**
```json
{
  "id": 2004,
  "data": {
    "success": true,
    "user_id": 1,
    "message": "登录成功"
  }
}
```

---

## 2. 会话管理

### 2.1 获取会话列表

**请求**
```json
{
  "id": 2007,
  "data": {
    "user_id": 1
  }
}
```

**响应**
```json
{
  "id": 2007,
  "data": [
    {
      "id": 1,
      "title": "会话标题",
      "created_at": "2024-01-01 12:00:00",
      "updated_at": "2024-01-01 12:00:00"
    }
  ]
}
```

### 2.2 创建新会话

**请求**
```json
{
  "id": 2008,
  "data": {
    "user_id": 1,
    "title": "新对话"
  }
}
```

**响应**
```json
{
  "id": 2008,
  "data": {
    "success": true,
    "conversation_id": 1
  }
}
```

### 2.3 删除会话

**请求**
```json
{
  "id": 2031,
  "data": {
    "conversation_id": 1
  }
}
```

**响应**
```json
{
  "id": 2031,
  "data": {
    "success": true,
    "conversation_id": 1
  }
}
```

---

## 3. 消息处理

### 3.1 发送聊天消息（流式）

**请求**
```json
{
  "id": 2001,
  "data": {
    "message": "用户输入的消息",
    "conversation_id": 1,
    "user_id": 1,
    "mode": "fast",
    "provider_name": "DeepSeek",
    "enable_search": false
  }
}
```

**参数说明**
| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| message | string | 是 | 用户输入的消息 |
| conversation_id | int | 是 | 会话ID |
| user_id | int | 是 | 用户ID |
| mode | string | 否 | 模式：fast/expert |
| provider_name | string | 否 | 模型提供商名称 |
| enable_search | bool | 否 | 是否启用联网搜索（默认false） |

**流式响应 - 开始**
```json
{
  "id": 2010,
  "data": {
    "conversation_id": 1,
    "model_name": "deepseek-chat"
  }
}
```

**流式响应 - 数据**
```json
{
  "id": 2011,
  "data": {
    "chunk": "AI回复的",
    "conversation_id": 1
  }
}
```

**流式响应 - 结束（无联网搜索）**
```json
{
  "id": 2012,
  "data": {
    "conversation_id": 1,
    "full_message": "完整的AI回复内容"
  }
}
```

**流式响应 - 结束（启用联网搜索）**
```json
{
  "id": 2012,
  "data": {
    "conversation_id": 1,
    "full_message": "完整的AI回复内容",
    "has_references": true,
    "references_json": [
      {
        "title": "来源标题1",
        "snippet": "来源摘要1",
        "url": "https://example.com/article1"
      },
      {
        "title": "来源标题2",
        "snippet": "来源摘要2",
        "url": "https://example.com/article2"
      }
    ]
  }
}
```

### 3.2 获取历史消息

**请求**
```json
{
  "id": 2009,
  "data": {
    "conversation_id": 1
  }
}
```

**响应**
```json
{
  "id": 2009,
  "data": [
    {
      "id": 1,
      "role": "user",
      "content": "用户消息",
      "created_at": "2024-01-01 12:00:00"
    },
    {
      "id": 2,
      "role": "assistant",
      "content": "AI回复",
      "model_name": "deepseek-chat",
      "references": "[{\"title\":\"来源1\",\"snippet\":\"摘要\",\"url\":\"url\"}]",
      "created_at": "2024-01-01 12:00:01"
    }
  ]
}
```

---

## 4. 模型配置

### 4.1 获取模型提供商列表

**请求**
```json
{
  "id": 2013,
  "data": {
    "user_id": 1
  }
}
```

**响应**
```json
{
  "id": 2014,
  "data": [
    {
      "id": 1,
      "name": "DeepSeek",
      "type": "openai",
      "api_key": "sk-xxx",
      "base_url": "https://api.deepseek.com/v1",
      "model_name": "deepseek-chat",
      "is_active": true,
      "created_at": "2024-01-01 12:00:00"
    }
  ]
}
```

### 4.2 添加模型提供商

**请求**
```json
{
  "id": 2015,
  "data": {
    "user_id": 1,
    "name": "DeepSeek",
    "type": "openai",
    "api_key": "sk-xxx",
    "base_url": "https://api.deepseek.com/v1",
    "model_name": "deepseek-chat"
  }
}
```

**响应**
```json
{
  "id": 2015,
  "data": {
    "success": true,
    "provider_id": 1
  }
}
```

### 4.3 删除模型提供商

**请求**
```json
{
  "id": 2017,
  "data": {
    "provider_id": 1
  }
}
```

**响应**
```json
{
  "id": 2017,
  "data": {
    "success": true
  }
}
```

### 4.4 测试模型连接

**请求**
```json
{
  "id": 2032,
  "data": {
    "user_id": 1,
    "provider_name": "DeepSeek"
  }
}
```

**响应**
```json
{
  "id": 2033,
  "data": {
    "success": true,
    "provider_name": "DeepSeek",
    "message": "连接成功",
    "response": "OK"
  }
}
```

---

## 5. 系统提示词

### 5.1 获取提示词列表

**请求**
```json
{
  "id": 2025,
  "data": {
    "user_id": 1
  }
}
```

**响应**
```json
{
  "id": 2026,
  "data": [
    {
      "id": 1,
      "name": "通用助手",
      "content": "你是一个有用的AI助手...",
      "is_default": true,
      "created_at": "2024-01-01 12:00:00"
    }
  ]
}
```

### 5.2 添加提示词

**请求**
```json
{
  "id": 2027,
  "data": {
    "user_id": 1,
    "name": "我的提示词",
    "content": "你是一个专业的...",
    "is_default": false
  }
}
```

**响应**
```json
{
  "id": 2027,
  "data": {
    "success": true,
    "prompt_id": 1
  }
}
```

### 5.3 删除提示词

**请求**
```json
{
  "id": 2029,
  "data": {
    "prompt_id": 1
  }
}
```

**响应**
```json
{
  "id": 2029,
  "data": {
    "success": true
  }
}
```

### 5.4 设置默认提示词

**请求**
```json
{
  "id": 2030,
  "data": {
    "prompt_id": 1,
    "name": "提示词名称",
    "content": "提示词内容"
  }
}
```

**响应**
```json
{
  "id": 2030,
  "data": {
    "success": true
  }
}
```

---

## 消息ID对照表

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