#pragma once
#define HEAD_DATA_LENGTH 2
#define HEAD_ID_LENGTH 2
#define HEAD_TOTAL_LENGTH 4
#define MAX_LENGTH 1024 * 2
#define MAX_RECVQUE 1e4
#define MAX_SENDQUE 1e3

// 消息ID枚举
enum MSG_ID
{
	MSG_HELLO_WORLD = 1001,

	MSG_CHAT_REQUEST = 2001,   // 用户发送消息
	MSG_CHAT_RESPONSE = 2002,  // AI回复（完整）
	MSG_CHAT_STREAM_START = 2010,  // 流式输出开始
	MSG_CHAT_STREAM_DATA = 2011,   // 流式输出数据
	MSG_CHAT_STREAM_END = 2012,    // 流式输出结束
	MSG_LOGIN_REQUEST = 2003,  // 登录请求
	MSG_LOGIN_RESPONSE = 2004, // 登录响应
	MSG_REGISTER_REQUEST = 2005,  // 注册请求
	MSG_REGISTER_RESPONSE = 2006, // 注册响应
	MSG_CONVERSATION_LIST = 2007, // 获取会话列表
	MSG_CREATE_CONVERSATION = 2008, // 创建新会话
	MSG_HISTORY_REQUEST = 2009,  // 获取历史消息
	
	// 模型配置相关
	MSG_GET_PROVIDERS = 2013,      // 获取模型提供商列表
	MSG_PROVIDERS_RESPONSE = 2014, // 提供商列表响应
	MSG_ADD_PROVIDER = 2015,       // 添加提供商
	MSG_UPDATE_PROVIDER = 2016,    // 更新提供商
	MSG_DELETE_PROVIDER = 2017,    // 删除提供商
	MSG_GET_PROVIDER_MODELS = 2018,  // 获取提供商的模型
	MSG_PROVIDER_MODELS_RESPONSE = 2019, // 模型列表响应
	MSG_ADD_PROVIDER_MODEL = 2020,   // 添加模型
	MSG_DELETE_PROVIDER_MODEL = 2021, // 删除模型
	MSG_SET_DEFAULT_MODEL = 2022,    // 设置默认模型
	MSG_GET_MODELS = 2023,           // 获取可用模型列表
	MSG_MODELS_RESPONSE = 2024,      // 可用模型列表响应
	
	// 系统提示词相关
	MSG_GET_PROMPTS = 2025,          // 获取系统提示词列表
	MSG_PROMPTS_RESPONSE = 2026,     // 提示词列表响应
	MSG_ADD_PROMPT = 2027,           // 添加系统提示词
	MSG_UPDATE_PROMPT = 2028,        // 更新系统提示词
	MSG_DELETE_PROMPT = 2029,        // 删除系统提示词
	MSG_SET_DEFAULT_PROMPT = 2030,   // 设置默认提示词
	
	// 会话删除
	MSG_DELETE_CONVERSATION = 2031,  // 删除会话
	
	// 模型测试
	MSG_TEST_MODEL = 2032,           // 测试模型连接
	MSG_TEST_MODEL_RESPONSE = 2033,  // 测试模型响应
};
