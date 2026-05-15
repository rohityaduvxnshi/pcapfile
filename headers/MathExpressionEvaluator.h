#ifndef MATHEXPRESSIONEVALUATOR_H
#define MATHEXPRESSIONEVALUATOR_H

#include <QString>

class MathExpressionEvaluator
{
public:
    static bool evaluate(const QString& expression, double& result, QString& errorMessage);

private:
    explicit MathExpressionEvaluator(const QString& expression);

    double parseExpression();
    double parseTerm();
    double parseUnary();
    double parsePower();
    double parsePrimary();
    double parseNumber();
    QString parseIdentifier();

    void skipSpaces();
    bool match(QChar ch);
    QChar peek() const;
    void setError(const QString& message);

    QString m_text;
    int m_pos;
    bool m_ok;
    QString m_error;
};

#endif // MATHEXPRESSIONEVALUATOR_H
