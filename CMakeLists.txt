cmake_minimum_required(VERSION 3.16)
project(SkypeClassic VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Multimedia Svg WebSockets Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED opus)

# === Client executable ===

set(CLIENT_SOURCES
    src/main.cpp
    src/app/SkypeApp.cpp
    src/models/Contact.cpp
    src/models/Message.cpp
    src/windows/LoginWindow.cpp
    src/windows/MainWindow.cpp
    src/windows/ChatWindow.cpp
    src/windows/CallWindow.cpp
    src/windows/AddContactDialog.cpp
    src/windows/SearchDialog.cpp
    src/windows/OptionsDialog.cpp
    src/windows/FileTransferDialog.cpp
    src/widgets/ContactListWidget.cpp
    src/widgets/ContactItem.cpp
    src/widgets/DialPad.cpp
    src/widgets/HistoryList.cpp
    src/widgets/StatusSelector.cpp
    src/audio/AudioStreamManager.cpp
    src/audio/VideoStreamManager.cpp
    src/audio/OpusCodec.cpp
    src/audio/JitterBuffer.cpp
    src/utils/SoundPlayer.cpp
    src/utils/CryptoUtils.cpp
    src/network/SkypeClient.cpp
    src/network/LANPeerService.cpp
    src/network/ConferenceManager.cpp
    src/windows/ConferenceCallWindow.cpp
    src/windows/GroupChatWindow.cpp
)

set(CLIENT_HEADERS
    src/app/SkypeApp.h
    src/models/Contact.h
    src/models/Message.h
    src/windows/LoginWindow.h
    src/windows/MainWindow.h
    src/windows/ChatWindow.h
    src/windows/CallWindow.h
    src/windows/AddContactDialog.h
    src/windows/SearchDialog.h
    src/windows/OptionsDialog.h
    src/windows/FileTransferDialog.h
    src/widgets/ContactListWidget.h
    src/widgets/ContactItem.h
    src/widgets/DialPad.h
    src/widgets/HistoryList.h
    src/widgets/StatusSelector.h
    src/audio/AudioStreamManager.h
    src/audio/VideoStreamManager.h
    src/audio/OpusCodec.h
    src/audio/JitterBuffer.h
    src/utils/SoundPlayer.h
    src/utils/CryptoUtils.h
    src/network/SkypeClient.h
    src/network/LANPeerService.h
    src/network/ConferenceManager.h
    src/windows/ConferenceCallWindow.h
    src/windows/GroupChatWindow.h
    src/models/GroupChat.h
)

set(RESOURCES
    resources/resources.qrc
)

add_executable(SkypeClassic ${CLIENT_SOURCES} ${CLIENT_HEADERS} ${RESOURCES})
target_include_directories(SkypeClassic PRIVATE src ${OPUS_INCLUDE_DIRS})
target_link_libraries(SkypeClassic PRIVATE Qt5::Widgets Qt5::Multimedia Qt5::Svg Qt5::WebSockets Qt5::Network ${OPUS_LIBRARIES})

# === Server executable ===

set(SERVER_SOURCES
    src/server/server_main.cpp
    src/server/ChatServer.cpp
)

set(SERVER_HEADERS
    src/server/ChatServer.h
)

add_executable(SkypeServer ${SERVER_SOURCES} ${SERVER_HEADERS})
target_include_directories(SkypeServer PRIVATE src)
target_link_libraries(SkypeServer PRIVATE Qt5::Core Qt5::WebSockets)
