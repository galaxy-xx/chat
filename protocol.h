#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>
#include <functional>

constexpr uint16_t SERVER_PORT = 8888;
constexpr int MAX_NAME_LEN = 32;
constexpr int MAX_PAYLOAD = 65536;
constexpr int FILE_CHUNK_SIZE = 8192;

// 消息类型 —— 以 JSON 的 "type" 字段发送
// 客户端 → 服务器 请求
#define MSG_REGISTER    "register"
#define MSG_LOGIN       "login"
#define MSG_LOGOUT      "logout"
#define MSG_PRIVATE_MSG "private_msg"
#define MSG_PUBLIC_MSG  "public_msg"
#define MSG_FILE_META   "file_meta"
#define MSG_FILE_DATA   "file_data"
#define MSG_FILE_END    "file_end"
#define MSG_FILE_ACCEPT "file_accept"
#define MSG_FILE_REJECT "file_reject"
#define MSG_HISTORY     "history_query"
#define MSG_USER_LIST   "user_list"

// 好友系统
#define MSG_FRIEND_REQUEST      "friend_request"
#define MSG_FRIEND_REQUEST_RES  "friend_request_res"
#define MSG_FRIEND_INCOMING     "friend_incoming"    // 服务器推送给接收方
#define MSG_FRIEND_ACCEPT       "friend_accept"
#define MSG_FRIEND_ACCEPT_RES   "friend_accept_res"
#define MSG_FRIEND_ACCEPT_NTF   "friend_accept_ntf"  // 通知请求方对方已接受
#define MSG_FRIEND_REJECT       "friend_reject"
#define MSG_FRIEND_REJECT_RES   "friend_reject_res"
#define MSG_FRIEND_LIST         "friend_list"
#define MSG_FRIEND_LIST_RES     "friend_list_res"
#define MSG_FRIEND_REMOVE       "friend_remove"
#define MSG_FRIEND_REMOVE_RES   "friend_remove_res"
#define MSG_FRIEND_PENDING_LIST      "friend_pending_list"
#define MSG_FRIEND_PENDING_LIST_RES  "friend_pending_list_res"

// 群聊
#define MSG_GROUP_CREATE     "group_create"
#define MSG_GROUP_CREATE_RES "group_create_res"
#define MSG_GROUP_INVITE     "group_invite"
#define MSG_GROUP_INVITE_NTF "group_invite_ntf"
#define MSG_GROUP_MSG        "group_msg"
#define MSG_GROUP_LIST       "group_list"
#define MSG_GROUP_LIST_RES   "group_list_res"
#define MSG_GROUP_INVITE_RES "group_invite_res"
#define MSG_GROUP_LEAVE      "group_leave"
#define MSG_GROUP_LEAVE_RES  "group_leave_res"
#define MSG_GROUP_MEMBERS    "group_members"
#define MSG_GROUP_MEMBERS_RES "group_members_res"

// 文件消息（直接发在聊天框）
#define MSG_FILE_MSG    "file_msg"

// 消息撤回
#define MSG_RECALL      "recall"
#define MSG_RECALL_RES  "recall_res"
#define MSG_RECALL_NTF  "recall_ntf"

// 离线消息
#define MSG_OFFLINE_QUERY "offline_query"
#define MSG_OFFLINE_RES   "offline_res"

// 服务器 → 客户端 响应
#define MSG_REGISTER_RES    "register_res"
#define MSG_LOGIN_RES       "login_res"
#define MSG_LOGOUT_RES      "logout_res"
#define MSG_MESSAGE         "message"       // 转发的聊天消息
#define MSG_FILE_META_RES   "file_meta_res"
#define MSG_FILE_DATA_FWD   "file_data_fwd"
#define MSG_FILE_END_FWD    "file_end_fwd"
#define MSG_FILE_INCOMING   "file_incoming" // 通知接收方有新文件
#define MSG_HISTORY_RES     "history_res"
#define MSG_USER_LIST_RES   "user_list_res"
#define MSG_ERROR           "error"
#define MSG_USER_STATUS     "user_status"   // 用户上下线通知

// 辅助函数：将 32 位整数打包成大端字节序
inline void pack32(uint32_t val, uint8_t *buf) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

// 辅助函数：将大端字节序解包成 32 位整数
inline uint32_t unpack32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24)
         | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[2] << 8)
         | buf[3];
}

// 构建长度前缀消息：[4 字节大端长度][数据]
// 通过回调返回完整缓冲区
inline void buildPacket(const std::string &jsonStr,
                         std::function<void(const uint8_t *, size_t)> callback)
{
    uint32_t len = static_cast<uint32_t>(jsonStr.size());
    uint8_t header[4];
    pack32(len, header);
    callback(header, 4);
    callback(reinterpret_cast<const uint8_t *>(jsonStr.data()), len);
}

#endif // PROTOCOL_H
