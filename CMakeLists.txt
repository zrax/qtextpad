project(qtextpad)
cmake_minimum_required(VERSION 3.1)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTORCC TRUE)
set(CMAKE_CXX_STANDARD 11)

find_package(Qt5Core 5.4 REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(KF5SyntaxHighlighting REQUIRED)
find_package(KF5Codecs REQUIRED)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra ${CMAKE_CXX_FLAGS}")
endif()

set(qtextpad_SOURCES
    main.cpp
    appsettings.cpp
    qtextpadwindow.cpp
    settingspopup.cpp
    syntaxtextedit.cpp
)

set(qtextpad_HEADERS
    activationlabel.h
    appsettings.h
    qtextpadwindow.h
    settingspopup.h
    syntaxtextedit.h
)

set(qtextpad_RESOURCES
    qtextpad.qrc
)

add_executable(qtextpad WIN32 MACOSX_BUNDLE
               ${qtextpad_SOURCES} ${qtextpad_HEADERS} ${qtextpad_RESOURCES})
target_link_libraries(qtextpad Qt5::Core Qt5::Widgets
                               KF5::SyntaxHighlighting KF5::Codecs)
