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

if(QTEXTPAD_WIDGET_ONLY)
    add_library(qtextpad "")
    target_sources(qtextpad
        PRIVATE
            charsets.h
            charsets.cpp
            cp437codec.cpp
            syntaxhighlighter.h
            syntaxhighlighter.cpp
            syntaxtextedit.h
            syntaxtextedit.cpp
    )
else()
    add_executable(qtextpad WIN32 MACOSX_BUNDLE main.cpp)
    target_include_directories(qtextpad PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/appversion.h" "\
    namespace QTextPadVersion\n\
    {\n\
        inline QString versionString() { return QStringLiteral(\"${APP_VERSION}\"); }\n\
    }\n\
    ")

    target_sources(qtextpad
        PRIVATE
            aboutdialog.h
            aboutdialog.cpp
            activationlabel.h
            appsettings.h
            appsettings.cpp
            charsets.h
            charsets.cpp
            cp437codec.cpp
            definitiondownload.h
            definitiondownload.cpp
            ftdetect.h
            ftdetect.cpp
            indentsettings.h
            indentsettings.cpp
            qtextpadwindow.h
            qtextpadwindow.cpp
            searchdialog.h
            searchdialog.cpp
            settingspopup.h
            settingspopup.cpp
            syntaxhighlighter.h
            syntaxhighlighter.cpp
            syntaxtextedit.h
            syntaxtextedit.cpp
            undocommands.h
            undocommands.cpp

            # AUTORCC:
            qtextpad.qrc
    )

    if(WIN32)
        configure_file(app_winrc.h.in app_winrc.h @ONLY)
        target_sources(qtextpad PRIVATE qtextpad.rc)
    endif()
endif()

target_link_libraries(qtextpad
    PUBLIC
        Qt5::Core
        Qt5::Widgets
        Qt5::PrintSupport
        KF5::SyntaxHighlighting
)
