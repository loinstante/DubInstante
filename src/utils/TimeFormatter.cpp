/**
 * @file TimeFormatter.cpp
 * @brief Implementation of TimeFormatter utility functions.
 */

#include "TimeFormatter.h"

#include <QTime>

namespace TimeFormatter {

QString format(qint64 milliseconds)
{
    QTime time(0, 0);
    time = time.addMSecs(static_cast<int>(milliseconds));
    
    if (milliseconds >= 3600000) {
        return time.toString("hh:mm:ss");
    }
    return time.toString("mm:ss");
}

QString formatWithMillis(qint64 milliseconds)
{
    int mm = (milliseconds / 60000) % 60;
    int ss = (milliseconds / 1000) % 60;
    int ms = milliseconds % 1000;
    
    return QString("%1:%2.%3")
        .arg(mm, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

} // namespace TimeFormatter
