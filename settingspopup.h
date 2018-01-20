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

#ifndef _SETTINGSPOPUP_H
#define _SETTINGSPOPUP_H

#include <QWidget>
#include <Definition>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class FilteredTreePopup : public QWidget
{
    Q_OBJECT

public:
    FilteredTreePopup(QWidget *parent = Q_NULLPTR);

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QLineEdit *filter() { return m_filter; }
    QTreeWidget *tree() { return m_tree; }

public slots:
    void filterItems(const QString &text);

protected:
    void showEvent(QShowEvent *e) Q_DECL_OVERRIDE;

private:
    QLineEdit *m_filter;
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
    void syntaxItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

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
    void encodingItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
};

// Needed for serializing Definition objects into QVariant
Q_DECLARE_METATYPE(KSyntaxHighlighting::Definition)

#endif // _SETTINGSPOPUP_H
