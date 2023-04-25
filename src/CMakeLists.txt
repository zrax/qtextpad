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

include(CMakeDependentOption)
cmake_dependent_option(
        QTEXTPAD_USE_WIN10_ICU
        "Use the built-in Windows ICU library for Windows 1709+"
        ON "MSVC" OFF)

if(QTEXTPAD_USE_WIN10_ICU)
    # Define an import library that links the Windows ICU library
    # NOTE: This requires Windows 10 1703 or later
    add_library(ICU::uc INTERFACE IMPORTED)
    set_target_properties(ICU::uc PROPERTIES
            INTERFACE_LINK_LIBRARIES "icuuc"
            INTERFACE_COMPILE_DEFINITIONS "QTEXTPAD_USE_WIN10_ICU=1")
    add_library(ICU::data INTERFACE IMPORTED)
else()
    find_package(ICU REQUIRED COMPONENTS uc data)
    set_package_properties(ICU PROPERTIES
            URL "https://icu.unicode.org"
            DESCRIPTION "International Components for Unicode"
            TYPE REQUIRED)
endif()

add_executable(qtextpad WIN32 MACOSX_BUNDLE main.cpp)
target_include_directories(qtextpad PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/appversion.h" "\
namespace QTextPadVersion\n\
{\n\
    inline QString versionString() { return QStringLiteral(\"${APP_VERSION}\"); }\n\
}\n\
")

target_include_directories(qtextpad PRIVATE "${PROJECT_SOURCE_DIR}/lib")

target_sources(qtextpad
    PRIVATE
        aboutdialog.h
        aboutdialog.cpp
        activationlabel.h
        appsettings.h
        appsettings.cpp
        charsets.h
        charsets.cpp
        definitiondownload.h
        definitiondownload.cpp
        filetypeinfo.h
        filetypeinfo.cpp
        indentsettings.h
        indentsettings.cpp
        qtextpadwindow.h
        qtextpadwindow.cpp
        searchdialog.h
        searchdialog.cpp
        settingspopup.h
        settingspopup.cpp
        undocommands.h
        undocommands.cpp

        # AUTORCC:
        qtextpad.qrc
)

if(WIN32)
    configure_file(app_winrc.h.in app_winrc.h @ONLY)
    target_sources(qtextpad PRIVATE qtextpad.rc)
endif()

target_link_libraries(qtextpad PRIVATE syntaxtextedit ICU::uc ICU::data)
