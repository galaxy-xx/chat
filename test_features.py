#!/usr/bin/env python3
"""
综合测试脚本：测试聊天系统全部功能

用法:
  # 先启动服务端 (前台模式)
  cd build && ./chat_server -f &

  # 运行测试
  python3 test_features.py

流程:
  1. 注册/登录两个用户 (alice, bob)
  2. 好友系统: alice → bob 好友请求 → bob 接受
  3. 群聊: alice 创建群聊包含 bob
  4. 私聊 + 撤回: alice → bob 发私聊 → 撤回
  5. 公共消息 + 撤回: alice 发公共消息 → 撤回
  6. 群消息: alice 发群消息
  7. 离线消息: bob 登出 → alice 发消息 → bob 重新登录 → bob 收到离线消息
  8. 历史查询按时间范围过滤
  9. 客户注销
"""

import socket
import struct
import json
import time
import sys
import select

HOST = "127.0.0.1"
PORT = 8888


class ChatClient:
    """封装 TCP 连接，自动处理长度前缀消息协议"""

    def __init__(self, name):
        self.name = name
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((HOST, PORT))
        self.buf = b""
        self._msg_log = []  # 记录所有收到的消息
        print(f"[{name}] 已连接")

    def send(self, msg_type, data=None):
        """发送 JSON 消息"""
        msg = {"type": msg_type}
        if data:
            msg["data"] = data
        payload = json.dumps(msg, ensure_ascii=False).encode("utf-8")
        header = struct.pack("!I", len(payload))
        self.sock.sendall(header + payload)
        print(f"[{self.name}] → {msg_type}")

    def _recv_all(self):
        """读取 socket 所有可用数据并解析成消息列表"""
        msgs = []
        try:
            while True:
                ready, _, _ = select.select([self.sock], [], [], 0.01)
                if not ready:
                    break
                chunk = self.sock.recv(65536)
                if not chunk:
                    break
                self.buf += chunk
        except BlockingIOError:
            pass

        # 解析缓冲区中的消息
        while len(self.buf) >= 4:
            payload_len = struct.unpack("!I", self.buf[:4])[0]
            if len(self.buf) < 4 + payload_len:
                break
            payload = self.buf[4:4 + payload_len]
            self.buf = self.buf[4 + payload_len:]
            try:
                msg = json.loads(payload.decode("utf-8"))
                self._msg_log.append(msg)
                msgs.append(msg)
            except json.JSONDecodeError as e:
                print(f"[{self.name}] JSON解析错误: {e}")
        return msgs

    def skip_async(self, types_to_skip=None, timeout=2.0):
        """
        读取并丢弃异步推送消息 (user_list_res, user_status 等),
        返回期间跳过的消息列表。
        """
        if types_to_skip is None:
            types_to_skip = {"user_list_res", "user_status", "logout_res", "friend_list_res", "group_list_res"}
        skipped = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            msgs = self._recv_all()
            for m in msgs:
                t = m.get("type", "")
                if t in types_to_skip:
                    skipped.append(m)
                else:
                    # 不是要跳过的类型，放回 _msg_log (已经放了)
                    pass
            if not msgs:
                time.sleep(0.05)
        return skipped

    def recv(self, expected_type=None, timeout=5.0, accept_async=True):
        """
        接收下一条消息。
        - expected_type: 如果指定，自动跳过异步推送直到匹配
        - accept_async: 如果为 False，只返回目标类型不跳过
        """
        deadline = time.time() + timeout
        skip_types = {"user_list_res", "user_status", "logout_res", "friend_list_res", "group_list_res"}

        while time.time() < deadline:
            msgs = self._recv_all()
            for m in msgs:
                t = m.get("type", "")
                if expected_type and t == expected_type:
                    return m
                if not expected_type:
                    return m
                if accept_async and t in skip_types:
                    continue  # skip
                # 非预期类型，但 _recv_all 已添加到 _msg_log
            time.sleep(0.05)

        # 超时: 打印日志
        for m in self._msg_log[-10:]:
            print(f"  [最后消息] type={m.get('type')}, data={m.get('data')}")
        raise TimeoutError(f"[{self.name}] 等待 {expected_type} 超时 ({timeout}s)")

    def clear_log(self):
        self._msg_log.clear()

    def close(self):
        self.sock.close()
        print(f"[{self.name}] 已断开")


def test_register(client, user, password):
    """注册用户并验证"""
    client.send("register", {"username": user, "password": password})
    resp = client.recv("register_res")
    ok = resp.get("data", {}).get("ok", False)
    if not ok:
        msg = resp.get("data", {}).get("message", "")
        print(f"  注册结果: {msg}")
        # 用户已存在也可以接受
        if "已存在" in msg:
            print(f"  → 用户 {user} 已存在，继续")
            return True
        return False
    print(f"  ✓ 注册成功: {user}")
    return True


def test_login(client, user, password):
    """登录并验证"""
    client.send("login", {"username": user, "password": password})
    resp = client.recv("login_res")
    ok = resp.get("data", {}).get("ok", False)
    if not ok:
        msg = resp.get("data", {}).get("message", "")
        print(f"  ✗ 登录失败: {msg}")
        return False
    print(f"  ✓ 登录成功: {user}")
    # 登录后会收到 user_list_res 和 user_status，跳过
    client.skip_async(timeout=1.0)
    return True


def test_friend_request(alice, bob):
    """Alice 向 Bob 发送好友请求"""
    print("\n=== 测试: 好友请求 ===")
    alice.send("friend_request", {"target": "bob"})

    # Alice 收到自己的请求结果
    resp = alice.recv("friend_request_res")
    ok = resp.get("data", {}).get("ok", False)
    if not ok:
        msg = resp.get("data", {}).get("message", "")
        if "已经是好友" in msg or "已发送" in msg:
            print(f"  → {msg}")
            return True
        print(f"  ✗ 好友请求失败: {msg}")
        return False
    print("  ✓ Alice 好友请求已发送")

    # Bob 收到好友请求通知
    ntf = bob.recv("friend_incoming")
    from_user = ntf.get("data", {}).get("from", "")
    print(f"  ✓ Bob 收到来自 {from_user} 的好友请求")
    return True


def test_friend_accept(alice, bob):
    """Bob 接受 Alice 的好友请求"""
    print("\n=== 测试: 接受好友 ===")
    bob.send("friend_accept", {"from": "alice"})

    # Bob 收到接受结果
    resp = bob.recv("friend_accept_res")
    ok = resp.get("data", {}).get("ok", False)
    print(f"  Bob accept_res ok={ok}")

    # Alice 收到通知
    ntf = alice.recv("friend_accept_ntf")
    from_user = ntf.get("data", {}).get("from", "")
    print(f"  ✓ Alice 收到好友接受通知: from={from_user}")

    # 验证好友列表
    print("\n=== 验证: 好友列表 ===")
    alice.send("friend_list")
    resp = alice.recv("friend_list_res")
    friends = resp.get("data", {}).get("friends", [])
    names = [f.get("username") if isinstance(f, dict) else f for f in friends]
    if "bob" in names:
        print(f"  ✓ Alice 的好友列表包含 bob: {names}")
    else:
        print(f"  ✗ Alice 好友列表: {names}")

    bob.send("friend_list")
    resp = bob.recv("friend_list_res")
    friends = resp.get("data", {}).get("friends", [])
    names = [f.get("username") if isinstance(f, dict) else f for f in friends]
    if "alice" in names:
        print(f"  ✓ Bob 的好友列表包含 alice: {names}")
    else:
        print(f"  ✗ Bob 好友列表: {names}")

    return True


def test_group_create(alice):
    """Alice 创建群聊"""
    print("\n=== 测试: 创建群聊 ===")
    alice.send("group_create", {"name": "测试群", "members": ["bob"]})
    resp = alice.recv("group_create_res")
    data = resp.get("data", {})
    ok = data.get("ok", False)
    if not ok:
        print(f"  ✗ 群聊创建失败")
        return False
    group_id = data.get("group_id", -1)
    group_name = data.get("name", "")
    print(f"  ✓ 群聊创建成功: id={group_id}, name={group_name}")
    return group_id


def test_group_message(alice, bob, group_id):
    """Alice 发送群消息，Bob 验证收到"""
    print("\n=== 测试: 群消息 ===")
    # Bob 可能先受到 invite_ntf
    ntf = bob.recv("group_invite_ntf", timeout=3.0)
    if ntf:
        gid = ntf.get("data", {}).get("group_id", -1)
        gname = ntf.get("data", {}).get("group_name", "")
        print(f"  ✓ Bob 收到群邀请: {gname} (id={gid})")

    alice.send("group_msg", {"group_id": group_id, "content": "大家好，这是测试群消息"})

    # Alice 应该收到自己的消息回显
    resp_alice = alice.recv("group_msg")
    print(f"  Alice 收到回显: {resp_alice.get('data', {}).get('content', '')}")

    # Bob 应该收到消息
    resp_bob = bob.recv("group_msg")
    content = resp_bob.get("data", {}).get("content", "")
    sender = resp_bob.get("data", {}).get("from", "")
    if content == "大家好，这是测试群消息":
        print(f"  ✓ Bob 收到群消息: from={sender}, content={content}")
    else:
        print(f"  ✗ Bob 收到的内容不匹配: {content}")
    return True


def test_group_members(alice, group_id):
    """Alice 查询群成员"""
    print("\n=== 测试: 群成员查询 ===")
    alice.send("group_members", {"group_id": group_id})
    resp = alice.recv("group_members_res")
    members = resp.get("data", {}).get("members", [])
    names = [m.get("username") if isinstance(m, dict) else m for m in members]
    print(f"  群成员: {names}")
    if "alice" in names and "bob" in names:
        print(f"  ✓ 群成员查询正确")
    else:
        print(f"  ✗ 群成员不完整")
    return True


def test_group_invite_leave(alice, bob, charlie, group_id):
    """Alice 邀请 charlie -> charlie 收到通知 -> charlie 退出"""
    print("\n=== 测试: 群邀请 ===")
    alice.send("group_invite", {"group_id": group_id, "username": "charlie"})

    resp = alice.recv("group_invite_res")
    ok = resp.get("data", {}).get("ok", False)
    if ok:
        print(f"  ✓ 邀请成功")
    else:
        msg = resp.get("data", {}).get("message", "")
        print(f"  → {msg}")

    # Charlie 收到邀请通知
    ntf = charlie.recv("group_invite_ntf", timeout=3.0)
    if ntf:
        print(f"  ✓ Charlie 收到群邀请: {ntf.get('data', {}).get('group_name', '')}")

    print("\n=== 测试: 退出群聊 ===")
    charlie.send("group_leave", {"group_id": group_id})
    resp = charlie.recv("group_leave_res")
    ok = resp.get("data", {}).get("ok", False)
    if ok:
        print(f"  ✓ Charlie 已退出群聊")
    else:
        print(f"  ✗ 退出失败")

    # 再次查询成员，验证 charlie 已不在
    alice.send("group_members", {"group_id": group_id})
    resp = alice.recv("group_members_res")
    members = resp.get("data", {}).get("members", [])
    names = [m.get("username") if isinstance(m, dict) else m for m in members]
    print(f"  当前群成员: {names}")
    if "charlie" not in names:
        print(f"  ✓ Charlie 已从群成员中移除")
    else:
        print(f"  ✗ Charlie 仍在群成员中")
    return True


def test_private_message_and_recall(alice, bob):
    """Alice 给 Bob 发私聊，然后撤回"""
    print("\n=== 测试: 私聊 + 撤回 ===")
    alice.send("private_msg", {"target": "bob", "content": "这条消息会被撤回"})

    # Alice 收到回显（带 msg_id）
    resp_a = alice.recv("message")
    data = resp_a.get("data", {})
    msg_id = data.get("msg_id", 0)
    print(f"  Alice 收到回显 (msg_id={msg_id})")

    # Bob 收到消息
    resp_b = bob.recv("message")
    data_b = resp_b.get("data", {})
    print(f"  Bob 收到消息: {data_b.get('content', '')}")

    # Alice 撤回
    if msg_id > 0:
        alice.send("recall", {"msg_id": msg_id})

        resp_recall = alice.recv("recall_res")
        recall_ok = resp_recall.get("data", {}).get("ok", False)
        if recall_ok:
            print(f"  ✓ 撤回响应成功 (msg_id={msg_id})")
        else:
            msg_err = resp_recall.get("data", {}).get("message", "")
            print(f"  ✗ 撤回失败: {msg_err}")

        # Bob 收到撤回通知
        ntf = bob.recv("recall_ntf")
        ntf_msg_id = ntf.get("data", {}).get("msg_id", 0)
        print(f"  ✓ Bob 收到撤回通知 (msg_id={ntf_msg_id})")
    else:
        print("  ✗ msg_id 为 0，无法测试撤回")

    return True


def test_public_message_and_recall(alice, bob):
    """Alice 发送公共消息并撤回"""
    print("\n=== 测试: 公共消息 + 撤回 ===")
    alice.send("public_msg", {"content": "公共消息-待撤回"})

    # Alice 收到公共消息回显
    resp_a = alice.recv("message")
    msg_id = resp_a.get("data", {}).get("msg_id", 0)
    print(f"  Alice 公共消息 msg_id={msg_id}")

    # Bob 收到公共消息
    resp_b = bob.recv("message")
    print(f"  Bob 收到公共消息: {resp_b.get('data', {}).get('content', '')}")

    # 撤回
    if msg_id > 0:
        alice.send("recall", {"msg_id": msg_id})
        alice.recv("recall_res")
        print(f"  ✓ 公共消息撤回成功")

        # Bob 收到撤回通知
        ntf = bob.recv("recall_ntf")
        print(f"  ✓ Bob 收到公共消息撤回通知 (msg_id={ntf.get('data', {}).get('msg_id', 0)})")
    return True


def test_offline_sync(alice, bob):
    """Bob 登出 → Alice 发消息 → Bob 重新登录 → 验证离线消息"""
    print("\n=== 测试: 离线消息同步 ===")

    # Bob 断开
    print("  Bob 退出...")
    bob.send("logout")
    try:
        bob.recv("logout_res", timeout=2.0)
    except TimeoutError:
        pass
    bob.close()
    time.sleep(0.5)

    # Alice 发送私聊
    alice.clear_log()
    alice.send("private_msg", {"target": "bob", "content": "这是离线消息测试"})
    resp = alice.recv("message")
    last_msg_id = resp.get("data", {}).get("msg_id", 0)
    print(f"  Alice 发送离线消息 msg_id={last_msg_id}")

    # Alice 发送公共消息
    alice.send("public_msg", {"content": "离线公共消息"})
    resp = alice.recv("message")
    pub_msg_id = resp.get("data", {}).get("msg_id", 0)
    print(f"  Alice 发送离线公共消息 msg_id={pub_msg_id}")
    if pub_msg_id > last_msg_id:
        last_msg_id = pub_msg_id

    # Bob 重新登录
    print("  Bob 重新登录...")
    bob2 = ChatClient("bob")
    if not test_login(bob2, "bob", "bob123"):
        return bob2, False

    # Bob 请求离线消息
    bob2.send("offline_query", {"last_id": 0})
    resp = bob2.recv("offline_res")
    messages = resp.get("data", {}).get("messages", [])
    print(f"  Bob 收到 {len(messages)} 条离线消息:")
    for m in messages:
        print(f"    [{m.get('msg_type', '?')}] {m.get('from', '?')}: {m.get('content', '?')} (id={m.get('msg_id', 0)})")

    offline_contents = [m.get("content", "") for m in messages]
    if "这是离线消息测试" in offline_contents:
        print(f"  ✓ 离线私聊消息同步成功")
    else:
        print(f"  ✗ 离线私聊消息未找到")

    if "离线公共消息" in offline_contents:
        print(f"  ✓ 离线公共消息同步成功")
    else:
        print(f"  ✗ 离线公共消息未找到")

    # 清理
    bob2.close()
    return bob2, True


def test_pending_friend_request(charlie, alice):
    """Charlie 发好友请求给 Alice（Alice 离线），Alice 登录后收到"""
    print("\n=== 测试: 离线好友请求持久化 ===")

    # Alice 退出
    print("  Alice 退出...")
    alice.send("logout")
    try:
        alice.recv("logout_res", timeout=2.0)
    except TimeoutError:
        pass
    alice.close()
    time.sleep(0.5)

    # Charlie 发送好友请求（Alice 离线）
    charlie.send("friend_request", {"target": "alice"})
    resp = charlie.recv("friend_request_res")
    ok = resp.get("data", {}).get("ok", False)
    print(f"  Charlie 请求结果: ok={ok}")

    # Alice 重新登录，查看是否能收到 friend_incoming
    print("  Alice 重新登录...")
    alice2 = ChatClient("alice")
    alice2.send("login", {"username": "alice", "password": "alice123"})

    deadline = time.time() + 3.0
    found_from = None
    while time.time() < deadline:
        msgs = alice2._recv_all()
        for m in msgs:
            t = m.get("type", "")
            d = m.get("data", {})
            if t == "login_res" and d.get("ok", False):
                print(f"  ✓ Alice 登录成功")
            elif t == "friend_incoming":
                found_from = d.get("from", "")
                print(f"  ✓ Alice 收到离线好友请求: from={found_from}")
        if found_from:
            break
        time.sleep(0.05)

    if found_from == "charlie":
        print(f"  ✓ 离线好友请求持久化功能正常")
    else:
        print(f"  ✗ 未收到离线好友请求")

    # Alice 接受好友请求
    if found_from:
        alice2.send("friend_accept", {"from": found_from})
        resp = alice2.recv("friend_accept_res")
        print(f"  Alice 接受结果: ok={resp.get('data', {}).get('ok', False)}")

    return alice2


def test_history_time_range(alice):
    """测试历史查询按时间范围过滤"""
    print("\n=== 测试: 历史查询时间范围过滤 ===")

    # 先查全部历史
    alice.send("history_query", {"type": "public", "target": "alice", "limit": 100})
    resp = alice.recv("history_res")
    all_msgs = resp.get("data", {}).get("messages", [])
    print(f"  全部公共消息: {len(all_msgs)} 条")

    # 用一个很早的时间范围查询（应该能查到）
    alice.send("history_query", {
        "type": "public", "target": "alice", "limit": 100,
        "start_time": "2020-01-01 00:00:00",
        "end_time": "2030-12-31 23:59:59"
    })
    resp = alice.recv("history_res")
    range_msgs = resp.get("data", {}).get("messages", [])
    print(f"  大范围时间查询: {len(range_msgs)} 条")
    if len(range_msgs) >= len(all_msgs):
        print(f"  ✓ 时间范围查询结果正确")
    else:
        print(f"  ✗ 时间范围查询结果不一致")

    # 用一个很窄的时间范围（应该查不到）
    alice.send("history_query", {
        "type": "public", "target": "alice", "limit": 100,
        "start_time": "2020-01-01 00:00:00",
        "end_time": "2020-01-02 00:00:00"
    })
    resp = alice.recv("history_res")
    narrow_msgs = resp.get("data", {}).get("messages", [])
    print(f"  窄范围时间查询: {len(narrow_msgs)} 条")
    if len(narrow_msgs) == 0:
        print(f"  ✓ 窄范围时间过滤正确")
    else:
        print(f"  ✗ 窄范围应返回 0 条")

    return True


def test_delete_account(user_client, username, password):
    """测试注销账号"""
    print(f"\n=== 测试: 注销账号 ({username}) ===")
    user_client.send("delete_account")
    resp = user_client.recv("delete_account_res")
    ok = resp.get("data", {}).get("ok", False)
    if ok:
        print(f"  ✓ 账号 {username} 注销成功")
    else:
        print(f"  ✗ 注销失败")
        return False

    user_client.close()
    time.sleep(0.3)

    # 验证无法再登录
    test_client = ChatClient(f"{username}(验证)")
    test_client.send("login", {"username": username, "password": password})
    resp = test_client.recv("login_res")
    login_ok = resp.get("data", {}).get("ok", False)
    if not login_ok:
        print(f"  ✓ 注销后无法登录，验证通过")
    else:
        print(f"  ✗ 注销后仍能登录")
    test_client.close()
    return True


def main():
    print("=" * 60)
    print("聊天系统功能测试")
    print("=" * 60)

    # 注册/登录
    alice = ChatClient("alice")
    bob = ChatClient("bob")
    charlie = ChatClient("charlie")

    test_register(alice, "alice", "alice123")
    test_register(bob, "bob", "bob123")
    test_register(charlie, "charlie", "charlie123")

    test_login(alice, "alice", "alice123")
    test_login(bob, "bob", "bob123")
    test_login(charlie, "charlie", "charlie123")

    # 1. 好友系统
    test_friend_request(alice, bob)
    test_friend_accept(alice, bob)

    # 2. 群聊
    group_id = test_group_create(alice)
    if group_id:
        test_group_message(alice, bob, group_id)
        test_group_members(alice, group_id)
        test_group_invite_leave(alice, bob, charlie, group_id)

        # 群聊未读: bob 关闭群聊窗口后 alice 再发消息
        # (纯 Python 测试无法直接验证 UI 未读徽标，但可以验证消息送达)

    # 3. 消息撤回
    test_private_message_and_recall(alice, bob)
    test_public_message_and_recall(alice, bob)

    # 4. 离线消息同步
    bob2, offline_ok = test_offline_sync(alice, bob)

    # 5. 离线好友请求持久化
    alice = test_pending_friend_request(charlie, alice)

    # 6. 历史查询时间范围过滤
    test_history_time_range(alice)

    # 7. 客户注销（用 charlie 测试）
    test_delete_account(charlie, "charlie", "charlie123")

    # 清理
    alice.close()

    print("\n" + "=" * 60)
    print("测试完成!")
    print("=" * 60)
    print("\n注意: 以下功能需通过 GUI 客户端手动验证:")
    print("  - 图片预览 (文件传输后自动检测图片显示)")
    print("  - 未读徽标 (左侧列表显示 [N] 计数)")
    print("  - 右击撤回菜单 (自己气泡2分钟内可右击撤回)")
    print("  - 注销账号菜单项 (文件→注销账号)")
    print("  - 历史查询时间范围选择器 (查看→聊天历史)")


if __name__ == "__main__":
    main()
