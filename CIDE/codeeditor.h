#pragma once

#include <QPlainTextEdit>
#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextBlock>
#include <QEvent>
#include <QStack>
#include <QPair>
#include <QKeyEvent>   // 记得包含 QKeyEvent

class LineNumberArea;

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;  // <-- 加上这一行

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    int findMatchingBracket(const QString& text, int pos);
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;

    void highlightMatchingBrackets();
    bool isInCommentOrString(int pos) const;  // 判断当前位置是否在注释或字符串
};

// ----------------------------------------------------------------------
// LineNumberArea 类，用于绘制行号
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *e) override;  // 拦截鼠标事件，使行号不可编辑

private:
    CodeEditor *codeEditor;
};
