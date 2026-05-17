// WebSocket 连接管理
class WebSocketManager {
    constructor() {
        this.ws = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.messageHandlers = new Map();
        this.isConnected = false;
    }

    connect() {
        return new Promise((resolve, reject) => {
            try {
                // 自动获取服务器地址（支持本地和局域网访问）
                const serverHost = window.location.hostname || 'localhost';
                this.ws = new WebSocket(`ws://${serverHost}:8081`);

                this.ws.onopen = () => {
                    console.log('WebSocket connected');
                    this.isConnected = true;
                    this.reconnectAttempts = 0;
                    resolve();
                };

                this.ws.onmessage = (event) => {
                    this.handleMessage(event.data);
                };

                this.ws.onclose = () => {
                    console.log('WebSocket disconnected');
                    this.isConnected = false;
                    this.attemptReconnect();
                };

                this.ws.onerror = (error) => {
                    console.error('WebSocket error:', error);
                    reject(error);
                };
            } catch (error) {
                reject(error);
            }
        });
    }

    attemptReconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            console.log(`Attempting to reconnect... (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
            setTimeout(() => this.connect(), 2000);
        }
    }

    send(message) {
        if (this.isConnected && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
            return true;
        }
        console.error('WebSocket is not connected');
        return false;
    }

    handleMessage(data) {
        try {
            const message = JSON.parse(data);
            console.log('Received:', message);

            // 根据消息 ID 调用对应的处理器
            if (this.messageHandlers.has(message.id)) {
                this.messageHandlers.get(message.id)(message.data);
            }
        } catch (error) {
            console.error('Failed to parse message:', error);
        }
    }

    on(messageId, handler) {
        this.messageHandlers.set(messageId, handler);
    }

    disconnect() {
        if (this.ws) {
            this.ws.close();
        }
    }
}

// 消息 ID 常量
const MSG_ID = {
    CHAT_REQUEST: 2001,
    CHAT_RESPONSE: 2002,
    LOGIN_REQUEST: 2003,
    LOGIN_RESPONSE: 2004,
    REGISTER_REQUEST: 2005,
    REGISTER_RESPONSE: 2006,
    CONVERSATION_LIST: 2007,
    CREATE_CONVERSATION: 2008,
    HISTORY_REQUEST: 2009,
    CHAT_STREAM_START: 2010,
    CHAT_STREAM_DATA: 2011,
    CHAT_STREAM_END: 2012,
    
    // 模型配置相关
    GET_PROVIDERS: 2013,
    PROVIDERS_RESPONSE: 2014,
    ADD_PROVIDER: 2015,
    UPDATE_PROVIDER: 2016,
    DELETE_PROVIDER: 2017,
    GET_PROVIDER_MODELS: 2018,
    PROVIDER_MODELS_RESPONSE: 2019,
    ADD_PROVIDER_MODEL: 2020,
    DELETE_PROVIDER_MODEL: 2021,
    SET_DEFAULT_MODEL: 2022,
    GET_MODELS: 2023,
    MODELS_RESPONSE: 2024,
    
    // 系统提示词相关
    GET_PROMPTS: 2025,
    PROMPTS_RESPONSE: 2026,
    ADD_PROMPT: 2027,
    UPDATE_PROMPT: 2028,
    DELETE_PROMPT: 2029,
    SET_DEFAULT_PROMPT: 2030,
    
    // 会话删除
    DELETE_CONVERSATION: 2031,
    
    // 模型测试
    TEST_MODEL: 2032,
    TEST_MODEL_RESPONSE: 2033
};

// 全局 WebSocket 实例
const wsManager = new WebSocketManager();
