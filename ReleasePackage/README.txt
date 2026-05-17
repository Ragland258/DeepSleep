===========================================
DeepSleep - AI 对话系统
===========================================

【系统要求】
- Windows 10/11 64位操作系统
- 已安装 Visual C++ 2022 Redistributable (x64)
  下载地址：https://aka.ms/vs/17/release/vc_redist.x64.exe

【文件说明】
DeepSleep.exe       - 主程序
libmysql.dll        - MySQL 数据库驱动
libssl-3-x64.dll    - OpenSSL SSL 库
libcrypto-3-x64.dll - OpenSSL 加密库
frontend/           - Web 前端界面
                    - index.html  主界面
                    - model-config.html 模型配置
                    - prompt-config.html 提示词配置
docs/               - 项目文档

【使用方法】
1. 确保已安装 Visual C++ 2022 Redistributable
2. 双击运行 DeepSleep.exe 启动服务
3. 打开浏览器访问 http://localhost:8080 使用系统
   或直接用浏览器打开 frontend/index.html

【注意事项】
- 首次运行需要配置数据库连接
- 默认服务端口：8081
- 详细配置请参考 docs/ 目录下的文档

【技术支持】
如有问题，请参考 docs/ 目录下的文档或联系开发者。
