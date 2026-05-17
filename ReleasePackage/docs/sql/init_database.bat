@echo off
echo ========================================
echo DeepSleep 数据库初始化脚本
echo ========================================
echo.

echo 请确保 MySQL 服务正在运行...
echo.
echo 按任意键继续，或按 Ctrl+C 退出
pause >nul

echo.
echo 正在执行数据库初始化...
echo.

REM 尝试使用 MySQL 命令行工具执行 schema.sql
REM 请根据你的 MySQL 安装路径修改下面的路径

REM 方式 1: 如果 mysql 在 PATH 中
mysql -u root -proot < "c:\Users\17969\source\repos\DeepSleep\project-root\docs\sql\schema.sql

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo 数据库初始化成功！
    echo ========================================
) else (
    echo.
    echo ========================================
    echo 数据库初始化失败！
    echo ========================================
    echo.
    echo 可能的原因：
    echo 1. MySQL 命令行工具不在 PATH 中
    echo 2. 用户名或密码错误
    echo 3. 需要手动执行 schema.sql
    echo.
    echo 请手动在 MySQL Workbench 或命令行中执行：
    echo source c:/Users/17969/source/repos/DeepSleep/project-root/docs/sql/schema.sql
    echo.
)

echo.
pause
