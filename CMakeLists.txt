cmake_minimum_required(VERSION 3.19)
project(chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ============================================================
# Chat Server (daemon)
# ============================================================
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Network Sql)

qt_add_executable(chat_server
    server/main.cpp
    server/database.h
    server/database.cpp
    server/server.h
    server/server.cpp
    server/handlers/auth.cpp
    server/handlers/msg.cpp
    server/handlers/friend.cpp
    server/handlers/group.cpp
    server/handlers/file.cpp
)

target_link_libraries(chat_server
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Sql
)

target_include_directories(chat_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS chat_server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================
# Chat Client (Qt GUI)
# ============================================================
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network Sql)

qt_add_executable(chat_client
    client/main.cpp
    client/mainwindow.h
    client/mainwindow.cpp
    client/clientnetwork.h
    client/clientnetwork.cpp
    client/logindialog.h
    client/logindialog.cpp
    client/widgets/bubble.h
    client/widgets/bubble.cpp
    client/widgets/chat.h
    client/widgets/chat.cpp
    client/dialogs/frienddlg.h
    client/dialogs/frienddlg.cpp
    client/dialogs/groupcreatedlg.h
    client/dialogs/groupcreatedlg.cpp
    client/dialogs/memberdlg.h
    client/dialogs/memberdlg.cpp
    client/dialogs/historydlg.h
    client/dialogs/historydlg.cpp
    client/dialogs/imagedlg.h
    client/dialogs/imagedlg.cpp
)

target_link_libraries(chat_client
    PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
)

target_include_directories(chat_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS chat_client
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================
# Deploy (disabled for development)
# ============================================================
