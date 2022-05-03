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

add_library(syntaxtextedit "")
target_sources(syntaxtextedit
    PRIVATE
        charsets.h
        charsets.cpp
        filetypeinfo.h
        filetypeinfo.cpp
        syntaxhighlighter.h
        syntaxhighlighter.cpp
        syntaxtextedit.h
        syntaxtextedit.cpp
)

target_link_libraries(syntaxtextedit
    PUBLIC
        Qt::Core
        Qt::Widgets
        Qt::PrintSupport
        KF5::SyntaxHighlighting
        ICU::uc
)

if(TARGET Qt::Core5Compat)
    target_link_libraries(syntaxtextedit PUBLIC Qt::Core5Compat)
endif()
