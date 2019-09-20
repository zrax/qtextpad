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

#include "searchdialog.h"

#include <QIcon>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QCompleter>
#include <QMessageBox>

#include "qtextpadwindow.h"
#include "syntaxtextedit.h"
#include "appsettings.h"

static SearchDialog *s_instance = Q_NULLPTR;

struct SessionSettings
{
    QString searchText;
    bool caseSensitive, wholeWord, regex, escapes;
    bool wrapSearch;
    SyntaxTextEdit *editor;

    SessionSettings()
        : caseSensitive(), wholeWord(), regex(), escapes(),
          wrapSearch(), editor()
    { }
};

static SessionSettings *session()
{
    static SessionSettings s_session;
    return &s_session;
}

/* Just sets some more sane defaults for QComboBox:
 * - Don't auto-insert items (we handle that manually)
 * - Disable the completer, since it insists on changing the a typed
 *   item to match another item in the list that differs only in case.
 */
class SearchComboBox : public QComboBox
{
public:
    explicit SearchComboBox(QWidget *parent)
        : QComboBox(parent)
    {
        setEditable(true);
        setInsertPolicy(QComboBox::NoInsert);
        setDuplicatesEnabled(true);
        setCompleter(Q_NULLPTR);
    }
};

SearchDialog::SearchDialog(QWidget *parent)
    : QDialog(parent)
{
    s_instance = this;
    setAttribute(Qt::WA_DeleteOnClose);

    QTextPadSettings settings;

    m_searchText = new SearchComboBox(this);
    m_searchText->addItems(settings.recentSearches());
    m_searchText->setCurrentText(QString());

    m_replaceText = new SearchComboBox(this);
    m_replaceText->addItems(settings.recentSearchReplacements());
    m_replaceText->setCurrentText(QString());
    m_replaceWidgets.append(m_replaceText);

    m_caseSensitive = new QCheckBox(tr("Match ca&se"), this);
    m_caseSensitive->setChecked(settings.searchCaseSensitive());
    m_wholeWord = new QCheckBox(tr("Match &whole words"), this);
    m_wholeWord->setChecked(settings.searchWholeWord());
    m_regex = new QCheckBox(tr("Regular e&xpressions"), this);
    m_regex->setChecked(settings.searchRegex());
    m_escapes = new QCheckBox(tr("&Escape sequences"), this);
    m_escapes->setChecked(settings.searchEscapes());
    m_wrapSearch = new QCheckBox(tr("Wrap Aro&und"), this);
    m_wrapSearch->setChecked(settings.searchWrap());

    m_toggleReplace = new QLabel(this);
    m_toggleReplace->setOpenExternalLinks(false);
    connect(m_toggleReplace, &QLabel::linkActivated, this, [this](const QString &) {
        showReplace(!m_replaceWidgets.front()->isVisible());
    });

    // QDialogButtonBox insists on rearranging buttons depending on your platform,
    // which would be fine if we only had standard actions, but most of our
    // action buttons here are custom.
    auto buttonBox = new QWidget(this);
    buttonBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    auto buttonLayout = new QVBoxLayout(buttonBox);
    buttonLayout->setMargin(0);
    buttonLayout->setSpacing(5);
    auto findNext = new QPushButton(tr("Find &Next"), this);
    buttonLayout->addWidget(findNext);
    auto findPrev = new QPushButton(tr("Find &Previous"), this);
    buttonLayout->addWidget(findPrev);
    auto replaceNext = new QPushButton(tr("&Replace"), this);
    buttonLayout->addWidget(replaceNext);
    m_replaceWidgets.append(replaceNext);
    auto replaceAll = new QPushButton(tr("Replace &All"), this);
    buttonLayout->addWidget(replaceAll);
    m_replaceWidgets.append(replaceAll);
    auto replaceSelection = new QPushButton(tr("&In Selection"), this);
    buttonLayout->addWidget(replaceSelection);
    m_replaceWidgets.append(replaceSelection);
    buttonLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    auto closeButton = new QPushButton(tr("&Close"), this);
    buttonLayout->addWidget(closeButton);

    connect(findNext, &QPushButton::clicked, this, &SearchDialog::searchForward);
    connect(findPrev, &QPushButton::clicked, this, &SearchDialog::searchBackward);
    connect(replaceNext, &QPushButton::clicked, this, &SearchDialog::replaceCurrent);
    connect(replaceAll, &QPushButton::clicked, this, &SearchDialog::replaceAll);
    connect(replaceSelection, &QPushButton::clicked, this, &SearchDialog::replaceInSelection);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

    auto layout = new QGridLayout(this);
    layout->setMargin(10);
    layout->setVerticalSpacing(5);
    layout->setHorizontalSpacing(10);
    auto searchLabel = new QLabel(tr("&Find:"), this);
    searchLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    searchLabel->setBuddy(m_searchText);
    layout->addWidget(searchLabel, 0, 0);
    layout->addWidget(m_searchText, 0, 1, 1, 2);
    auto replaceLabel = new QLabel(tr("Replace wit&h:"), this);
    replaceLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    replaceLabel->setBuddy(m_replaceText);
    m_replaceWidgets.append(replaceLabel);
    layout->addWidget(replaceLabel, 1, 0);
    layout->addWidget(m_replaceText, 1, 1, 1, 2);
    layout->addItem(new QSpacerItem(0, 10, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed),
                    2, 0, 1, 3);
    layout->addWidget(m_caseSensitive, 3, 1);
    layout->addWidget(m_wholeWord, 4, 1);
    layout->addWidget(m_regex, 5, 1);
    layout->addWidget(m_escapes, 6, 1);
    layout->addWidget(m_wrapSearch, 3, 2);
    layout->addWidget(m_toggleReplace, 6, 2);
    layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding),
                    layout->rowCount(), 0, 1, 3);
    layout->addWidget(buttonBox, 0, layout->columnCount(), layout->rowCount(), 1);
}

SearchDialog::~SearchDialog()
{
    syncSearchSettings(false);
    s_instance = Q_NULLPTR;
}

SearchDialog *SearchDialog::create(QTextPadWindow *parent, bool replace)
{
    if (s_instance) {
        s_instance->raise();
        s_instance->showReplace(replace);
    } else {
        s_instance = new SearchDialog(parent);
        s_instance->showReplace(replace);
        s_instance->show();
        s_instance->raise();
        s_instance->activateWindow();
    }

    if (parent->editor()->textCursor().hasSelection()) {
        QTextCursor cursor = parent->editor()->textCursor();
        s_instance->m_searchText->setCurrentText(cursor.selectedText());
        s_instance->m_searchText->lineEdit()->selectAll();
    }

    return s_instance;
}

SearchDialog *SearchDialog::current()
{
    return s_instance;
}

SyntaxTextEdit *SearchDialog::editor()
{
    auto window = qobject_cast<QTextPadWindow *>(parent());
    Q_ASSERT(window);
    return window->editor();
}

void SearchDialog::showReplace(bool show)
{
    if (show) {
        setWindowTitle(tr("Replace..."));
        setWindowIcon(ICON("edit-find-replace"));
    } else {
        setWindowTitle(tr("Find..."));
        setWindowIcon(ICON("edit-find"));
    }

    for (auto widget : m_replaceWidgets)
        widget->setVisible(show);

    m_toggleReplace->setText(show ? tr("<a href=\"#\">Switch to Find</a>")
                                  : tr("<a href=\"#\">Switch to Replace</a>"));
    adjustSize();
}

QString translateCharEscape(const QStringRef &digits, int *advance)
{
    Q_ASSERT(digits.size() > 0);
    Q_ASSERT(advance);

    *advance = 0;
    if (digits.at(0) == QLatin1Char('x')) {
        // We only support exactly 2 hex bytes with \x format
        if (digits.size() < 3)
            return QString();
        QByteArray number = digits.mid(1, 2).toLatin1();
        char *end;
        ulong ch = strtoul(number.constData(), &end, 16);
        if (*end != '\0')
            return QString();
        *advance = 2;
        return QString(QChar::fromLatin1(static_cast<char>(ch)));
    } else if (digits.at(0) == QLatin1Char('u')) {
        if (digits.size() < 5)
            return QString();
        QByteArray number = digits.mid(1, 4).toLatin1();
        char *end;
        ulong ch = strtoul(number.constData(), &end, 16);
        if (*end != '\0')
            return QString();
        *advance = 4;
        return QString(QChar(static_cast<ushort>(ch)));
    } else if (digits.at(0) == QLatin1Char('U')) {
        if (digits.size() < 9)
            return QString();
        QByteArray number = digits.mid(1, 8).toLatin1();
        char *end;
        ulong ch = strtoul(number.constData(), &end, 16);
        if (*end != '\0')
            return QString();

        *advance = 8;
        if (ch > 0xFFFFU) {
            const QChar utf16[2] = {
                QChar::highSurrogate(ch),
                QChar::lowSurrogate(ch),
            };
            return QString(utf16, 2);
        } else {
            return QString(QChar(static_cast<ushort>(ch)));
        }
    } else {
        // Octal codes can be 1, 2, or 3 characters long...
        QByteArray number = digits.mid(0, 3).toLatin1();
        char *end;
        ulong ch = strtoul(number.constData(), &end, 8);
        *advance = static_cast<int>(end - number.constData());
        if (*advance == 0 || ch > 0xFFU)
            return QString();
        *advance -= 1;      // The first digit was already accounted for
        return QString(QChar::fromLatin1(static_cast<char>(ch)));
    }
}

QString translateEscapes(const QString &text)
{
    QString result;
    result.reserve(text.size());
    int start = 0;
    for ( ;; ) {
        int pos = text.indexOf(QLatin1Char('\\'), start);
        if (pos < 0 || pos + 1 >= text.size())
            break;

        result.append(text.midRef(start, pos - start));
        QChar next = text.at(pos + 1);
        start = pos + 2;
        switch (next.unicode()) {
        case 'a':
            result.append(QLatin1Char('\a'));
            break;
        case 'b':
            result.append(QLatin1Char('\b'));
            break;
        case 'e':
            result.append(QLatin1Char('\x1b'));
            break;
        case 'f':
            result.append(QLatin1Char('\f'));
            break;
        case 'n':
            result.append(QLatin1Char('\n'));
            break;
        case 'r':
            result.append(QLatin1Char('\r'));
            break;
        case 't':
            result.append(QLatin1Char('\t'));
            break;
        case 'v':
            result.append(QLatin1Char('\v'));
            break;
        case '\\':
        case '?':
        case '\'':
        case '"':
            result.append(next);
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':   // Octal code
        case 'x':   // Hex byte
        case 'u':   // Unicode character (16-bit)
        case 'U':   // Unicode character (32-bit)
            {
                int advance;
                const QString chars = translateCharEscape(text.midRef(pos + 1), &advance);
                if (chars.isEmpty()) {
                    // Translation failed
                    result.append(QLatin1Char('\\'));
                    result.append(next);
                } else {
                    result.append(chars);
                    start += advance;
                }
            }
            break;
        default:
            // Just keep unrecognized sequences untranslated
            result.append(QLatin1Char('\\'));
            result.append(next);
            break;
        }
    }

    result.append(text.midRef(start));
    return result;
}

void SearchDialog::syncSearchSettings(bool saveRecent)
{
    QTextPadSettings settings;

    session()->editor = editor();

    const QString searchText = m_searchText->currentText();
    if (!searchText.isEmpty() && saveRecent) {
        if (m_searchText->count() == 0 || m_searchText->itemText(0) != searchText) {
            settings.addRecentSearch(searchText);
            m_searchText->insertItem(0, searchText);
        }
    }
    session()->searchText = searchText;

    const QString replaceText = m_replaceText->currentText();
    if (!replaceText.isEmpty() && saveRecent) {
        if (m_replaceText->count() == 0 || m_replaceText->itemText(0) != replaceText) {
            settings.addRecentSearchReplacement(replaceText);
            m_replaceText->insertItem(0, replaceText);
        }
    }

    session()->caseSensitive = m_caseSensitive->isChecked();
    session()->wholeWord = m_wholeWord->isChecked();
    session()->regex = m_regex->isChecked();
    session()->escapes = m_escapes->isChecked();
    session()->wrapSearch = m_wrapSearch->isChecked();

    settings.setSearchCaseSensitive(m_caseSensitive->isChecked());
    settings.setSearchWholeWord(m_wholeWord->isChecked());
    settings.setSearchRegex(m_regex->isChecked());
    settings.setSearchEscapes(m_escapes->isChecked());
    settings.setSearchWrap(m_wrapSearch->isChecked());
}

QTextCursor SearchDialog::performSearch(const QTextCursor &start, bool reverse)
{
    if (!session()->editor)
        return QTextCursor();

    QTextDocument::FindFlags searchOptions = 0;
    if (session()->caseSensitive)
        searchOptions |= QTextDocument::FindCaseSensitively;
    if (session()->wholeWord)
        searchOptions |= QTextDocument::FindWholeWords;
    if (reverse)
        searchOptions |= QTextDocument::FindBackward;

    QString text = session()->searchText;
    if (session()->escapes)
        text = translateEscapes(text);

    auto document = session()->editor->document();
    if (session()->regex) {
        QRegularExpression::PatternOptions csOption = session()->caseSensitive
                        ? QRegularExpression::NoPatternOption
                        : QRegularExpression::CaseInsensitiveOption;
        return document->find(QRegularExpression(text, csOption),
                              start, searchOptions);

    } else {
        return document->find(text, start, searchOptions);
    }
}

QTextCursor SearchDialog::searchNext(QTextPadWindow *parent, bool reverse)
{
    if (!session()->editor || session()->searchText.isEmpty()) {
        create(parent, false);
        return QTextCursor();
    }

    SyntaxTextEdit *editor = session()->editor;
    auto searchCursor = performSearch(editor->textCursor(), reverse);
    if (searchCursor.isNull() && session()->wrapSearch) {
        QTextCursor wrapCursor = editor->textCursor();
        wrapCursor.movePosition(reverse ? QTextCursor::End : QTextCursor::Start);
        searchCursor = performSearch(wrapCursor, reverse);
    }

    if (searchCursor.isNull()) {
        QMessageBox::information(parent, QString(),
                                 tr("The specified text was not found"));
    } else {
        editor->setTextCursor(searchCursor);
    }
    return searchCursor;
}

void SearchDialog::searchForward()
{
    syncSearchSettings(true);
    m_replaceCursor = searchNext(Q_NULLPTR, false);
}

void SearchDialog::searchBackward()
{
    syncSearchSettings(true);
    m_replaceCursor = searchNext(Q_NULLPTR, true);
}

void SearchDialog::replaceCurrent()
{
    syncSearchSettings(true);

    const QString searchText = m_searchText->currentText();
    if (searchText.isEmpty())
        return;

    if (m_replaceCursor.isNull() || m_replaceCursor != editor()->textCursor()) {
        m_replaceCursor = searchNext(Q_NULLPTR, false);
        return;
    }

    QString replaceText = m_replaceText->currentText();
    if (m_escapes->isChecked())
        replaceText = translateEscapes(replaceText);

    m_replaceCursor.beginEditBlock();
    m_replaceCursor.removeSelectedText();
    m_replaceCursor.insertText(replaceText);
    m_replaceCursor.endEditBlock();

    m_replaceCursor = searchNext(Q_NULLPTR, false);
}

void SearchDialog::performReplaceAll(bool inSelection)
{
    syncSearchSettings(true);

    const QString searchText = m_searchText->currentText();
    if (searchText.isEmpty())
        return;

    auto searchCursor = editor()->textCursor();
    if (inSelection)
        searchCursor.setPosition(editor()->textCursor().selectionStart());
    else
        searchCursor.movePosition(QTextCursor::Start);
    searchCursor = performSearch(searchCursor, false);
    if (searchCursor.isNull()) {
        QMessageBox::information(qobject_cast<QWidget *>(parent()), QString(),
                                 tr("The specified text was not found"));
        return;
    } else if (inSelection && searchCursor.selectionEnd() > editor()->textCursor().selectionEnd()) {
        QMessageBox::information(qobject_cast<QWidget *>(parent()), QString(),
                                 tr("The specified text was not found in the selection"));
        return;
    }

    QString replaceText = m_replaceText->currentText();
    if (m_escapes->isChecked())
        replaceText = translateEscapes(replaceText);

    searchCursor.beginEditBlock();
    auto replaceCursor = searchCursor;
    int replacements = 0;
    while (!replaceCursor.isNull()) {
        if (inSelection && replaceCursor.selectionEnd() > editor()->textCursor().selectionEnd())
            break;

        replaceCursor.removeSelectedText();
        replaceCursor.insertText(replaceText);
        replaceCursor = performSearch(replaceCursor, false);
        ++replacements;
    }
    searchCursor.endEditBlock();

    QMessageBox::information(qobject_cast<QWidget *>(parent()), QString(),
                             tr("Successfully replaced %1 matches").arg(replacements));
}

void SearchDialog::replaceAll()
{
    performReplaceAll(false);
}

void SearchDialog::replaceInSelection()
{
    performReplaceAll(true);
}
