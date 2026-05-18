# Chat - 基于 Qt6 的 Linux 即时通信系统  (cumt linux作业)

一个基于 C/S 架构的多客户端即时通信系统，参考微信风格设计，使用 C++/Qt6 开发。

## 功能特性

- **用户管理**：注册、登录、退出登录、注销账号
- **私聊通信**：一对一私密聊天，消息气泡显示
- **公聊通信**：公共聊天室，所有在线用户实时接收
- **文件传输**：支持任意类型文件传输，图片自动显示缩略图并支持预览
- **好友系统**：添加/删除好友、接受/拒绝请求、查看在线状态
- **群聊功能**：创建群聊、邀请成员、群消息广播、退出群聊
- **消息撤回**：2 分钟内可撤回自己发送的消息
- **离线消息**：登录后自动加载离线期间的消息
- **历史查询**：按消息类型（私聊/公聊/全部）和时间范围查询历史记录
- **未读提醒**：联系人列表显示未读消息数字徽标

## 系统架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         客户端 (Client)                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  LoginDialog  │  │  MainWindow  │  │  ChatWidget  │          │
│  │  登录/注册    │→ │  主聊天界面  │→ │  聊天窗口    │          │
│  └──────────────┘  └──────┬───────┘  └──────────────┘          │
│                           │                                      │
│                    ┌──────┴───────┐                              │
│                    │ ClientNetwork │                              │
│                    │  TCP 客户端   │                              │
│                    └──────┬───────┘                              │
└───────────────────────────┼─────────────────────────────────────┘
                            │ TCP (端口 8888)
                            │ 长度前缀 + JSON
┌───────────────────────────┼─────────────────────────────────────┐
│                    服务器 (Server)                                │
│                    ┌──────┴───────┐                              │
│                    │  ChatServer   │                              │
│                    │  消息分发     │                              │
│                    └──┬───┬───┬──┘                               │
│              ┌────────┤   │   ├────────┐                        │
│         ┌────┴───┐ ┌──┴──┐│┌──┴───┐ ┌──┴───┐                   │
│         │  auth  │ │ msg │││friend│ │group │                    │
│         │认证处理│ │消息 │││好友  │ │群聊  │                    │
│         └────────┘ └─────┘│└──────┘ └──────┘                   │
│                           │                                      │
│                    ┌──────┴───────┐                              │
│                    │   Database    │                              │
│                    │   SQLite      │                              │
│                    └──────────────┘                              │
└─────────────────────────────────────────────────────────────────┘
```

## 服务器端模块

```
ChatServer
├── 认证处理 (auth.cpp)
│   ├── handleRegister()      用户注册
│   ├── handleLogin()         用户登录
│   ├── handleLogout()        用户登出
│   └── handleDeleteAccount() 用户注销
├── 聊天消息 (msg.cpp)
│   ├── handlePrivateMsg()    私聊消息
│   ├── handlePublicMsg()     公聊消息
│   ├── handleFileMsg()       文件消息
│   ├── handleRecall()        消息撤回
│   ├── handleOfflineQuery()  离线消息查询
│   └── handleHistory()       历史记录查询
├── 好友关系 (friend.cpp)
│   ├── handleFriendRequest() 发送好友请求
│   ├── handleFriendAccept()  接受好友请求
│   ├── handleFriendReject()  拒绝好友请求
│   ├── handleFriendList()    好友列表
│   └── handleFriendRemove()  删除好友
├── 群聊管理 (group.cpp)
│   ├── handleGroupCreate()   创建群聊
│   ├── handleGroupInvite()   邀请成员
│   ├── handleGroupMsg()      群消息
│   ├── handleGroupList()     群列表
│   ├── handleGroupLeave()    退出群聊
│   └── handleGroupMembers()  群成员列表
└── 数据库 (database.cpp)
    ├── 用户管理    registerUser / loginUser / deleteUser
    ├── 消息管理    saveMessage / getMessages / getMessagesSince
    ├── 文件管理    saveFileRecord / getFiles
    ├── 好友管理    sendFriendRequest / acceptFriendRequest / ...
    └── 群组管理    createGroup / addGroupMember / ...
```

## 客户端模块

```
Client
├── main.cpp (程序入口)
│   ├── LoginDialog  → 登录/注册界面
│   └── MainWindow   → 主聊天界面（登录成功后循环）
├── ClientNetwork (网络通信)
│   ├── connectToServer()     连接服务器
│   ├── sendLogin/Register()  认证请求
│   ├── sendPrivateMsg()      私聊消息
│   ├── sendPublicMsg()       公聊消息
│   ├── sendFileMsg()         文件消息
│   ├── sendRecall()          撤回请求
│   ├── sendFriendRequest()   好友请求
│   ├── sendGroupCreate()     创建群聊
│   └── sendHistoryQuery()    历史查询
├── MainWindow (主界面)
│   ├── 左侧栏
│   │   ├── 群组列表 (QListWidget)
│   │   └── 联系人列表 (QListWidget)
│   ├── 右侧区
│   │   ├── 聊天标题 (QLabel)
│   │   ├── 聊天窗口 (QStackedWidget)
│   │   │   ├── 公聊 ChatWidget
│   │   │   ├── 私聊 ChatWidget (每个用户一个)
│   │   │   └── 群聊 ChatWidget (每个群一个)
│   │   └── 输入区域 (QTextEdit + 发送按钮)
│   └── 菜单栏
│       ├── 文件 → 发送文件 / 添加好友 / 好友管理 / 创建群聊 / 注销账号 / 退出登录
│       └── 查看 → 聊天历史
└── Dialogs (对话框)
    ├── HistoryDialog      历史查询（支持时间范围）
    ├── FriendManagement   好友管理
    ├── CreateGroupDialog  创建群聊
    ├── GroupMembersDialog 群成员列表
    └── ImagePreviewDialog 图片预览
```

## 消息流程图

```
用户A                    服务器                    用户B
  │                        │                        │
  │──── private_msg ──────→│                        │
  │     {target:B,         │                        │
  │      content:"你好"}    │                        │
  │                        │── 保存到 messages 表 ──│
  │                        │                        │
  │←─── message (回显) ────│                        │
  │     {msg_id:42,        │                        │
  │      from:A, to:B}     │──── message (转发) ───→│
  │                        │     {msg_id:42,        │
  │                        │      from:A, to:B}     │
  │                        │                        │
  │                        │                        │
  │──── recall ────────────→│                        │
  │     {msg_id:42}         │                        │
  │                        │── 验证: 发送者+2分钟 ──│
  │                        │── markRecalled() ──────│
  │←─── recall_res ────────│                        │
  │     {ok:true}          │──── recall_ntf ───────→│
  │                        │     {msg_id:42}        │
```

## 数据库 ER 图

```
┌──────────────┐       ┌──────────────────────┐       ┌──────────────┐
│    users     │       │      messages        │       │    files     │
├──────────────┤       ├──────────────────────┤       ├──────────────┤
│ id (PK)      │       │ id (PK)              │       │ id (PK)      │
│ username (UQ)│──┐    │ sender (FK→users)    │       │ sender       │
│ password     │  │    │ target               │       │ target       │
│ created_at   │  │    │ msg_type             │       │ file_type    │
└──────────────┘  │    │ content              │       │ filename     │
                  │    │ recalled             │       │ filepath     │
                  │    │ timestamp            │       │ filesize     │
                  │    └──────────────────────┘       │ status       │
                  │                                    │ timestamp    │
                  │    ┌──────────────────────┐       └──────────────┘
                  │    │    friendships        │
                  │    ├──────────────────────┤       ┌──────────────┐
                  ├───→│ requester (FK→users) │       │  groups_tbl  │
                  ├───→│ responder (FK→users) │       ├──────────────┤
                  │    │ status               │       │ id (PK)      │
                  │    │ created_at           │       │ name         │
                  │    └──────────────────────┘       │ creator (FK) │
                  │                                    │ created_at   │
                  │    ┌──────────────────────┐       └──────┬───────┘
                  │    │   group_members       │              │
                  │    ├──────────────────────┤       ┌──────┴───────┐
                  │    │ id (PK)              │       │ group_members│
                  ├───→│ group_id (FK→groups) │       ├──────────────┤
                  │    │ username (FK→users)  │       │ group_id (FK)│
                  │    │ role                 │       │ username (FK)│
                  │    │ joined_at            │       │ role         │
                  │    └──────────────────────┘       │ joined_at    │
                  │                                    └──────────────┘
                  │
            (用户是所有表的核心实体)
```

## 技术架构

| 层面 | 技术选型 |
|------|---------|
| 语言 | C++17 |
| GUI 框架 | Qt 6 Widgets |
| 网络通信 | QTcpServer / QTcpSocket（TCP 长连接） |
| 数据库 | SQLite 3（Qt SQL 模块） |
| 通信协议 | 长度前缀（4字节大端序）+ JSON 消息体 |
| 构建工具 | CMake |

### 项目结构

```
├── protocol.h              # 协议定义（消息类型、辅助函数）
├── server/
│   ├── main.cpp            # 服务器入口（守护进程）
│   ├── server.h/cpp        # TCP 服务器、消息分发
│   ├── database.h/cpp      # SQLite 数据库封装
│   └── handlers/
│       ├── auth.cpp        # 认证处理（注册/登录/注销）
│       ├── msg.cpp         # 聊天消息、文件消息、撤回、历史查询
│       ├── friend.cpp      # 好友关系处理
│       ├── group.cpp       # 群聊处理
│       └── file.cpp        # 文件传输处理
├── client/
│   ├── main.cpp            # 客户端入口（登录循环）
│   ├── clientnetwork.h/cpp # TCP 客户端封装
│   ├── logindialog.h/cpp   # 登录/注册界面
│   ├── mainwindow.h/cpp    # 主聊天界面
│   ├── widgets/
│   │   ├── chat.h/cpp      # 聊天窗口组件
│   │   └── bubble.h/cpp    # 消息气泡组件
│   └── dialogs/
│       ├── historydlg.h/cpp    # 历史查询对话框
│       ├── frienddlg.h/cpp     # 好友管理对话框
│       ├── groupcreatedlg.h/cpp # 创建群聊对话框
│       ├── memberdlg.h/cpp     # 群成员对话框
│       └── imagedlg.h/cpp      # 图片预览对话框
└── test_features.py        # 协议层自动化测试脚本
```

### 数据库表

| 表名 | 用途 |
|------|------|
| users | 用户账号信息 |
| messages | 聊天消息和文件消息记录 |
| files | 文件传输元信息 |
| friendships | 好友关系 |
| groups_tbl | 群组信息 |
| group_members | 群组成员关系 |

## 构建与运行

### 环境要求

- Ubuntu 22.04+ / Linux
- Qt 6（Core、Widgets、Network、Sql 模块）
- CMake 3.16+
- GCC 11+

### 编译

```bash
mkdir build && cd build
/home/aloha/Qt/6.11.0/gcc_64/bin/qt-cmake ..
make -j$(nproc)
```

### 启动

```bash
# 启动服务器（前台模式，可查看日志）
./chat_server -f

# 启动服务器（守护进程模式，后台运行）
./chat_server

# 启动客户端（默认连接 127.0.0.1:8888）
./chat_client

# 指定服务器地址
./chat_client 192.168.1.100 8888
```

打开多个客户端窗口即可模拟多用户聊天。

### 运行测试

```bash
# 先启动服务器
./chat_server -f &

# 运行协议层测试
python3 test_features.py
```

## 通信协议

采用长度前缀 + JSON 消息体格式：

```
[4字节大端序长度][JSON 消息体]
```

JSON 消息体结构：
```json
{
  "type": "消息类型",
  "data": { ... }
}
```

支持 30 余种消息类型，涵盖认证、聊天、文件、好友、群聊、撤回、离线消息等。

## 数据存储

- `~/.chat_server.db` — SQLite 数据库
- `~/.chat_state_用户名.json` — 已读消息状态
- `~/.chat_files/` — 服务器端文件存储
- `received_files/` — 客户端接收的文件

## 界面风格

参考微信设计，采用绿色主题（#07C160），聊天气泡使用绿色（自己）和白色（对方）配色，支持左右分栏布局。
<img width="473" height="546" alt="image" src="https://github.com/user-attachments/assets/2e130b94-9537-4332-9680-26bab14730a9" />

<img width="765" height="543" alt="image" src="https://github.com/user-attachments/assets/5a3a4e0b-dd96-4bd8-abef-339a4bb4fd7e" />


