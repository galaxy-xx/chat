#!/bin/bash
# Chat 项目自动编译运行脚本

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 检查依赖
check_deps() {
    local missing=()

    command -v cmake &>/dev/null || missing+=("cmake")
    command -v make &>/dev/null || missing+=("make")
    command -v qmake6 &>/dev/null || missing+=("qt6")

    if [ ${#missing[@]} -gt 0 ]; then
        echo -e "${RED}缺少依赖: ${missing[*]}${NC}"
        echo "安装命令: sudo apt install cmake make qt6-base-dev libqt6sql6-sqlite"
        exit 1
    fi
}

# 编译
build() {
    echo -e "${YELLOW}正在编译...${NC}"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR" || exit 1

    if ! cmake "$PROJECT_DIR" 2>&1; then
        echo -e "${RED}CMake 配置失败${NC}"
        exit 1
    fi

    if ! make -j$(nproc) 2>&1; then
        echo -e "${RED}编译失败${NC}"
        exit 1
    fi

    echo -e "${GREEN}编译成功${NC}"
}

# 启动服务器
start_server() {
    local server="$BUILD_DIR/chat_server"
    if [ ! -f "$server" ]; then
        echo -e "${RED}服务器程序不存在，请先编译${NC}"
        return 1
    fi

    pid=$(lsof -t -i :8888 2>/dev/null)
    if [ -n "$pid" ]; then
        echo -e "${YELLOW}服务器已在运行 (PID $pid)${NC}"
        return 0
    fi

    "$server" --foreground &
    echo -e "${GREEN}服务器已启动${NC}"
}

# 启动客户端
start_client() {
    local client="$BUILD_DIR/chat_client"
    if [ ! -f "$client" ]; then
        echo -e "${RED}客户端程序不存在，请先编译${NC}"
        return 1
    fi

    "$client"
}

# 停止服务器
stop_server() {
    pid=$(lsof -t -i :8888 2>/dev/null)
    if [ -n "$pid" ]; then
        kill "$pid" && echo -e "${GREEN}服务器已停止${NC}"
    else
        echo -e "${YELLOW}服务器未运行${NC}"
    fi
}

# 主菜单
case "$1" in
    build)
        check_deps
        build
        ;;
    start)
        start_server
        ;;
    stop)
        stop_server
        ;;
    restart)
        stop_server
        sleep 1
        start_server
        ;;
    client)
        start_client
        ;;
    run)
        # 一键编译并运行
        check_deps
        build
        start_server
        sleep 1
        start_client
        ;;
    *)
        echo "用法: $0 {build|start|stop|restart|client|run}"
        echo ""
        echo "  build   - 编译项目"
        echo "  start   - 启动服务器"
        echo "  stop    - 停止服务器"
        echo "  restart - 重启服务器"
        echo "  client  - 启动客户端"
        echo "  run     - 一键编译并运行（服务器+客户端）"
        ;;
esac
