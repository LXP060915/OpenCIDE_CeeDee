#include "codeeditor.h"
#include <QPainter>
#include <QTextBlock>
#include "CppHighlighter.h"
#include <QStack>
#include <QPair>

// ---------------- CodeEditor ----------------
CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    new CppHighlighter(this->document());
}

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) { max /= 10; ++digits; }
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (dy == 0)
        lineNumberArea->update();
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// ---------------- 行高亮 + 括号匹配高亮 ----------------

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // 当前行高亮
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection lineSel;
        lineSel.format.setBackground(QColor(Qt::yellow).lighter(160));
        lineSel.format.setProperty(QTextFormat::FullWidthSelection, true);
        lineSel.cursor = textCursor();
        lineSel.cursor.clearSelection();
        extraSelections.append(lineSel);
    }

    QString text = document()->toPlainText();
    int pos = textCursor().position();
    if (text.isEmpty() || pos < 0 || pos >= text.size()) return;

    // 只检查光标直接所在的字符是否是括号
    QChar charAtPos = text.at(pos);
    bool isBracket = (charAtPos == '(' || charAtPos == ')' ||
                     charAtPos == '{' || charAtPos == '}' ||
                     charAtPos == '[' || charAtPos == ']');

    // 如果不是括号，直接返回
    if (!isBracket) {
        setExtraSelections(extraSelections);
        return;
    }

    // 查找匹配的括号
    int matchPos = findMatchingBracket(text, pos);
    if (matchPos == -1) {
        setExtraSelections(extraSelections);
        return;
    }

    // 高亮匹配的括号
    auto makeSelection = [&](int p) -> QTextEdit::ExtraSelection {
        QTextEdit::ExtraSelection sel;
        QTextCursor cursor(document());
        cursor.setPosition(p);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        sel.cursor = cursor;
        sel.format.setBackground(QColor(Qt::green).lighter(160));
        sel.format.setFontWeight(QFont::Bold);
        return sel;
    };

    extraSelections.append(makeSelection(pos));
    extraSelections.append(makeSelection(matchPos));

    setExtraSelections(extraSelections);
}

int CodeEditor::findMatchingBracket(const QString& text, int pos)
{
    if (pos < 0 || pos >= text.length()) return -1;

    QChar bracket = text.at(pos);

    // 确定括号类型和搜索方向
    QChar targetBracket;
    int direction;

    if (bracket == '(') {
        targetBracket = ')';
        direction = 1;
    } else if (bracket == ')') {
        targetBracket = '(';
        direction = -1;
    } else if (bracket == '{') {
        targetBracket = '}';
        direction = 1;
    } else if (bracket == '}') {
        targetBracket = '{';
        direction = -1;
    } else if (bracket == '[') {
        targetBracket = ']';
        direction = 1;
    } else if (bracket == ']') {
        targetBracket = '[';
        direction = -1;
    } else {
        return -1; // 不是括号
    }

    int depth = 0;
    int i = pos + direction;

    enum State { Normal, InString, InChar, InLineComment, InBlockComment };
    State state = Normal;
    QChar stringQuote;

    while (i >= 0 && i < text.length()) {
        QChar c = text.at(i);

        // 处理状态转换
        switch (state) {
            case Normal:
                if (c == '"' || c == '\'') {
                    state = (c == '"') ? InString : InChar;
                    stringQuote = c;
                } else if (c == '/' && i+1 < text.length()) {
                    if (text.at(i+1) == '/') {
                        state = InLineComment;
                        i++; // 跳过下一个字符
                    } else if (text.at(i+1) == '*') {
                        state = InBlockComment;
                        i++; // 跳过下一个字符
                    }
                } else if (c == bracket) {
                    // 遇到同类型的括号，增加深度
                    depth++;
                } else if (c == targetBracket) {
                    if (depth == 0) {
                        // 找到匹配的括号
                        return i;
                    }
                    depth--; // 减少深度
                }
                break;

            case InString:
            case InChar:
                if (c == stringQuote && (i == 0 || text.at(i-1) != '\\')) {
                    state = Normal;
                }
                break;

            case InLineComment:
                if (c == '\n') {
                    state = Normal;
                }
                break;

            case InBlockComment:
                if (c == '*' && i+1 < text.length() && text.at(i+1) == '/') {
                    state = Normal;
                    i++; // 跳过下一个字符
                }
                break;
        }

        i += direction;
    }

    return -1; // 没有找到匹配的括号
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int topMargin = contentOffset().y();
    int top = (int)blockBoundingGeometry(block).translated(0, topMargin).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width() - 2, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// ---------------- 自动补全括号 ----------------
void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    QTextCursor cursor = textCursor();
    QChar ch = event->text().isEmpty() ? QChar() : event->text().at(0);

    // 自动补全的括号对
    QMap<QChar, QChar> brackets = {
        {'(', ')'},
        {'[', ']'},
        {'{', '}'},
        {'"', '"'},
        {'\'', '\''},
        {'<', '>'},
    };

    if (brackets.contains(ch)) {
        // 插入左括号
        QPlainTextEdit::keyPressEvent(event);

        // 插入匹配的右括号
        QChar right = brackets[ch];
        cursor = textCursor();
        cursor.insertText(right);

        // 将光标移动到左括号和右括号中间
        cursor.movePosition(QTextCursor::Left);
        setTextCursor(cursor);
        return;
    }

    // 如果输入的是右括号，并且光标右侧已经有相同的右括号，则跳过插入，直接移动光标
    if (brackets.values().contains(ch)) {
        cursor = textCursor();
        if (!cursor.atEnd() && cursor.document()->characterAt(cursor.position()) == ch) {
            cursor.movePosition(QTextCursor::Right);
            setTextCursor(cursor);
            return;
        }
    }

    // 其他按键正常处理
    QPlainTextEdit::keyPressEvent(event);
}


// ---------------- LineNumberArea ----------------
LineNumberArea::LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor)
{
    setMouseTracking(true);
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    codeEditor->lineNumberAreaPaintEvent(event);
}

bool LineNumberArea::event(QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress ||
        e->type() == QEvent::MouseButtonDblClick ||
        e->type() == QEvent::MouseButtonRelease) {
        return true;
    }
    return QWidget::event(e);
}

