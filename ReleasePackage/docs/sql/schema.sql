-- DeepSleep 数据库建表脚本
-- 编码: UTF-8

-- 创建数据库
CREATE DATABASE IF NOT EXISTS deepsleep CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE deepsleep;

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) UNIQUE NOT NULL COMMENT '用户名',
    password_hash VARCHAR(255) NOT NULL COMMENT '密码哈希',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户表';

-- 会话表
CREATE TABLE IF NOT EXISTS conversations (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL COMMENT '用户ID',
    title VARCHAR(255) DEFAULT '新对话' COMMENT '会话标题',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_user_id (user_id),
    INDEX idx_updated_at (updated_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='会话表';

-- 消息表
CREATE TABLE IF NOT EXISTS messages (
    id INT PRIMARY KEY AUTO_INCREMENT,
    conversation_id INT NOT NULL COMMENT '会话ID',
    role ENUM('user', 'assistant', 'system') NOT NULL COMMENT '角色',
    content TEXT NOT NULL COMMENT '消息内容',
    model_name VARCHAR(100) DEFAULT NULL COMMENT 'AI模型名称',
    `references` TEXT DEFAULT NULL COMMENT '引用信息(联网搜索来源)',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE,
    INDEX idx_conversation_id (conversation_id),
    INDEX idx_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='消息表';

-- 模型提供商表
CREATE TABLE IF NOT EXISTS model_providers (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL COMMENT '用户ID',
    name VARCHAR(50) NOT NULL COMMENT '提供商名称',
    type VARCHAR(20) NOT NULL COMMENT '类型(openai/lmstudio/ollama等)',
    api_key VARCHAR(255) DEFAULT '' COMMENT 'API密钥',
    base_url VARCHAR(255) NOT NULL COMMENT 'API基础URL',
    model_name VARCHAR(100) NOT NULL COMMENT '模型名称',
    is_active BOOLEAN DEFAULT TRUE COMMENT '是否启用',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_user_id (user_id),
    INDEX idx_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='模型提供商表';

-- 系统提示词表
CREATE TABLE IF NOT EXISTS system_prompts (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL COMMENT '用户ID',
    name VARCHAR(100) NOT NULL COMMENT '提示词名称',
    content TEXT NOT NULL COMMENT '提示词内容',
    is_default BOOLEAN DEFAULT FALSE COMMENT '是否为默认',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    INDEX idx_user_id (user_id),
    INDEX idx_is_default (is_default)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='系统提示词表';

-- 初始化默认系统提示词
INSERT INTO system_prompts (user_id, name, content, is_default) VALUES
(0, '通用助手', '你是一个有用、友好且智慧的AI助手。请用中文回答用户的问题，保持回答清晰、准确且有帮助。', TRUE),
(0, '编程专家', '你是一个专业的编程助手，精通多种编程语言和框架。请帮助用户解决编程问题，提供高质量的代码示例和解释。', FALSE),
(0, '写作助手', '你是一个专业的写作助手，帮助用户撰写文章、报告、邮件等各种文本内容。请保持写作风格专业、流畅且符合用户需求。', FALSE);
