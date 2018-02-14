# This file is part of QTextPad.
#
# QTextPad is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# QTextPad is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with QTextPad.  If not, see <http://www.gnu.org/licenses/>.

project(qtextpad)
cmake_minimum_required(VERSION 3.1)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTORCC TRUE)
set(CMAKE_CXX_STANDARD 11)

include(FeatureSummary)

find_package(Qt5Core 5.4 REQUIRED)
find_package(Qt5Widgets REQUIRED)

find_package(KF5SyntaxHighlighting REQUIRED)
set_package_properties(KF5SyntaxHighlighting PROPERTIES
        URL "https://community.kde.org/Frameworks"
        DESCRIPTION "Syntax Highlighting framework for Qt5"
        TYPE REQUIRED)

find_package(KF5Codecs REQUIRED)
set_package_properties(KF5Codecs PROPERTIES
        URL "https://community.kde.org/Frameworks"
        DESCRIPTION "Text encoding framework and extra encodings for Qt5"
        TYPE REQUIRED)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Magic)
set_package_properties(Magic PROPERTIES
        URL "https://www.darwinsys.com/file/"
        DESCRIPTION "File type identification library"
        TYPE RECOMMENDED
        PURPOSE "Provides improved auto-detection of file types and encodings")

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra ${CMAKE_CXX_FLAGS}")
endif()

set(qtextpad_SOURCES
    main.cpp
    appsettings.cpp
    ftdetect.cpp
    indentsettings.cpp
    qtextpadwindow.cpp
    searchdialog.cpp
    settingspopup.cpp
    syntaxtextedit.cpp
    undocommands.cpp
)

set(qtextpad_HEADERS
    activationlabel.h
    appsettings.h
    ftdetect.h
    indentsettings.h
    qtextpadwindow.h
    searchdialog.h
    settingspopup.h
    syntaxtextedit.h
    undocommands.h
)

set(qtextpad_RESOURCES
    qtextpad.qrc
)

if(${Magic_FOUND})
    add_definitions(-DHAVE_LIBMAGIC)
    include_directories(${Magic_INCLUDE_DIR})
endif()

add_executable(qtextpad WIN32 MACOSX_BUNDLE
               ${qtextpad_SOURCES} ${qtextpad_HEADERS} ${qtextpad_RESOURCES})
target_link_libraries(qtextpad Qt5::Core Qt5::Widgets
                               KF5::SyntaxHighlighting KF5::Codecs)

if(${Magic_FOUND})
    target_link_libraries(qtextpad ${Magic_LIBRARY})
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
