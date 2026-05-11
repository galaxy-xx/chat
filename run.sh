#!/bin/bash
SERVER=build/Desktop_Qt_6_11_0-Debug/chat_server
CLIENT=build/Desktop_Qt_6_11_0-Debug/chat_client

case "$1" in
    start)
        pid=$(lsof -t -i :8888 2>/dev/null)
        if [ -n "$pid" ]; then
            echo "Server already running (PID $pid)"
        else
            $SERVER --foreground &
            echo "Server started"
        fi
        ;;
    stop)
        pid=$(lsof -t -i :8888 2>/dev/null)
        if [ -n "$pid" ]; then
            kill $pid && echo "Server stopped"
        else
            echo "Server not running"
        fi
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    client)
        $CLIENT
        ;;
    *)
        echo "Usage: ./run.sh {start|stop|restart|client}"
        ;;
esac
