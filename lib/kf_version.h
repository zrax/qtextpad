/* This file is part of QTextPad.
 *
 * QTextPad is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QTextPad is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QTextPad.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QTEXTPAD_KF_VERSION_H
#define QTEXTPAD_KF_VERSION_H

#include <ksyntaxhighlighting_version.h>

#if defined(KSYNTAXHIGHLIGHTING_VERSION)
#   if KSYNTAXHIGHLIGHTING_VERSION >= ((5<<16)|(56<<8)|(0))
#       define SUPPORT_DEFINITION_DOWNLOADER 1
#   endif
#elif defined(SyntaxHighlighting_VERSION)
#   if SyntaxHighlighting_VERSION >= ((5<<16)|(56<<8)|(0))
#       define SUPPORT_DEFINITION_DOWNLOADER 1
#   endif
#   define KSYNTAXHIGHLIGHTING_VERSION_STRING SyntaxHighlighting_VERSION_STRING
#else
#   error Unsupported ksyntaxhighlighting_version.h header format
#endif

#endif  // QTEXTPAD_KF_VERSION_H
