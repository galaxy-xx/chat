# 缺失功能补充设计

日期: 2026-05-18

## 背景

根据《Linux操作系统》课程报告要求，当前系统缺少两个功能：
1. 客户注销（报告第⑶条"注册与注销"）
2. 历史查询按时间范围过滤（报告1.1.1第(5)条）

## 功能一：客户注销

### 协议

新增消息类型：
- `MSG_DELETE_ACCOUNT "delete_account"` — 客户端请求
- `MSG_DELETE_ACCOUNT_RES "delete_account_res"` — 服务器响应

请求格式：
```json
{"type": "delete_account", "data": {}}
```

响应格式：
```json
{"type": "delete_account_res", "data": {"ok": true}}
```

### 服务端

1. `protocol.h` — 添加宏定义
2. `ChatServer::processMessage` — 添加路由分支
3. `ChatServer::handleDeleteAccount(QTcpSocket *sock)`:
   - 验证用户已登录
   - 调用 `m_db->deleteUser(username)`
   - 清除在线状态（m_onlineUsers, m_userMap, m_clients）
   - 广播用户列表更新
   - 返回成功响应
4. `Database::deleteUser(const QString &username)`:
   - DELETE FROM users WHERE username=?
   - messages 和 files 表数据保留（涉及其他用户）

### 客户端

1. `ClientNetwork::sendDeleteAccount()` — 发送请求
2. `MainWindow` — 在菜单栏添加"注销账号"菜单项
3. 点击后弹出 QMessageBox 确认对话框（二次确认）
4. 确认后发送请求，收到成功响应后：
   - 清除本地状态
   - 断开连接
   - 返回登录界面

## 功能二：历史查询按时间范围过滤

### 协议

扩展现有 `history_query` 的 data 字段，增加可选参数：
```json
{
  "type": "history_query",
  "data": {
    "type": "public",
    "target": "username",
    "limit": 200,
    "start_time": "2026-01-01 00:00:00",
    "end_time": "2026-12-31 23:59:59"
  }
}
```
start_time 和 end_time 均为可选，省略则不限制。

### 服务端

1. `ChatServer::handleHistory` — 从 data 读取 start_time/end_time，传给数据库方法
2. `Database::getMessages(type, target, limit, startTime, endTime)`:
   - SQL 增加 `AND timestamp BETWEEN ? AND ?` 条件（当参数非空时）
3. `Database::getFiles(type, target, limit, startTime, endTime)`:
   - 同上

### 客户端

1. `HistoryDialog` — 增加两个 QDateTimeEdit 控件：
   - 开始时间（默认为一个月前）
   - 结束时间（默认为当前时间）
   - 勾选框"启用时间范围"（默认不勾选，不传时间参数）
2. 搜索时将时间参数传入 `sendHistoryQuery`

## 涉及文件

| 文件 | 改动 |
|------|------|
| protocol.h | 添加 MSG_DELETE_ACCOUNT 宏 |
| server/handlers/auth.cpp | 添加 handleDeleteAccount |
| server/handlers/msg.cpp | 修改 handleHistory 传递时间参数 |
| server/database.h | 添加 deleteUser 声明，修改 getMessages/getFiles 签名 |
| server/database.cpp | 实现 deleteUser，修改 getMessages/getFiles 增加时间过滤 |
| server/server.h | 添加 handleDeleteAccount 声明 |
| server/server.cpp | processMessage 添加路由 |
| client/clientnetwork.h | 添加 sendDeleteAccount 声明 |
| client/clientnetwork.cpp | 实现 sendDeleteAccount |
| client/mainwindow.h | 添加 onDeleteAccount 槽 |
| client/mainwindow.cpp | 添加菜单项和注销逻辑 |
| client/dialogs/historydlg.h | 添加时间控件成员 |
| client/dialogs/historydlg.cpp | 添加时间范围 UI 和参数传递 |
