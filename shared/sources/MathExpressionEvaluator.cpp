#include "MathExpressionEvaluator.h"

#include <cmath>

namespace
{
bool isValidNumber(double value)
{
    return std::isfinite(value);
}
}

MathExpressionEvaluator::MathExpressionEvaluator(const QString& expression)
    : m_text(expression),
      m_pos(0),
      m_ok(true)
{
}

bool MathExpressionEvaluator::evaluate(const QString& expression,
                                       double& result,
                                       QString& errorMessage)
{
    MathExpressionEvaluator parser(expression.trimmed());

    if (parser.m_text.isEmpty())
    {
        errorMessage = "Expression is empty.";
        return false;
    }

    result = parser.parseExpression();
    parser.skipSpaces();

    if (!parser.m_ok)
    {
        errorMessage = parser.m_error;
        return false;
    }

    if (parser.m_pos != parser.m_text.length())
    {
        errorMessage = QString("Unexpected character: '%1'")
                           .arg(parser.m_text.at(parser.m_pos));
        return false;
    }

    if (!isValidNumber(result))
    {
        errorMessage = "Expression result is not a valid finite number.";
        return false;
    }

    return true;
}

void MathExpressionEvaluator::setError(const QString& message)
{
    if (m_ok)
    {
        m_ok = false;
        m_error = message;
    }
}

void MathExpressionEvaluator::skipSpaces()
{
    while (m_pos < m_text.length() && m_text.at(m_pos).isSpace())
    {
        ++m_pos;
    }
}

QChar MathExpressionEvaluator::peek() const
{
    if (m_pos >= m_text.length())
    {
        return QChar();
    }

    return m_text.at(m_pos);
}

bool MathExpressionEvaluator::match(QChar ch)
{
    skipSpaces();

    if (m_pos < m_text.length() && m_text.at(m_pos) == ch)
    {
        ++m_pos;
        return true;
    }

    return false;
}

double MathExpressionEvaluator::parseExpression()
{
    double left = parseTerm();

    while (m_ok)
    {
        if (match(QChar('+')))
        {
            left += parseTerm();
        }
        else if (match(QChar('-')))
        {
            left -= parseTerm();
        }
        else
        {
            break;
        }
    }

    return left;
}

double MathExpressionEvaluator::parseTerm()
{
    double left = parseUnary();

    while (m_ok)
    {
        if (match(QChar('*')))
        {
            left *= parseUnary();
        }
        else if (match(QChar('/')))
        {
            const double right = parseUnary();

            if (right == 0.0)
            {
                setError("Division by zero.");
                return 0.0;
            }

            left /= right;
        }
        else
        {
            break;
        }
    }

    return left;
}

double MathExpressionEvaluator::parseUnary()
{
    skipSpaces();

    if (match(QChar('+')))
    {
        return parseUnary();
    }

    if (match(QChar('-')))
    {
        return -parseUnary();
    }

    return parsePower();
}

double MathExpressionEvaluator::parsePower()
{
    double left = parsePrimary();

    if (m_ok && match(QChar('^')))
    {
        const double right = parseUnary();
        left = std::pow(left, right);

        if (!isValidNumber(left))
        {
            setError("Invalid power operation.");
            return 0.0;
        }
    }

    return left;
}

double MathExpressionEvaluator::parsePrimary()
{
    skipSpaces();

    if (match(QChar('(')))
    {
        const double value = parseExpression();

        if (!match(QChar(')')))
        {
            setError("Missing closing bracket.");
            return 0.0;
        }

        return value;
    }

    const QChar ch = peek();

    if (ch.isLetter() || ch == QChar('_'))
    {
        const QString name = parseIdentifier();

        if (name == "pi")
        {
            return 3.14159265358979323846;
        }

        if (name == "e")
        {
            return 2.71828182845904523536;
        }

        setError(QString("Unknown constant: %1").arg(name));
        return 0.0;
    }

    return parseNumber();
}

QString MathExpressionEvaluator::parseIdentifier()
{
    skipSpaces();

    const int start = m_pos;

    if (m_pos >= m_text.length() ||
        !(m_text.at(m_pos).isLetter() || m_text.at(m_pos) == QChar('_')))
    {
        setError("Expected constant.");
        return QString();
    }

    ++m_pos;

    while (m_pos < m_text.length())
    {
        const QChar ch = m_text.at(m_pos);

        if (ch.isLetterOrNumber() || ch == QChar('_'))
        {
            ++m_pos;
        }
        else
        {
            break;
        }
    }

    return m_text.mid(start, m_pos - start).toLower();
}

double MathExpressionEvaluator::parseNumber()
{
    skipSpaces();

    const int start = m_pos;
    bool hasDigit = false;

    while (m_pos < m_text.length() && m_text.at(m_pos).isDigit())
    {
        hasDigit = true;
        ++m_pos;
    }

    if (m_pos < m_text.length() && m_text.at(m_pos) == QChar('.'))
    {
        ++m_pos;

        while (m_pos < m_text.length() && m_text.at(m_pos).isDigit())
        {
            hasDigit = true;
            ++m_pos;
        }
    }

    if (!hasDigit)
    {
        setError("Expected number.");
        return 0.0;
    }

    if (m_pos < m_text.length() &&
        (m_text.at(m_pos) == QChar('e') || m_text.at(m_pos) == QChar('E')))
    {
        const int exponentPosition = m_pos;
        ++m_pos;

        if (m_pos < m_text.length() &&
            (m_text.at(m_pos) == QChar('+') || m_text.at(m_pos) == QChar('-')))
        {
            ++m_pos;
        }

        const int exponentStart = m_pos;

        while (m_pos < m_text.length() && m_text.at(m_pos).isDigit())
        {
            ++m_pos;
        }

        if (exponentStart == m_pos)
        {
            m_pos = exponentPosition;
        }
    }

    bool ok = false;
    const double value = m_text.mid(start, m_pos - start).toDouble(&ok);

    if (!ok)
    {
        setError("Invalid number.");
        return 0.0;
    }

    return value;
}
