#include "CppHighlighter.h"
#include <QTextDocument>

CppHighlighter::CppHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // ----------------- 关键字 -----------------
    QStringList keywordPatterns = {
        "\\bif\\b", "\\belse\\b", "\\bfor\\b", "\\bwhile\\b",
        "\\breturn\\b", "\\bbreak\\b", "\\bcontinue\\b", "\\bswitch\\b",
        "\\bcase\\b", "\\bdefault\\b", "\\bdo\\b", "\\bconst\\b",
        "\\bstatic\\b", "\\bextern\\b", "\\bnamespace\\b", "\\bclass\\b",
        "\\bconstexpr\\b", "\\bnullptr\\b", "\\bauto\\b", "\\boverride\\b",
        "\\bfinal\\b", "\\bnoexcept\\b", "\\btemplate\\b"
    };
    keywordFormat.setForeground(Qt::blue);
    keywordFormat.setFontWeight(QFont::Bold);
    for (const QString &pattern : keywordPatterns)
        highlightingRules.append({QRegularExpression(pattern), keywordFormat});

    // ----------------- 类型 -----------------
    QStringList typePatterns = {"\\bint\\b", "\\bfloat\\b", "\\bdouble\\b",
                                "\\bchar\\b", "\\bbool\\b", "\\bvoid\\b", "\\bshort\\b",
                                "\\blong\\b", "\\bsigned\\b", "\\bunsigned\\b"};
    typeFormat.setForeground(Qt::darkMagenta);
    typeFormat.setFontWeight(QFont::Bold);
    for (const QString &pattern : typePatterns)
        highlightingRules.append({QRegularExpression(pattern), typeFormat});

    // ----------------- 函数名 -----------------
    functionFormat.setForeground(Qt::darkCyan);
    highlightingRules.append({QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()"), functionFormat});

    // ----------------- 字符串/字符 -----------------
    stringFormat.setForeground(Qt::red);

    // ----------------- 数字 -----------------
    numberFormat.setForeground(Qt::darkYellow);
    highlightingRules.append({QRegularExpression("\\b[0-9]+(\\.[0-9]+)?\\b"), numberFormat});
    highlightingRules.append({QRegularExpression("\\b0x[0-9A-Fa-f]+\\b"), numberFormat});

    // ----------------- 宏 / 预处理指令 -----------------
    preprocessorFormat.setForeground(Qt::darkRed);
    preprocessorFormat.setFontWeight(QFont::Bold);
    highlightingRules.append({QRegularExpression("^#\\s*\\w+"), preprocessorFormat});

    // ----------------- 单行注释 -----------------
    singleLineCommentFormat.setForeground(Qt::darkGreen);
    singleLineCommentFormat.setFontItalic(true);
    highlightingRules.append({QRegularExpression("//[^\n]*"), singleLineCommentFormat});

    // ----------------- 多行注释 -----------------
    multiLineCommentFormat.setForeground(Qt::darkGreen);
    multiLineCommentFormat.setFontItalic(true);
    commentStartExpression = QRegularExpression("/\\*");
    commentEndExpression = QRegularExpression("\\*/");
}

void CppHighlighter::highlightBlock(const QString &text)
{
    // ----------------- 字符串处理 -----------------
    QVector<bool> isInString(text.length(), false);
    QRegularExpression stringPattern("\"(\\\\.|[^\"])*\"|'(\\\\.|[^'])*'");
    QRegularExpressionMatchIterator stringIt = stringPattern.globalMatch(text);
    while (stringIt.hasNext()) {
        QRegularExpressionMatch match = stringIt.next();
        for (int i = match.capturedStart(); i < match.capturedStart() + match.capturedLength(); ++i)
            isInString[i] = true;
        setFormat(match.capturedStart(), match.capturedLength(), stringFormat);
    }

    // ----------------- 单行规则 -----------------
    for (const HighlightingRule &rule : highlightingRules) {
        if (&rule.format == &stringFormat) continue;
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            bool skip = false;
            for (int i = match.capturedStart(); i < match.capturedStart() + match.capturedLength(); ++i) {
                if (i < isInString.size() && isInString[i]) { skip = true; break; }
            }
            if (!skip)
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // ----------------- 多行注释 -----------------
    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(commentStartExpression);

    while (startIndex >= 0) {
        if (startIndex < isInString.size() && isInString[startIndex]) {
            startIndex = text.indexOf(commentStartExpression, startIndex + 1);
            continue;
        }

        QRegularExpressionMatch endMatch;
        int endIndex = text.indexOf(commentEndExpression, startIndex, &endMatch);
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
    }
}
