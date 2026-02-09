/**
 * @file TimeFormatter.h
 * @brief Utility functions for time formatting.
 * 
 * @note Part of the Utils layer - shared utilities.
 */

#ifndef TIMEFORMATTER_H
#define TIMEFORMATTER_H

#include <QString>

/**
 * @namespace TimeFormatter
 * @brief Provides utility functions for formatting time values.
 */
namespace TimeFormatter {

/**
 * @brief Formats milliseconds to MM:SS or HH:MM:SS format.
 * @param milliseconds Time value in milliseconds.
 * @return Formatted string.
 */
QString format(qint64 milliseconds);

/**
 * @brief Formats milliseconds to MM:SS.mmm format.
 * @param milliseconds Time value in milliseconds.
 * @return Formatted string with milliseconds.
 */
QString formatWithMillis(qint64 milliseconds);

} // namespace TimeFormatter

#endif // TIMEFORMATTER_H
