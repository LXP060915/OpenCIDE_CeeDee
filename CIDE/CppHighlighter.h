#ifndef CPPHIGHLIGHTER_H
#define CPPHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

struct HighlightingRule
{
    QRegularExpression pattern;
    QTextCharFormat format;
};

class CppHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit CppHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat typeFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat preprocessorFormat;

    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;
};

#endif // CPPHIGHLIGHTER_H
