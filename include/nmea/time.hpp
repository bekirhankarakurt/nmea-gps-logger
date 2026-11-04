#pragma once

#include <cstdint>
#include <string>

namespace nmea {

/// UTC time of day as transmitted in NMEA sentences (hhmmss.ss).
struct UtcTime {
    int hour{};
    int minute{};
    double second{};

    [[nodiscard]] double seconds_of_day() const noexcept;
};

/// Calendar date (proleptic Gregorian, UTC).
struct Date {
    int year{};
    int month{};
    int day{};
};

inline constexpr double kSecondsPerDay = 86400.0;

/// Days since 1970-01-01 for a civil date (Howard Hinnant's algorithm).
[[nodiscard]] std::int64_t days_from_civil(int year, int month, int day) noexcept;

/// Inverse of days_from_civil.
[[nodiscard]] Date civil_from_days(std::int64_t days) noexcept;

/// Seconds since the Unix epoch for a date and time of day.
[[nodiscard]] double to_unix_seconds(const Date& date, const UtcTime& time) noexcept;

/// ISO 8601 UTC timestamp, e.g. "2026-11-04T09:15:02Z" or "...02.500Z" when fractional.
[[nodiscard]] std::string format_iso8601(double unix_seconds);

/// "hh:mm:ss" for a seconds-of-day value (wrapped into [0, 86400)).
[[nodiscard]] std::string format_time_of_day(double seconds_of_day);

/// Elapsed time as "h:mm:ss" with unbounded hours, e.g. "26:03:10".
[[nodiscard]] std::string format_duration(double seconds);

}  // namespace nmea
