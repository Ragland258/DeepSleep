// 首先取消 byte 宏定义，避免 Windows SDK 与 std::byte 冲突
#ifdef byte
#undef byte
#endif

#include "Database.h"
#include "RedisClient.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <array>
#include <vector>

// MySQL C API
#include <mysql.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

// 辅助宏：将void*转换为MYSQL*
#define MYSQL_CONN (static_cast<MYSQL*>(mysql_conn_))

namespace {
const char* kPasswordHashPrefix = "pbkdf2_sha256";
const int kPasswordIterations = 120000;
const size_t kPasswordSaltBytes = 16;
const size_t kPasswordHashBytes = 32;

std::string BytesToHex(const unsigned char* data, size_t length)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        stream << std::setw(2) << static_cast<int>(data[i]);
    }
    return stream.str();
}

bool HexToBytes(const std::string& hex, std::vector<unsigned char>& output)
{
    if (hex.size() % 2 != 0) {
        return false;
    }

    output.clear();
    output.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int value = 0;
        std::istringstream stream(hex.substr(i, 2));
        stream >> std::hex >> value;
        if (stream.fail()) {
            return false;
        }
        output.push_back(static_cast<unsigned char>(value));
    }

    return true;
}

std::vector<std::string> SplitString(const std::string& value, char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

std::string LegacyHashPassword(const std::string& password)
{
    std::hash<std::string> hasher;
    std::stringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hasher(password);
    return stream.str();
}
}

Database::Database()
    : mysql_conn_(nullptr), port_(3306), connected_(false) {
}

Database::~Database() {
    Cleanup();
}

void Database::Cleanup() {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (mysql_conn_) {
        mysql_close(MYSQL_CONN);
        mysql_conn_ = nullptr;
        connected_ = false;
    }
}

bool Database::IsConnected() const {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    return connected_ && mysql_conn_;
}

bool Database::Init(const std::string& host, const std::string& user,
                   const std::string& password, const std::string& dbname,
                   int port) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    host_ = host;
    user_ = user;
    password_ = password;
    dbname_ = dbname;
    port_ = port;

    std::cout << "[Database] Initializing MySQL connection..." << std::endl;
    std::cout << "[Database] Host: " << host << ":" << port << std::endl;
    std::cout << "[Database] User: " << user << std::endl;
    std::cout << "[Database] Database: " << dbname << std::endl;

    // 初始化MySQL
    mysql_conn_ = mysql_init(nullptr);
    if (!mysql_conn_) {
        std::cerr << "[Database] mysql_init failed!" << std::endl;
        return false;
    }

    // 连接到MySQL（先不指定数据库，以便创建数据库）
    if (!mysql_real_connect(MYSQL_CONN, host.c_str(), user.c_str(), 
                           password.c_str(), nullptr, port, nullptr, 0)) {
        std::cerr << "[Database] Connection failed: " << mysql_error(MYSQL_CONN) << std::endl;
        Cleanup();
        return false;
    }

    std::cout << "[Database] Connected to MySQL server!" << std::endl;

    // 自动初始化数据库和表
    if (!InitDatabase()) {
        std::cerr << "[Database] Failed to initialize database!" << std::endl;
        Cleanup();
        return false;
    }

    connected_ = true;
    std::cout << "[Database] Database initialized successfully!" << std::endl;
    return true;
}

bool Database::InitDatabase() {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    // 创建数据库（如果不存在）
    std::string create_db = "CREATE DATABASE IF NOT EXISTS " + dbname_ + 
                           " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
    if (!ExecuteSQL(create_db)) {
        std::cerr << "[Database] Failed to create database!" << std::endl;
        return false;
    }
    std::cout << "[Database] Database '" << dbname_ << "' ready!" << std::endl;

    // 选择数据库
    if (mysql_select_db(MYSQL_CONN, dbname_.c_str()) != 0) {
        std::cerr << "[Database] Failed to select database: " << mysql_error(MYSQL_CONN) << std::endl;
        return false;
    }

    // 创建users表
    const char* create_users = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INT AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(50) UNIQUE NOT NULL,
            password_hash VARCHAR(255) NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            INDEX idx_username (username)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    if (!ExecuteSQL(create_users)) {
        std::cerr << "[Database] Failed to create users table!" << std::endl;
        return false;
    }

    // 创建conversations表
    const char* create_conversations = R"(
        CREATE TABLE IF NOT EXISTS conversations (
            id INT AUTO_INCREMENT PRIMARY KEY,
            user_id INT NOT NULL,
            title VARCHAR(255) DEFAULT 'New Chat',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            INDEX idx_user_id (user_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    if (!ExecuteSQL(create_conversations)) {
        std::cerr << "[Database] Failed to create conversations table!" << std::endl;
        return false;
    }

    // 创建messages表
    const char* create_messages = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INT AUTO_INCREMENT PRIMARY KEY,
            conversation_id INT NOT NULL,
            role ENUM('user', 'assistant') NOT NULL,
            content TEXT NOT NULL,
            model_name VARCHAR(100) DEFAULT NULL,
            `references` TEXT DEFAULT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE,
            INDEX idx_conversation_id (conversation_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    if (!ExecuteSQL(create_messages)) {
        std::cerr << "[Database] Failed to create messages table!" << std::endl;
        return false;
    }

    const char* check_column = "SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'messages' AND COLUMN_NAME = 'model_name'";
    if (mysql_query(MYSQL_CONN, check_column) == 0) {
        MYSQL_RES* result = mysql_store_result(MYSQL_CONN);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && atoi(row[0]) == 0) {
                const char* add_model_name = "ALTER TABLE messages ADD COLUMN model_name VARCHAR(100) DEFAULT NULL";
                if (mysql_query(MYSQL_CONN, add_model_name) != 0) {
                    std::cerr << "[Database] Failed to add model_name column: " << mysql_error(MYSQL_CONN) << std::endl;
                } else {
                    std::cout << "[Database] Added model_name column to messages table" << std::endl;
                }
            }
            mysql_free_result(result);
        }
    }

    const char* check_ref_column = "SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'messages' AND COLUMN_NAME = 'references'";
    if (mysql_query(MYSQL_CONN, check_ref_column) == 0) {
        MYSQL_RES* result = mysql_store_result(MYSQL_CONN);
        if (result) {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row && atoi(row[0]) == 0) {
                const char* add_ref = "ALTER TABLE messages ADD COLUMN `references` TEXT DEFAULT NULL";
                if (mysql_query(MYSQL_CONN, add_ref) != 0) {
                    std::cerr << "[Database] Failed to add references column: " << mysql_error(MYSQL_CONN) << std::endl;
                } else {
                    std::cout << "[Database] Added references column to messages table" << std::endl;
                }
            }
            mysql_free_result(result);
        }
    }

    // 创建system_prompts表（系统提示词）
    const char* create_prompts = R"(
        CREATE TABLE IF NOT EXISTS system_prompts (
            id INT AUTO_INCREMENT PRIMARY KEY,
            user_id INT NOT NULL,
            name VARCHAR(100) NOT NULL,
            content TEXT NOT NULL,
            is_default BOOLEAN DEFAULT FALSE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            INDEX idx_user_id (user_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    if (!ExecuteSQL(create_prompts)) {
        std::cerr << "[Database] Failed to create system_prompts table!" << std::endl;
        return false;
    }

    // 创建model_providers表（模型提供商配置）
    const char* create_providers = R"(
        CREATE TABLE IF NOT EXISTS model_providers (
            id INT AUTO_INCREMENT PRIMARY KEY,
            user_id INT NOT NULL,
            name VARCHAR(50) NOT NULL,
            type VARCHAR(20) NOT NULL,
            api_key VARCHAR(255),
            base_url VARCHAR(255) NOT NULL,
            model_name VARCHAR(100) DEFAULT 'default',
            is_active BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            INDEX idx_user_id (user_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    if (!ExecuteSQL(create_providers)) {
        std::cerr << "[Database] Failed to create model_providers table!" << std::endl;
        return false;
    }

    // 检查并添加 model_name 字段（如果表已存在但缺少该字段）
    // MySQL 8.0+ 支持 IF NOT EXISTS，旧版本需要手动处理
    const char* check_provider_column = "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema = DATABASE() AND table_name = 'model_providers' AND column_name = 'model_name'";
    if (mysql_query(MYSQL_CONN, check_provider_column) == 0) {
        MYSQL_RES* res = mysql_store_result(MYSQL_CONN);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && atoi(row[0]) == 0) {
                // 字段不存在，添加它
                const char* add_provider_column = "ALTER TABLE model_providers ADD COLUMN model_name VARCHAR(100) DEFAULT 'default'";
                if (mysql_query(MYSQL_CONN, add_provider_column) != 0) {
                    std::cerr << "[Database] Warning: Failed to add model_name column: " << mysql_error(MYSQL_CONN) << std::endl;
                } else {
                    std::cout << "[Database] Added model_name column to model_providers table" << std::endl;
                }
            }
            mysql_free_result(res);
        }
    }

    // 创建provider_models表（提供商下的具体模型）
    const char* create_provider_models = R"(
        CREATE TABLE IF NOT EXISTS provider_models (
            id INT AUTO_INCREMENT PRIMARY KEY,
            provider_id INT NOT NULL,
            model_name VARCHAR(100) NOT NULL,
            model_id VARCHAR(100) NOT NULL,
            is_active BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (provider_id) REFERENCES model_providers(id) ON DELETE CASCADE,
            INDEX idx_provider_id (provider_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )";
    if (!ExecuteSQL(create_provider_models)) {
        std::cerr << "[Database] Failed to create provider_models table!" << std::endl;
        return false;
    }

// 创建email_verification_codes表（邮箱验证码）    const char* create_verification_codes = R"(        CREATE TABLE IF NOT EXISTS email_verification_codes (            id INT AUTO_INCREMENT PRIMARY KEY,            email VARCHAR(100) NOT NULL,            code VARCHAR(10) NOT NULL,            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,            expires_at TIMESTAMP NOT NULL,            used BOOLEAN DEFAULT FALSE,            INDEX idx_email (email),            INDEX idx_expires (expires_at)        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4    )";    if (!ExecuteSQL(create_verification_codes)) {        std::cerr << "[Database] Failed to create email_verification_codes table!" << std::endl;        return false;    }    // 检查users表是否有email字段，没有则添加    const char* check_email_column = "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema = DATABASE() AND table_name = users AND column_name = email";    if (mysql_query(MYSQL_CONN, check_email_column) == 0) {        MYSQL_RES* res = mysql_store_result(MYSQL_CONN);        if (res) {            MYSQL_ROW row = mysql_fetch_row(res);            if (row && atoi(row[0]) == 0) {                const char* add_email = "ALTER TABLE users ADD COLUMN email VARCHAR(100) DEFAULT NULL AFTER username";                if (mysql_query(MYSQL_CONN, add_email) != 0) {                    std::cerr << "[Database] Warning: Failed to add email column: " << mysql_error(MYSQL_CONN) << std::endl;                } else {                    std::cout << "[Database] Added email column to users table" << std::endl;                }            }            mysql_free_result(res);        }    }
    std::cout << "[Database] All tables created successfully!" << std::endl;
    return true;
}

bool Database::ExecuteSQL(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!mysql_conn_) return false;
    
    if (mysql_query(MYSQL_CONN, sql.c_str()) != 0) {
        std::cerr << "[Database] SQL Error: " << mysql_error(MYSQL_CONN) << std::endl;
        std::cerr << "[Database] Query: " << sql.substr(0, 100) << "..." << std::endl;
        return false;
    }
    return true;
}

std::string Database::HashPassword(const std::string& password) {
    std::array<unsigned char, kPasswordSaltBytes> salt = {};
    std::array<unsigned char, kPasswordHashBytes> derived = {};

    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        std::cerr << "[Database] Failed to generate secure password salt" << std::endl;
        return "";
    }

    if (PKCS5_PBKDF2_HMAC(
            password.c_str(),
            static_cast<int>(password.size()),
            salt.data(),
            static_cast<int>(salt.size()),
            kPasswordIterations,
            EVP_sha256(),
            static_cast<int>(derived.size()),
            derived.data()) != 1) {
        std::cerr << "[Database] Failed to derive password hash" << std::endl;
        return "";
    }

    std::ostringstream stream;
    stream << kPasswordHashPrefix
           << "$" << std::to_string(kPasswordIterations)
           << "$" << BytesToHex(salt.data(), salt.size())
           << "$" << BytesToHex(derived.data(), derived.size());
    return stream.str();
}

bool Database::VerifyPassword(const std::string& password, const std::string& stored_hash, bool* needs_rehash) {
    if (needs_rehash) {
        *needs_rehash = false;
    }

    if (stored_hash.find(kPasswordHashPrefix) == 0) {
        const auto parts = SplitString(stored_hash, '$');
        std::cout << "[DB] VerifyPassword: prefix=" << kPasswordHashPrefix << " parts=" << parts.size() << std::endl;
        if (parts.size() != 4) {
            std::cerr << "[DB] VerifyPassword: invalid hash format, parts=" << parts.size() << std::endl;
            return false;
        }

        int iterations = 0;
        try {
            iterations = std::stoi(parts[1]);
        } catch (const std::exception&) {
            return false;
        }

        std::vector<unsigned char> salt;
        std::vector<unsigned char> expected_hash;
        if (!HexToBytes(parts[2], salt) || !HexToBytes(parts[3], expected_hash) || expected_hash.empty()) {
            return false;
        }

        std::vector<unsigned char> actual_hash(expected_hash.size());
        if (PKCS5_PBKDF2_HMAC(
                password.c_str(),
                static_cast<int>(password.size()),
                salt.data(),
                static_cast<int>(salt.size()),
                iterations,
                EVP_sha256(),
                static_cast<int>(actual_hash.size()),
                actual_hash.data()) != 1) {
            return false;
        }

        bool match = CRYPTO_memcmp(actual_hash.data(), expected_hash.data(), actual_hash.size()) == 0;
        std::cout << "[DB] VerifyPassword: password_len=" << password.size() << " salt_hex=" << parts[2].size() << " hash_match=" << match << std::endl;
        return match;
    }

    const bool matched = (LegacyHashPassword(password) == stored_hash);
    if (matched && needs_rehash) {
        *needs_rehash = true;
    }
    return matched;
}

bool Database::UpdateUserPasswordHash(int user_id, const std::string& password_hash) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!connected_ || !mysql_conn_ || password_hash.empty()) {
        return false;
    }

    const char* query = "UPDATE users SET password_hash = ? WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)password_hash.c_str();
    bind[0].buffer_length = static_cast<unsigned long>(password_hash.length());

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = &user_id;

    const bool bound = mysql_stmt_bind_param(stmt, bind) == 0;
    const bool updated = bound && mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);
    return updated;
}

std::string Database::GetCurrentTimestamp() {

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    struct tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &time_t);
#else
    localtime_r(&time_t, &timeinfo);
#endif
    ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// ========== 用户相关操作 ==========

int Database::RegisterUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return -1;
    
    // 检查用户名是否已存在
    if (UserExists(username)) {
        std::cerr << "[Database] Username already exists: " << username << std::endl;
        return -1;
    }

    std::string password_hash = HashPassword(password);
    if (password_hash.empty()) return -1;
    
    // 使用预处理语句防止SQL注入
    const char* query = "INSERT INTO users (username, password_hash) VALUES (?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return -1;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)username.c_str();
    bind[0].buffer_length = static_cast<unsigned long>(username.length());

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)password_hash.c_str();
    bind[1].buffer_length = static_cast<unsigned long>(password_hash.length());

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        std::cerr << "[Database] Failed to register user: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    int user_id = static_cast<int>(mysql_stmt_insert_id(stmt));
    mysql_stmt_close(stmt);

    std::cout << "[Database] User registered successfully: " << username << " (ID: " << user_id << ")" << std::endl;
    return user_id;
}

int Database::LoginUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return -1;

    const char* query = "SELECT id, password_hash FROM users WHERE username = ? LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return -1;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = (void*)username.c_str();
    bind.buffer_length = static_cast<unsigned long>(username.length());

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND result_bind[2];
    memset(result_bind, 0, sizeof(result_bind));
    int user_id = -1;
    char stored_hash[256] = { 0 };
    unsigned long stored_hash_length = 0;

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &user_id;
    result_bind[0].is_unsigned = 0;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = stored_hash;
    result_bind[1].buffer_length = sizeof(stored_hash);
    result_bind[1].length = &stored_hash_length;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_fetch(stmt) == 0) {
        const std::string stored_hash_value(stored_hash, stored_hash_length);
        bool needs_rehash = false;
        const bool verified = VerifyPassword(password, stored_hash_value, &needs_rehash);
        mysql_stmt_close(stmt);

        if (!verified) {
            std::cerr << "[Database] Login failed for user: " << username << std::endl;
            return -1;
        }

        if (needs_rehash) {
            const std::string upgraded_hash = HashPassword(password);
            if (!upgraded_hash.empty()) {
                UpdateUserPasswordHash(user_id, upgraded_hash);
            }
        }

        std::cout << "[Database] User logged in: " << username << " (ID: " << user_id << ")" << std::endl;
        return user_id;
    }

    mysql_stmt_close(stmt);
    std::cerr << "[Database] Login failed for user: " << username << std::endl;
    return -1;
}

bool Database::UserExists(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    const char* query = "SELECT 1 FROM users WHERE username = ? LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = (void*)username.c_str();
    bind.buffer_length = static_cast<unsigned long>(username.length());

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_store_result(stmt);
    bool exists = mysql_stmt_num_rows(stmt) > 0;
    mysql_stmt_close(stmt);

    return exists;
}

// ========== 会话相关操作 ==========

int Database::CreateConversation(int user_id, const std::string& title) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return -1;

    const char* query = "INSERT INTO conversations (user_id, title) VALUES (?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return -1;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (void*)&user_id;
    bind[0].is_unsigned = 0;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)title.c_str();
    bind[1].buffer_length = static_cast<unsigned long>(title.length());

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    int conversation_id = static_cast<int>(mysql_stmt_insert_id(stmt));
    mysql_stmt_close(stmt);

    std::cout << "[Database] Conversation created: " << title << " (ID: " << conversation_id << ")" << std::endl;
    return conversation_id;
}

std::vector<Conversation> Database::GetUserConversations(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    std::vector<Conversation> conversations;
    if (!IsConnected()) return conversations;

    const char* query = "SELECT id, user_id, title, created_at, updated_at "
                       "FROM conversations WHERE user_id = ? ORDER BY updated_at DESC";
    
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return conversations;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return conversations;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = (void*)&user_id;
    bind.is_unsigned = 0;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return conversations;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return conversations;
    }

    // 绑定结果
    MYSQL_BIND result_bind[5];
    memset(result_bind, 0, sizeof(result_bind));

    Conversation conv;
    char title_buffer[256];
    char created_at_buffer[20];
    char updated_at_buffer[20];
    unsigned long title_length, created_at_length, updated_at_length;

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &conv.id;

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &conv.user_id;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = title_buffer;
    result_bind[2].buffer_length = sizeof(title_buffer);
    result_bind[2].length = &title_length;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = created_at_buffer;
    result_bind[3].buffer_length = sizeof(created_at_buffer);
    result_bind[3].length = &created_at_length;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = updated_at_buffer;
    result_bind[4].buffer_length = sizeof(updated_at_buffer);
    result_bind[4].length = &updated_at_length;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0) {
        mysql_stmt_close(stmt);
        return conversations;
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        conv.title = std::string(title_buffer, title_length);
        conv.created_at = std::string(created_at_buffer, created_at_length);
        conv.updated_at = std::string(updated_at_buffer, updated_at_length);
        conversations.push_back(conv);
    }

    mysql_stmt_close(stmt);
    return conversations;
}

bool Database::DeleteConversation(int conversation_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    const char* query = "DELETE FROM conversations WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = (void*)&conversation_id;
    bind.is_unsigned = 0;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool success = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);

    return success;
}

bool Database::UpdateConversationTitle(int conversation_id, const std::string& title) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    const char* query = "UPDATE conversations SET title = ? WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)title.c_str();
    bind[0].buffer_length = static_cast<unsigned long>(title.length());

    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (void*)&conversation_id;
    bind[1].is_unsigned = 0;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool success = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);

    return success;
}

// ========== 消息相关操作 ==========

bool Database::AddMessage(int conversation_id, const std::string& role, const std::string& content,
                          const std::string& model_name, const std::string& references)
{
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    const char* query = "INSERT INTO messages (conversation_id, role, content, model_name, `references`) VALUES (?, ?, ?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[5];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (void*)&conversation_id;
    bind[0].is_unsigned = 0;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)role.c_str();
    bind[1].buffer_length = static_cast<unsigned long>(role.length());

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)content.c_str();
    bind[2].buffer_length = static_cast<unsigned long>(content.length());

    bool model_null = model_name.empty() ? 1 : 0;
    if (!model_name.empty()) {
        bind[3].buffer_type = MYSQL_TYPE_STRING;
        bind[3].buffer = (void*)model_name.c_str();
        bind[3].buffer_length = static_cast<unsigned long>(model_name.length());
        bind[3].is_null = &model_null;
    } else {
        bind[3].buffer_type = MYSQL_TYPE_NULL;
        bind[3].is_null = &model_null;
    }

    bool ref_null = references.empty() ? 1 : 0;
    if (!references.empty()) {
        bind[4].buffer_type = MYSQL_TYPE_STRING;
        bind[4].buffer = (void*)references.c_str();
        bind[4].buffer_length = static_cast<unsigned long>(references.length());
        bind[4].is_null = &ref_null;
    } else {
        bind[4].buffer_type = MYSQL_TYPE_NULL;
        bind[4].is_null = &ref_null;
    }

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool success = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);

    return success;
}

std::vector<Message> Database::GetConversationMessages(int conversation_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    std::vector<Message> messages;
    if (!IsConnected()) return messages;

    const char* query = "SELECT id, conversation_id, role, content, model_name, `references`, created_at "
                       "FROM messages WHERE conversation_id = ? ORDER BY created_at ASC";

    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return messages;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return messages;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = (void*)&conversation_id;
    bind.is_unsigned = 0;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return messages;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return messages;
    }

    MYSQL_BIND result_bind[7];
    memset(result_bind, 0, sizeof(result_bind));

    Message msg;
    char role_buffer[20];
    char content_buffer[65535];
    char model_name_buffer[100];
    char references_buffer[4096];
    char created_at_buffer[20];
    unsigned long role_length, content_length, model_name_length, references_length, created_at_length;
    bool model_name_is_null = false;
    bool references_is_null = false;

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &msg.id;

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &msg.conversation_id;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = role_buffer;
    result_bind[2].buffer_length = sizeof(role_buffer);
    result_bind[2].length = &role_length;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = content_buffer;
    result_bind[3].buffer_length = sizeof(content_buffer);
    result_bind[3].length = &content_length;

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = model_name_buffer;
    result_bind[4].buffer_length = sizeof(model_name_buffer);
    result_bind[4].length = &model_name_length;
    result_bind[4].is_null = &model_name_is_null;

    result_bind[5].buffer_type = MYSQL_TYPE_STRING;
    result_bind[5].buffer = references_buffer;
    result_bind[5].buffer_length = sizeof(references_buffer);
    result_bind[5].length = &references_length;
    result_bind[5].is_null = &references_is_null;

    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = created_at_buffer;
    result_bind[6].buffer_length = sizeof(created_at_buffer);
    result_bind[6].length = &created_at_length;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0) {
        mysql_stmt_close(stmt);
        return messages;
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        msg.role = std::string(role_buffer, role_length);
        msg.content = std::string(content_buffer, content_length);
        if (!model_name_is_null && model_name_length > 0) {
            msg.model_name = std::string(model_name_buffer, model_name_length);
        }
        if (!references_is_null && references_length > 0) {
            msg.references = std::string(references_buffer, references_length);
        }
        msg.created_at = std::string(created_at_buffer, created_at_length);
        messages.push_back(msg);
    }

    mysql_stmt_close(stmt);
    return messages;
}

std::vector<Message> Database::GetRecentMessages(int conversation_id, int limit) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    std::vector<Message> messages;
    if (!IsConnected() || limit <= 0) return messages;

    const char* query = "SELECT id, conversation_id, role, content, model_name, `references`, created_at "
                       "FROM messages WHERE conversation_id = ? ORDER BY created_at DESC, id DESC LIMIT ?";

    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return messages;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return messages;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (void*)&conversation_id;
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (void*)&limit;

    if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return messages;
    }

    MYSQL_BIND result_bind[7];
    memset(result_bind, 0, sizeof(result_bind));

    Message msg;
    char role_buffer[20];
    char content_buffer[65535];
    char model_name_buffer[100];
    char references_buffer[4096];
    char created_at_buffer[20];
    unsigned long role_length, content_length, model_name_length, references_length, created_at_length;
    bool model_name_is_null = false;
    bool references_is_null = false;

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &msg.id;
    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &msg.conversation_id;
    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = role_buffer;
    result_bind[2].buffer_length = sizeof(role_buffer);
    result_bind[2].length = &role_length;
    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = content_buffer;
    result_bind[3].buffer_length = sizeof(content_buffer);
    result_bind[3].length = &content_length;
    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = model_name_buffer;
    result_bind[4].buffer_length = sizeof(model_name_buffer);
    result_bind[4].length = &model_name_length;
    result_bind[4].is_null = &model_name_is_null;
    result_bind[5].buffer_type = MYSQL_TYPE_STRING;
    result_bind[5].buffer = references_buffer;
    result_bind[5].buffer_length = sizeof(references_buffer);
    result_bind[5].length = &references_length;
    result_bind[5].is_null = &references_is_null;
    result_bind[6].buffer_type = MYSQL_TYPE_STRING;
    result_bind[6].buffer = created_at_buffer;
    result_bind[6].buffer_length = sizeof(created_at_buffer);
    result_bind[6].length = &created_at_length;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0) {
        mysql_stmt_close(stmt);
        return messages;
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        msg.role = std::string(role_buffer, role_length);
        msg.content = std::string(content_buffer, content_length);
        msg.model_name = (!model_name_is_null && model_name_length > 0)
            ? std::string(model_name_buffer, model_name_length)
            : "";
        msg.references = (!references_is_null && references_length > 0)
            ? std::string(references_buffer, references_length)
            : "";
        msg.created_at = std::string(created_at_buffer, created_at_length);
        messages.push_back(msg);
    }

    mysql_stmt_close(stmt);
    std::reverse(messages.begin(), messages.end());
    return messages;
}

// ========== JSON接口 ==========

Json::Value Database::GetConversationListJson(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    Json::Value result;
    result["conversations"] = Json::arrayValue;

    auto conversations = GetUserConversations(user_id);
    for (const auto& conv : conversations) {
        Json::Value item;
        item["id"] = conv.id;
        item["user_id"] = conv.user_id;
        item["title"] = conv.title;
        item["created_at"] = conv.created_at;
        item["updated_at"] = conv.updated_at;
        result["conversations"].append(item);
    }

    return result;
}

Json::Value Database::GetConversationHistoryJson(int conversation_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    Json::Value result;
    result["messages"] = Json::arrayValue;

    auto messages = GetConversationMessages(conversation_id);
    for (const auto& msg : messages) {
        Json::Value item;
        item["id"] = msg.id;
        item["conversation_id"] = msg.conversation_id;
        item["role"] = msg.role;
        item["content"] = msg.content;
        item["model_name"] = msg.model_name;
        item["references"] = msg.references;
        item["created_at"] = msg.created_at;
        result["messages"].append(item);
    }

    return result;
}

// ========== 系统提示词相关操作 ==========

int Database::AddSystemPrompt(int user_id, const std::string& name, const std::string& content, bool is_default) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return -1;

    // 如果设置为默认，先取消其他默认提示词
    if (is_default) {
        const char* reset_query = "UPDATE system_prompts SET is_default = FALSE WHERE user_id = ?";
        MYSQL_STMT* reset_stmt = mysql_stmt_init(MYSQL_CONN);
        if (reset_stmt) {
            if (mysql_stmt_prepare(reset_stmt, reset_query, static_cast<unsigned long>(strlen(reset_query))) == 0) {
                MYSQL_BIND reset_bind;
                memset(&reset_bind, 0, sizeof(reset_bind));
                reset_bind.buffer_type = MYSQL_TYPE_LONG;
                reset_bind.buffer = (void*)&user_id;
                mysql_stmt_bind_param(reset_stmt, &reset_bind);
                mysql_stmt_execute(reset_stmt);
            }
            mysql_stmt_close(reset_stmt);
        }
    }

    const char* query = "INSERT INTO system_prompts (user_id, name, content, is_default) VALUES (?, ?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return -1;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (void*)&user_id;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)name.c_str();
    bind[1].buffer_length = static_cast<unsigned long>(name.length());

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)content.c_str();
    bind[2].buffer_length = static_cast<unsigned long>(content.length());

    char is_default_val = is_default ? 1 : 0;
    bind[3].buffer_type = MYSQL_TYPE_TINY;
    bind[3].buffer = &is_default_val;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    bool success = mysql_stmt_execute(stmt) == 0;
    int prompt_id = success ? static_cast<int>(mysql_stmt_insert_id(stmt)) : -1;
    mysql_stmt_close(stmt);

    return prompt_id;
}

bool Database::UpdateSystemPrompt(int prompt_id, const std::string& name, const std::string& content, bool is_default) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    // 获取提示词的user_id
    int user_id = -1;
    const char* get_user_query = "SELECT user_id FROM system_prompts WHERE id = ?";
    MYSQL_STMT* get_user_stmt = mysql_stmt_init(MYSQL_CONN);
    if (get_user_stmt) {
        if (mysql_stmt_prepare(get_user_stmt, get_user_query, static_cast<unsigned long>(strlen(get_user_query))) == 0) {
            MYSQL_BIND user_bind;
            memset(&user_bind, 0, sizeof(user_bind));
            user_bind.buffer_type = MYSQL_TYPE_LONG;
            user_bind.buffer = &prompt_id;
            mysql_stmt_bind_param(get_user_stmt, &user_bind);
            if (mysql_stmt_execute(get_user_stmt) == 0) {
                MYSQL_BIND result_bind;
                memset(&result_bind, 0, sizeof(result_bind));
                result_bind.buffer_type = MYSQL_TYPE_LONG;
                result_bind.buffer = &user_id;
                mysql_stmt_bind_result(get_user_stmt, &result_bind);
                mysql_stmt_fetch(get_user_stmt);
            }
        }
        mysql_stmt_close(get_user_stmt);
    }

    // 如果设置为默认，先取消其他默认提示词
    if (is_default && user_id != -1) {
        const char* reset_query = "UPDATE system_prompts SET is_default = FALSE WHERE user_id = ? AND id != ?";
        MYSQL_STMT* reset_stmt = mysql_stmt_init(MYSQL_CONN);
        if (reset_stmt) {
            if (mysql_stmt_prepare(reset_stmt, reset_query, static_cast<unsigned long>(strlen(reset_query))) == 0) {
                MYSQL_BIND reset_bind[2];
                memset(reset_bind, 0, sizeof(reset_bind));
                reset_bind[0].buffer_type = MYSQL_TYPE_LONG;
                reset_bind[0].buffer = &user_id;
                reset_bind[1].buffer_type = MYSQL_TYPE_LONG;
                reset_bind[1].buffer = &prompt_id;
                mysql_stmt_bind_param(reset_stmt, reset_bind);
                mysql_stmt_execute(reset_stmt);
            }
            mysql_stmt_close(reset_stmt);
        }
    }

    const char* query = "UPDATE system_prompts SET name = ?, content = ?, is_default = ? WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)name.c_str();
    bind[0].buffer_length = static_cast<unsigned long>(name.length());

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)content.c_str();
    bind[1].buffer_length = static_cast<unsigned long>(content.length());

    char is_default_val = is_default ? 1 : 0;
    bind[2].buffer_type = MYSQL_TYPE_TINY;
    bind[2].buffer = &is_default_val;

    bind[3].buffer_type = MYSQL_TYPE_LONG;
    bind[3].buffer = &prompt_id;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool success = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);

    return success;
}

bool Database::DeleteSystemPrompt(int prompt_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    const char* query = "DELETE FROM system_prompts WHERE id = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = &prompt_id;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool success = mysql_stmt_execute(stmt) == 0;
    mysql_stmt_close(stmt);

    return success;
}

Json::Value Database::GetUserSystemPrompts(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    Json::Value result;
    result["prompts"] = Json::arrayValue;

    if (!IsConnected()) return result;

    const char* query = "SELECT id, user_id, name, content, is_default, created_at FROM system_prompts WHERE user_id = ? ORDER BY created_at DESC";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return result;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return result;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = &user_id;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return result;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return result;
    }

    // 绑定结果
    int id;
    int uid;
    char name[101];
    char content[4096];
    char is_default;
    char created_at[20];
    unsigned long name_len, content_len, created_at_len;

    MYSQL_BIND result_bind[6];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_LONG;
    result_bind[1].buffer = &uid;

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = name;
    result_bind[2].buffer_length = sizeof(name);
    result_bind[2].length = &name_len;

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = content;
    result_bind[3].buffer_length = sizeof(content);
    result_bind[3].length = &content_len;

    result_bind[4].buffer_type = MYSQL_TYPE_TINY;
    result_bind[4].buffer = &is_default;

    result_bind[5].buffer_type = MYSQL_TYPE_STRING;
    result_bind[5].buffer = created_at;
    result_bind[5].buffer_length = sizeof(created_at);
    result_bind[5].length = &created_at_len;

    if (mysql_stmt_bind_result(stmt, result_bind) != 0) {
        mysql_stmt_close(stmt);
        return result;
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        Json::Value item;
        item["id"] = id;
        item["user_id"] = uid;
        item["name"] = std::string(name, name_len);
        item["content"] = std::string(content, content_len);
        item["is_default"] = (is_default != 0);
        item["created_at"] = std::string(created_at, created_at_len);
        result["prompts"].append(item);
    }

    mysql_stmt_close(stmt);
    return result;
}

std::string Database::GetDefaultSystemPrompt(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return "";

    const char* query = "SELECT content FROM system_prompts WHERE user_id = ? AND is_default = TRUE LIMIT 1";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return "";

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return "";
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = &user_id;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return "";
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return "";
    }

    char content[4096];
    unsigned long content_len;

    MYSQL_BIND result_bind;
    memset(&result_bind, 0, sizeof(result_bind));
    result_bind.buffer_type = MYSQL_TYPE_STRING;
    result_bind.buffer = content;
    result_bind.buffer_length = sizeof(content);
    result_bind.length = &content_len;

    if (mysql_stmt_bind_result(stmt, &result_bind) != 0) {
        mysql_stmt_close(stmt);
        return "";
    }

    std::string result;
    if (mysql_stmt_fetch(stmt) == 0) {
        result = std::string(content, content_len);
    }

    mysql_stmt_close(stmt);
    return result;
}

// ========== 模型配置相关操作 ==========

int Database::AddModelProvider(int user_id, const std::string& name, const std::string& type,
                               const std::string& api_key, const std::string& base_url, const std::string& model_name) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) {
        std::cerr << "[Database] AddModelProvider failed: not connected" << std::endl;
        return -1;
    }

    const char* query = "INSERT INTO model_providers (user_id, name, type, api_key, base_url, model_name) VALUES (?, ?, ?, ?, ?, ?)";
    
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) {
        std::cerr << "[Database] AddModelProvider failed: stmt init failed" << std::endl;
        return -1;
    }

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        std::cerr << "[Database] AddModelProvider prepare failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[6];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (void*)&user_id;

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)name.c_str();
    bind[1].buffer_length = static_cast<unsigned long>(name.length());

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)type.c_str();
    bind[2].buffer_length = static_cast<unsigned long>(type.length());

    std::string api_key_str = api_key.empty() ? "" : api_key;
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)api_key_str.c_str();
    bind[3].buffer_length = static_cast<unsigned long>(api_key_str.length());

    bind[4].buffer_type = MYSQL_TYPE_STRING;
    bind[4].buffer = (void*)base_url.c_str();
    bind[4].buffer_length = static_cast<unsigned long>(base_url.length());
    
    std::string model_name_str = model_name.empty() ? "default" : model_name;
    bind[5].buffer_type = MYSQL_TYPE_STRING;
    bind[5].buffer = (void*)model_name_str.c_str();
    bind[5].buffer_length = static_cast<unsigned long>(model_name_str.length());

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        std::cerr << "[Database] AddModelProvider bind failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        std::cerr << "[Database] Failed to add provider: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    int provider_id = static_cast<int>(mysql_stmt_insert_id(stmt));
    mysql_stmt_close(stmt);

    std::cout << "[Database] Provider added successfully: " << name << " (ID: " << provider_id << ")" << std::endl;
    return provider_id;
}

bool Database::UpdateModelProvider(int provider_id, const std::string& name, const std::string& api_key,
                                   const std::string& base_url, const std::string& model_name) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    const char* query = "UPDATE model_providers SET name=?, api_key=?, base_url=?, model_name=? WHERE id=?";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[5];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)name.c_str();
    bind[0].buffer_length = static_cast<unsigned long>(name.length());

    std::string api_key_str = api_key.empty() ? "" : api_key;
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)api_key_str.c_str();
    bind[1].buffer_length = static_cast<unsigned long>(api_key_str.length());

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)base_url.c_str();
    bind[2].buffer_length = static_cast<unsigned long>(base_url.length());

    std::string model_name_str = model_name.empty() ? "default" : model_name;
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)model_name_str.c_str();
    bind[3].buffer_length = static_cast<unsigned long>(model_name_str.length());

    bind[4].buffer_type = MYSQL_TYPE_LONG;
    bind[4].buffer = (void*)&provider_id;

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool success = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);

    if (success)
        std::cout << "[Database] Provider updated: " << name << " (ID: " << provider_id << ")" << std::endl;
    return success;
}

Json::Value Database::GetUserModelProviders(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    Json::Value result;
    result["providers"] = Json::arrayValue;
    
    if (!IsConnected()) return result;

    const char* query = "SELECT id, name, type, api_key, base_url, model_name, is_active, created_at "
                       "FROM model_providers WHERE user_id = ? ORDER BY created_at DESC";
    
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return result;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return result;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = (void*)&user_id;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return result;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return result;
    }

    MYSQL_RES* meta = mysql_stmt_result_metadata(stmt);
    if (!meta) {
        mysql_stmt_close(stmt);
        return result;
    }

    mysql_stmt_store_result(stmt);

    int id;
    char name[51], type[21], api_key[256], base_url[256], model_name[101], created_at[20];
    bool is_active;
    unsigned long name_len, type_len, api_key_len, base_url_len, model_name_len, created_at_len;
    bool is_null[8];

    MYSQL_BIND result_bind[8];
    memset(result_bind, 0, sizeof(result_bind));

    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &id;

    result_bind[1].buffer_type = MYSQL_TYPE_STRING;
    result_bind[1].buffer = name;
    result_bind[1].buffer_length = sizeof(name);
    result_bind[1].length = &name_len;
    result_bind[1].is_null = &is_null[1];

    result_bind[2].buffer_type = MYSQL_TYPE_STRING;
    result_bind[2].buffer = type;
    result_bind[2].buffer_length = sizeof(type);
    result_bind[2].length = &type_len;
    result_bind[2].is_null = &is_null[2];

    result_bind[3].buffer_type = MYSQL_TYPE_STRING;
    result_bind[3].buffer = api_key;
    result_bind[3].buffer_length = sizeof(api_key);
    result_bind[3].length = &api_key_len;
    result_bind[3].is_null = &is_null[3];

    result_bind[4].buffer_type = MYSQL_TYPE_STRING;
    result_bind[4].buffer = base_url;
    result_bind[4].buffer_length = sizeof(base_url);
    result_bind[4].length = &base_url_len;
    result_bind[4].is_null = &is_null[4];
    
    result_bind[5].buffer_type = MYSQL_TYPE_STRING;
    result_bind[5].buffer = model_name;
    result_bind[5].buffer_length = sizeof(model_name);
    result_bind[5].length = &model_name_len;
    result_bind[5].is_null = &is_null[5];

    result_bind[6].buffer_type = MYSQL_TYPE_TINY;
    result_bind[6].buffer = &is_active;
    result_bind[6].is_null = &is_null[6];

    result_bind[7].buffer_type = MYSQL_TYPE_STRING;
    result_bind[7].buffer = created_at;
    result_bind[7].buffer_length = sizeof(created_at);
    result_bind[7].length = &created_at_len;
    result_bind[7].is_null = &is_null[7];

    if (mysql_stmt_bind_result(stmt, result_bind) != 0) {
        mysql_free_result(meta);
        mysql_stmt_close(stmt);
        return result;
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        Json::Value provider;
        provider["id"] = id;
        provider["name"] = std::string(name, name_len);
        provider["type"] = std::string(type, type_len);
        // Hide api_key for security - only return masked version
        std::string full_key = is_null[3] ? "" : std::string(api_key, api_key_len);
        if (full_key.length() > 8) {
            provider["api_key_masked"] = full_key.substr(0, 4) + "****" + full_key.substr(full_key.length() - 4);
        } else if (!full_key.empty()) {
            provider["api_key_masked"] = "****";
        } else {
            provider["api_key_masked"] = "";
        }
        provider["api_key_configured"] = !full_key.empty();
        provider["base_url"] = std::string(base_url, base_url_len);
        provider["model_name"] = is_null[5] ? "default" : std::string(model_name, model_name_len);
        provider["is_active"] = is_active ? true : false;
        provider["created_at"] = std::string(created_at, created_at_len);
        result["providers"].append(provider);
    }

    mysql_free_result(meta);
    mysql_stmt_close(stmt);

    return result;
}

bool Database::DeleteModelProvider(int provider_id) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    const char* query = "DELETE FROM model_providers WHERE id = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_LONG;
    bind.buffer = (void*)&provider_id;

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    bool success = (mysql_stmt_execute(stmt) == 0);
    mysql_stmt_close(stmt);

    return success;
}


// ========== 邮箱验证码相关 ==========

bool Database::EmailExists(const std::string& email) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return false;

    const char* query = "SELECT COUNT(*) FROM users WHERE email = ?";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return false;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = (void*)email.c_str();
    bind.buffer_length = static_cast<unsigned long>(email.length());

    if (mysql_stmt_bind_param(stmt, &bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    int count = 0;
    MYSQL_BIND result_bind;
    memset(&result_bind, 0, sizeof(result_bind));
    result_bind.buffer_type = MYSQL_TYPE_LONG;
    result_bind.buffer = &count;

    if (mysql_stmt_bind_result(stmt, &result_bind) != 0) {
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt) == 0) {
        mysql_stmt_close(stmt);
        return count > 0;
    }

    mysql_stmt_close(stmt);
    return false;
}

bool Database::SaveVerificationCode(const std::string& email, const std::string& code, int expires_minutes) {
    try {
        auto conn_wrapper = RedisPool::GetInstance().Get();
        RedisClient& redis = **conn_wrapper;

        // Key: verification:email:{email}
        std::string key = "verification:email:" + email;

        // 存储验证码，设置过期时间（秒）
        bool success = redis.set(key, code, expires_minutes * 60);

        if (success) {
            std::cout << "[Database] Verification code saved to Redis: " << email << " -> " << code << std::endl;
        } else {
            std::cerr << "[Database] Failed to save verification code to Redis" << std::endl;
        }

        return success;
    } catch (const std::exception& e) {
        std::cerr << "[Database] Redis error in SaveVerificationCode: " << e.what() << std::endl;
        return false;
    }
}

bool Database::VerifyCode(const std::string& email, const std::string& code) {
    try {
        auto conn_wrapper = RedisPool::GetInstance().Get();
        RedisClient& redis = **conn_wrapper;

        // Key: verification:email:{email}
        std::string key = "verification:email:" + email;

        // 获取存储的验证码
        auto stored_code = redis.get(key);

        if (!stored_code.has_value()) {
            std::cout << "[Database] Verification code not found or expired: " << email << std::endl;
            return false;
        }

        // 比较验证码
        if (stored_code.value() == code) {
            // 验证成功，删除验证码（一次性使用）
            redis.del(key);
            std::cout << "[Database] Verification code verified: " << email << std::endl;
            return true;
        }

        std::cout << "[Database] Verification code mismatch: " << email << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[Database] Redis error in VerifyCode: " << e.what() << std::endl;
        return false;
    }
}

int Database::RegisterUserWithEmail(const std::string& username, const std::string& password, const std::string& email) {
    std::lock_guard<std::recursive_mutex> lock(db_mutex_);
    if (!IsConnected()) return -1;

    // 检查用户名是否已存在
    if (UserExists(username)) {
        std::cerr << "[Database] Username already exists: " << username << std::endl;
        return -1;
    }

    // 检查邮箱是否已被使用
    if (EmailExists(email)) {
        std::cerr << "[Database] Email already registered: " << email << std::endl;
        return -2;  // 返回-2表示邮箱已存在
    }

    std::string password_hash = HashPassword(password);
    if (password_hash.empty()) return -1;

    const char* query = "INSERT INTO users (username, password_hash, email) VALUES (?, ?, ?)";
    MYSQL_STMT* stmt = mysql_stmt_init(MYSQL_CONN);
    if (!stmt) return -1;

    if (mysql_stmt_prepare(stmt, query, static_cast<unsigned long>(strlen(query))) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)username.c_str();
    bind[0].buffer_length = static_cast<unsigned long>(username.length());

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)password_hash.c_str();
    bind[1].buffer_length = static_cast<unsigned long>(password_hash.length());

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)email.c_str();
    bind[2].buffer_length = static_cast<unsigned long>(email.length());

    if (mysql_stmt_bind_param(stmt, bind) != 0) {
        mysql_stmt_close(stmt);
        return -1;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        std::cerr << "[Database] Failed to register user: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return -1;
    }

    int user_id = static_cast<int>(mysql_stmt_insert_id(stmt));
    mysql_stmt_close(stmt);

    std::cout << "[Database] User registered successfully: " << username << " (ID: " << user_id << ", Email: " << email << ")" << std::endl;
    return user_id;
}

bool Database::SendEmail(const std::string& to_email, const std::string& subject, const std::string& body) {
    std::cout << "[Database] Sending email to: " << to_email << std::endl;

    // 使用 Python 脚本发送邮件
    std::string script_path = "/home/ragland/deepsleep/backend/send_email.py";
    std::string cmd = "python3 " + script_path + " '" + to_email + "' '" + subject + "' '" + body + "'";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[Database] Failed to execute Python script" << std::endl;
        return false;
    }

    char buffer[256];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }

    int ret = pclose(pipe);

    if (result.find("SUCCESS") != std::string::npos) {
        std::cout << "[Database] Email sent successfully to: " << to_email << std::endl;
        return true;
    }

    std::cerr << "[Database] Failed to send email: " << result << std::endl;
    return false;
}
