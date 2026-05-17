// 全局状态
let currentUser = null;
let currentConversationId = null;
let conversations = [];
let isStreaming = false;
let currentMode = 'fast'; // 默认快速模式
let availableModels = []; // 可用的模型列表
let selectedModel = null; // 当前选中的模型

// 初始化
async function init() {
    // 先连接 WebSocket（不管有没有登录）
    try {
        console.log('Connecting WebSocket...');
        await wsManager.connect();
        setupMessageHandlers();
        console.log('WebSocket connected!');
    } catch (error) {
        console.error('Failed to connect WebSocket:', error);
    }

    // 检查是否已登录
    const savedUser = localStorage.getItem('user');
    if (savedUser) {
        currentUser = JSON.parse(savedUser);
        updateUIForLoggedInUser();
        
        if (wsManager.isConnected) {
            loadConversations();
            loadModels(); // 加载模型列表
        }
    } else {
        // 未登录状态
        updateUIForLoggedOutUser();
    }
}

// 更新UI为已登录状态
function updateUIForLoggedInUser() {
    document.getElementById('username').textContent = currentUser.username;
    document.getElementById('userActionBtn').textContent = '退出';
    document.getElementById('loginModal').style.display = 'none';
    document.getElementById('registerModal').style.display = 'none';
}

// 更新UI为未登录状态
function updateUIForLoggedOutUser() {
    document.getElementById('username').textContent = '游客';
    document.getElementById('userActionBtn').textContent = '登录';
    document.getElementById('loginModal').style.display = 'none';
    document.getElementById('registerModal').style.display = 'none';
}

// 处理用户操作按钮点击
function handleUserAction() {
    if (currentUser) {
        // 已登录，执行退出
        logout();
    } else {
        // 未登录，显示登录弹窗
        showLogin();
    }
}

// 关闭弹窗
function closeModal() {
    document.getElementById('loginModal').style.display = 'none';
    document.getElementById('registerModal').style.display = 'none';
}

// 设置消息处理器
function setupMessageHandlers() {
    // 登录响应
    wsManager.on(MSG_ID.LOGIN_RESPONSE, (data) => {
        if (data.success) {
            currentUser = { id: data.user_id, username: document.getElementById('loginUsername').value };
            localStorage.setItem('user', JSON.stringify(currentUser));
            updateUIForLoggedInUser();
            loadConversations();
            loadModels(); // 加载模型列表
        } else {
            alert(data.message || '登录失败');
        }
    });

    // 注册响应
    wsManager.on(MSG_ID.REGISTER_RESPONSE, (data) => {
        if (data.success) {
            alert('注册成功，请登录');
            showLogin();
        } else {
            alert(data.message || '注册失败');
        }
    });

    // 聊天响应
    wsManager.on(MSG_ID.CHAT_RESPONSE, (data) => {
        if (data.type === 'bot') {
            displayAssistantMessage(data.message);
        }
    });

    // 会话列表响应
    wsManager.on(MSG_ID.CONVERSATION_LIST, (data) => {
        console.log('收到会话列表:', data);
        // 确保数据是数组
        if (Array.isArray(data)) {
            conversations = data;
        } else if (data && Array.isArray(data.conversations)) {
            conversations = data.conversations;
        } else {
            conversations = [];
        }
        console.log('会话数组:', conversations);
        renderConversationList();
    });

    // 创建会话响应
    wsManager.on(MSG_ID.CREATE_CONVERSATION, (data) => {
        if (data.success) {
            currentConversationId = data.conversation_id;
            loadConversations();
            clearChat();
        }
    });

    // 历史消息响应
    wsManager.on(MSG_ID.HISTORY_REQUEST, (data) => {
        console.log('收到历史消息数据:', data);
        // 支持两种格式：data 直接是数组，或 data.messages 是数组
        let messages = [];
        if (Array.isArray(data)) {
            messages = data;
        } else if (data && Array.isArray(data.messages)) {
            messages = data.messages;
        }
        renderHistoryMessages(messages);
    });

    // 删除会话响应
    wsManager.on(MSG_ID.DELETE_CONVERSATION, (data) => {
        if (data.success) {
            console.log('会话已删除:', data.conversation_id);
            // 如果删除的是当前会话，清空聊天区域
            if (data.conversation_id === currentConversationId) {
                currentConversationId = null;
                clearChat();
            }
            // 重新加载会话列表
            loadConversations();
        } else {
            alert('删除会话失败');
        }
    });

    // 模型提供商列表响应
    wsManager.on(MSG_ID.PROVIDERS_RESPONSE, (data) => {
        console.log('收到模型提供商列表:', data);
        if (Array.isArray(data)) {
            availableModels = data;
        } else if (data && Array.isArray(data.providers)) {
            availableModels = data.providers;
        } else {
            availableModels = [];
        }
        renderModelSelector();
    });

    // 流式输出 - 开始
    wsManager.on(MSG_ID.CHAT_STREAM_START, (data) => {
        console.log('流式输出开始:', data);
        startStreamMessage(data.model_name);
    });
    
    // 流式输出 - 数据
    wsManager.on(MSG_ID.CHAT_STREAM_DATA, (data) => {
        console.log('流式数据:', data);
        appendStreamChunk(data.chunk);
    });
    
    // 流式输出 - 结束
    wsManager.on(MSG_ID.CHAT_STREAM_END, (data) => {
        console.log('流式输出结束:', data);
        const hasReferences = data.has_references || false;
        if (hasReferences && data.references_json) {
            currentReferences = data.references_json;
        }
        finishStreamMessage(data.full_message, hasReferences);
    });
}

// 全局变量存储当前流式消息元素
let currentStreamElement = null;
let currentStreamContent = '';
let currentModelName = '';
let currentReferences = null;

// 开始流式消息
function startStreamMessage(modelName) {
    const container = document.getElementById('chatMessages');

    currentModelName = modelName || 'AI';
    currentReferences = null;

    const messageDiv = document.createElement('div');
    messageDiv.className = 'message assistant';
    messageDiv.id = 'streaming-message';
    messageDiv.innerHTML = `
        <div class="message-avatar">AI</div>
        <div class="message-content-wrapper">
            <div class="message-model-name">${currentModelName}</div>
            <div class="message-content">
                <span class="typing-cursor"></span>
            </div>
        </div>
    `;

    container.appendChild(messageDiv);
    currentStreamElement = messageDiv;
    currentStreamContent = '';

    container.scrollTop = container.scrollHeight;
}

// 检查用户是否在底部附近（允许自动滚动）
function isUserNearBottom(container) {
    const threshold = 100; // 距离底部100px内认为是在底部
    return container.scrollHeight - container.scrollTop - container.clientHeight < threshold;
}

// 添加流式数据块
function appendStreamChunk(chunk) {
    if (!currentStreamElement) return;
    
    currentStreamContent += chunk;
    const contentDiv = currentStreamElement.querySelector('.message-content');
    contentDiv.innerHTML = currentStreamContent + '<span class="typing-cursor"></span>';
    
    // 只有用户在底部附近时才自动滚动
    const container = document.getElementById('chatMessages');
    if (isUserNearBottom(container)) {
        container.scrollTop = container.scrollHeight;
    }
}

// 完成流式消息
function finishStreamMessage(fullMessage, hasReferences) {
    if (!currentStreamElement) return;

    const contentDiv = currentStreamElement.querySelector('.message-content');
    contentDiv.textContent = fullMessage;

    if (hasReferences && currentReferences) {
        const wrapper = currentStreamElement.querySelector('.message-content-wrapper');

        let referencesHtml = '';
        try {
            const refs = typeof currentReferences === 'string' ? JSON.parse(currentReferences) : currentReferences;
            if (Array.isArray(refs) && refs.length > 0) {
                referencesHtml = `
                    <div class="message-references">
                        <h4>📚 引用信息</h4>
                        <ul class="message-references-list">
                            ${refs.map((ref, idx) => `
                                <li>
                                    <div class="reference-title">[${idx + 1}] ${escapeHtml(ref.title || '来源 ' + (idx + 1))}</div>
                                    <div class="reference-snippet">${escapeHtml(ref.snippet || '')}</div>
                                    ${ref.url ? `<a class="reference-url" href="${escapeHtml(ref.url)}" target="_blank">${escapeHtml(ref.url)}</a>` : ''}
                                </li>
                            `).join('')}
                        </ul>
                    </div>
                `;
            }
        } catch (e) {
            console.log('解析引用信息失败:', e);
        }

        if (referencesHtml) {
            wrapper.insertAdjacentHTML('beforeend', referencesHtml);
        }
    }

    currentStreamElement.removeAttribute('id');

    currentStreamElement = null;
    currentStreamContent = '';
    currentReferences = null;
}

// 显示登录弹窗
function showLogin() {
    document.getElementById('loginModal').style.display = 'flex';
    document.getElementById('registerModal').style.display = 'none';
}

// 显示注册弹窗
function showRegister() {
    document.getElementById('loginModal').style.display = 'none';
    document.getElementById('registerModal').style.display = 'flex';
}

// 登录
function login() {
    const username = document.getElementById('loginUsername').value;
    const password = document.getElementById('loginPassword').value;
    
    if (!username || !password) {
        alert('请输入用户名和密码');
        return;
    }

    if (!wsManager.isConnected) {
        alert('WebSocket 未连接，请刷新页面重试');
        return;
    }

    wsManager.send({
        id: MSG_ID.LOGIN_REQUEST,
        data: { username, password }
    });
}

// 注册
function register() {
    const username = document.getElementById('registerUsername').value;
    const password = document.getElementById('registerPassword').value;
    
    if (!username || !password) {
        alert('请输入用户名和密码');
        return;
    }

    if (!wsManager.isConnected) {
        alert('WebSocket 未连接，请刷新页面重试');
        return;
    }

    wsManager.send({
        id: MSG_ID.REGISTER_REQUEST,
        data: { username, password }
    });
}

// 退出登录
function logout() {
    currentUser = null;
    currentConversationId = null;
    localStorage.removeItem('user');
    wsManager.disconnect();
    location.reload();
}

// 创建新会话
function createNewConversation() {
    if (!currentUser) return;

    const title = '新对话' + (conversations.length + 1);
    wsManager.send({
        id: MSG_ID.CREATE_CONVERSATION,
        data: { user_id: currentUser.id, title }
    });
}

// 加载会话列表
function loadConversations() {
    if (!currentUser) return;

    wsManager.send({
        id: MSG_ID.CONVERSATION_LIST,
        data: { user_id: currentUser.id }
    });
}

// 加载模型列表
function loadModels() {
    if (!currentUser) return;

    wsManager.send({
        id: MSG_ID.GET_PROVIDERS,
        data: { user_id: currentUser.id }
    });
}

// 渲染模型选择器
function renderModelSelector() {
    const selectEl = document.getElementById('modelSelect');
    if (!selectEl) return;

    // 保存当前选中的值
    const currentValue = selectEl.value;

    // 重新生成选项
    let html = '<option value="">选择模型...</option>';
    
    availableModels.forEach(model => {
        const modelName = model.name || '未命名模型';
        const modelType = model.model_name || 'default';
        html += `<option value="${modelName}">${modelName} (${modelType})</option>`;
    });
    
    selectEl.innerHTML = html;

    // 恢复选中的值，如果该模型仍然存在
    if (currentValue && availableModels.find(m => m.name === currentValue)) {
        selectEl.value = currentValue;
    } else if (availableModels.length > 0 && !selectedModel) {
        // 默认选中第一个
        selectEl.value = availableModels[0].name;
        selectedModel = availableModels[0];
    }
}

// 选择模型
function selectModel(modelName) {
    if (!modelName) {
        selectedModel = null;
        return;
    }
    
    selectedModel = availableModels.find(m => m.name === modelName);
    console.log('选择模型:', selectedModel);
}

// 切换联网搜索
function toggleSearch() {
    const checkbox = document.getElementById('searchToggle');
    const label = checkbox.closest('.search-toggle');
    
    if (checkbox.checked) {
        label.classList.add('active');
        console.log('联网搜索已开启');
    } else {
        label.classList.remove('active');
        console.log('联网搜索已关闭');
    }
}

// 渲染会话列表
function renderConversationList() {
    console.log('渲染会话列表, 数量:', conversations.length);
    const listEl = document.getElementById('conversationList');
    listEl.innerHTML = '';

    if (conversations.length === 0) {
        listEl.innerHTML = '<div style="padding: 20px; text-align: center; color: #999; font-size: 14px;">暂无会话</div>';
        return;
    }

    // 按更新时间排序（最新的在前）
    conversations.sort((a, b) => {
        const timeA = new Date(a.updated_at || a.created_at);
        const timeB = new Date(b.updated_at || b.created_at);
        return timeB - timeA;
    });

    conversations.forEach(conv => {
        console.log('渲染会话:', conv, 'title:', conv.title, '类型:', typeof conv.title);
        const item = document.createElement('div');
        item.className = 'conversation-item' + (conv.id === currentConversationId ? ' active' : '');
        // 确保 title 是字符串
        const title = conv.title || '新对话';
        item.innerHTML = `
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" width="16" height="16">
                <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>
            </svg>
            <span>${title}</span>
            <button class="delete-conv-btn" onclick="event.stopPropagation(); deleteConversation(${conv.id})" title="删除会话">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="14" height="14">
                    <path d="M18 6L6 18M6 6l12 12"/>
                </svg>
            </button>
        `;
        item.onclick = () => selectConversation(conv.id);
        listEl.appendChild(item);
    });
}

// 删除会话
function deleteConversation(id) {
    if (!confirm('确定要删除这个会话吗？')) {
        return;
    }
    
    wsManager.send({
        id: MSG_ID.DELETE_CONVERSATION,
        data: { conversation_id: id }
    });
}

// 选择会话
function selectConversation(id) {
    currentConversationId = id;
    renderConversationList();
    
    // 隐藏居中的欢迎界面，切换到聊天视图
    const centerContainer = document.getElementById('centerContainer');
    if (centerContainer) {
        centerContainer.style.display = 'none';
        const chatMessages = document.getElementById('chatMessages');
        chatMessages.style.justifyContent = 'flex-start';
        chatMessages.style.paddingTop = '80px';
        createBottomInput();
    }
    
    // 加载历史消息
    wsManager.send({
        id: MSG_ID.HISTORY_REQUEST,
        data: { conversation_id: id }
    });
}

// 选择模式
function selectMode(mode) {
    currentMode = mode;
    document.querySelectorAll('.mode-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    event.target.classList.add('active');
}

// 发送消息
function sendMessage() {
    const input = document.getElementById('messageInput');
    const message = input.value.trim();
    
    if (!message) return;

    // 如果没有登录，提示登录
    if (!currentUser) {
        alert('请先登录后再发送消息');
        showLogin();
        return;
    }

    // 隐藏居中的容器（欢迎消息和输入框），显示聊天消息区域
    const centerContainer = document.getElementById('centerContainer');
    if (centerContainer) {
        centerContainer.style.display = 'none';
        // 显示聊天消息区域并移动到正常位置
        const chatMessages = document.getElementById('chatMessages');
        chatMessages.style.justifyContent = 'flex-start';
        chatMessages.style.paddingTop = '80px';
        
        // 创建底部输入框
        createBottomInput();
    }

    // 如果没有会话，先创建会话
    if (!currentConversationId) {
        createNewConversation();
        // 延迟发送消息，等待会话创建完成
        setTimeout(() => {
            if (currentConversationId) {
                sendMessageToServer(message, input);
            }
        }, 500);
        return;
    }

    sendMessageToServer(message, input);
}

// 创建底部输入框
function createBottomInput() {
    // 检查是否已存在底部输入框
    if (document.getElementById('bottomInputContainer')) return;
    
    const bottomContainer = document.createElement('div');
    bottomContainer.id = 'bottomInputContainer';
    bottomContainer.className = 'bottom-input-container';
    bottomContainer.innerHTML = `
        <div class="chat-input-wrapper">
            <textarea 
                id="bottomMessageInput" 
                placeholder="输入消息..."
                rows="1"
                onkeydown="handleBottomKeyDown(event)"
            ></textarea>
            <button class="send-btn" onclick="sendBottomMessage()">
                <svg viewBox="0 0 24 24" width="24" height="24">
                    <path fill="currentColor" d="M2.01 21L23 12 2.01 3 2 10l15 2-15 2z"/>
                </svg>
            </button>
        </div>
    `;
    
    document.querySelector('.chat-container').appendChild(bottomContainer);
}

// 底部输入框键盘事件
function handleBottomKeyDown(event) {
    if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        sendBottomMessage();
    }
}

// 发送底部消息
function sendBottomMessage() {
    const input = document.getElementById('bottomMessageInput');
    const message = input.value.trim();
    
    if (!message) return;
    
    input.value = '';
    input.style.height = 'auto';
    
    sendMessageToServer(message, input);
}

function sendMessageToServer(message, input) {
    // 显示用户消息
    displayUserMessage(message);
    input.value = '';
    input.style.height = 'auto';

    // 检查是否选择了模型
    if (!selectedModel) {
        alert('请先选择一个模型');
        return;
    }

    // 检查是否开启联网搜索
    const enableSearch = document.getElementById('searchToggle')?.checked || false;

    // 发送到服务器
    wsManager.send({
        id: MSG_ID.CHAT_REQUEST,
        data: {
            message: message,
            conversation_id: currentConversationId,
            user_id: currentUser.id,
            mode: currentMode,
            provider_name: selectedModel.name,
            enable_search: enableSearch
        }
    });
}

// 处理键盘事件
function handleKeyDown(event) {
    if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        sendMessage();
    }
}

// 显示用户消息
function displayUserMessage(message) {
    const container = document.getElementById('chatMessages');
    
    // 移除欢迎消息
    const welcomeMsg = container.querySelector('.welcome-message');
    if (welcomeMsg) welcomeMsg.remove();

    const msgDiv = document.createElement('div');
    msgDiv.className = 'message user';
    msgDiv.innerHTML = `
        <div class="message-avatar">U</div>
        <div class="message-content">${escapeHtml(message)}</div>
    `;
    container.appendChild(msgDiv);
    container.scrollTop = container.scrollHeight;
}

// 显示 AI 消息（带流式效果）
function displayAssistantMessage(message, modelName) {
    const container = document.getElementById('chatMessages');
    
    const displayName = modelName || currentModelName || 'AI';
    
    const msgDiv = document.createElement('div');
    msgDiv.className = 'message assistant';
    msgDiv.innerHTML = `
        <div class="message-avatar">AI</div>
        <div class="message-content-wrapper">
            <div class="message-model-name">${displayName}</div>
            <div class="message-content typing-cursor"></div>
        </div>
    `;
    container.appendChild(msgDiv);
    container.scrollTop = container.scrollHeight;
    
    const contentDiv = msgDiv.querySelector('.message-content');
    
    // 流式输出效果
    let index = 0;
    const typeChar = () => {
        if (index < message.length) {
            contentDiv.textContent += message[index];
            index++;
            container.scrollTop = container.scrollHeight;
            setTimeout(typeChar, 30);
        } else {
            contentDiv.classList.remove('typing-cursor');
        }
    };
    typeChar();
}

// 直接显示AI消息（用于历史消息，无流式效果）
function displayAssistantMessageDirect(message, modelName, references) {
    const container = document.getElementById('chatMessages');

    const displayName = modelName || currentModelName || 'AI';

    const msgDiv = document.createElement('div');
    msgDiv.className = 'message assistant';

    let referencesHtml = '';
    if (references) {
        try {
            const refs = typeof references === 'string' ? JSON.parse(references) : references;
            if (Array.isArray(refs) && refs.length > 0) {
                referencesHtml = `
                    <div class="message-references">
                        <h4>📚 引用信息</h4>
                        <ul class="message-references-list">
                            ${refs.map((ref, idx) => `
                                <li>
                                    <div class="reference-title">[${idx + 1}] ${escapeHtml(ref.title || '来源 ' + (idx + 1))}</div>
                                    <div class="reference-snippet">${escapeHtml(ref.snippet || '')}</div>
                                    ${ref.url ? `<a class="reference-url" href="${escapeHtml(ref.url)}" target="_blank">${escapeHtml(ref.url)}</a>` : ''}
                                </li>
                            `).join('')}
                        </ul>
                    </div>
                `;
            }
        } catch (e) {
            console.log('解析引用信息失败:', e);
        }
    }

    msgDiv.innerHTML = `
        <div class="message-avatar">AI</div>
        <div class="message-content-wrapper">
            <div class="message-model-name">${displayName}</div>
            <div class="message-content">${escapeHtml(message)}</div>
            ${referencesHtml}
        </div>
    `;
    container.appendChild(msgDiv);
}

// 渲染历史消息
function renderHistoryMessages(messages) {
    console.log('渲染历史消息:', messages);
    
    // 确保切换到聊天视图
    const centerContainer = document.getElementById('centerContainer');
    if (centerContainer && centerContainer.style.display !== 'none') {
        centerContainer.style.display = 'none';
        const chatMessages = document.getElementById('chatMessages');
        chatMessages.style.justifyContent = 'flex-start';
        chatMessages.style.paddingTop = '80px';
        createBottomInput();
    }
    
    const container = document.getElementById('chatMessages');
    // 清除所有消息
    container.innerHTML = '';

    if (!messages || messages.length === 0) {
        console.log('没有历史消息');
        return;
    }

    messages.forEach(msg => {
        console.log('渲染消息:', msg);
        if (msg.role === 'user') {
            displayUserMessage(msg.content);
        } else {
            // 历史消息直接显示，不使用流式效果
            displayAssistantMessageDirect(msg.content, msg.model_name, msg.references);
        }
    });
    
    // 滚动到底部
    container.scrollTop = container.scrollHeight;
}

// 清空聊天区域
function clearChat() {
    const container = document.getElementById('chatMessages');
    container.innerHTML = `
        <div class="welcome-message">
            <h1>DeepSleep AI</h1>
            <p>我是你的 AI 助手，有什么可以帮你的吗？</p>
        </div>
    `;
}

// HTML 转义
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', init);
