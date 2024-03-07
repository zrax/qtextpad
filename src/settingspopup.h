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

#ifndef QTEXTPAD_SETTINGSPOPUP_H
#define QTEXTPAD_SETTINGSPOPUP_H

#include <QIcon>
#include <QLineEdit>
#include <KSyntaxHighlighting/Definition>

class QTreeWidget;
class QTreeWidgetItem;

class TreeFilterEdit : public QLineEdit
{
    Q_OBJECT

public:
    TreeFilterEdit(QWidget *parent = Q_NULLPTR);

    QSize sizeHint() const Q_DECL_OVERRIDE;

signals:
    void navigateDown();

protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

private:
    QIcon m_searchIcon;
    QPoint m_iconPosition;

    void recomputeIconPos();
};

class FilteredTreePopup : public QWidget
{
    Q_OBJECT

public:
    FilteredTreePopup(QWidget *parent = Q_NULLPTR);

    QSize sizeHint() const Q_DECL_OVERRIDE;
    TreeFilterEdit *filter() { return m_filter; }
    QTreeWidget *tree() { return m_tree; }

public slots:
    void filterItems(const QString &text);

protected:
    void showEvent(QShowEvent *e) Q_DECL_OVERRIDE;
    bool focusNextPrevChild(bool next) Q_DECL_OVERRIDE;

private:
    TreeFilterEdit *m_filter;
    QTreeWidget *m_tree;
};

class SyntaxPopup : public FilteredTreePopup
{
    Q_OBJECT

public:
    SyntaxPopup(QWidget *parent = Q_NULLPTR);

signals:
    void syntaxSelected(const KSyntaxHighlighting::Definition &syntax);

private slots:
    void syntaxItemChosen(QTreeWidgetItem *current, int column);

private:
    QTreeWidgetItem *m_plainTextItem;
};

class EncodingPopup : public FilteredTreePopup
{
    Q_OBJECT

public:
    EncodingPopup(QWidget *parent = Q_NULLPTR);

signals:
    void encodingSelected(const QString &codecName);

private slots:
    void encodingItemChosen(QTreeWidgetItem *current, int column);
};

// Needed for serializing Definition objects into QVariant
Q_DECLARE_METATYPE(KSyntaxHighlighting::Definition)

#endif // QTEXTPAD_SETTINGSPOPUP_H
