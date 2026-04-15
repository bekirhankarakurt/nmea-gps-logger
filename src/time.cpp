#include "nmea/time.hpp"

#include <cmath>
#include <cstdio>
#include <array>

namespace nmea {

double UtcTime::seconds_of_day() const noexcept {
    return static_cast<double>(hour) * 3600.0 + static_cast<double>(minute) * 60.0 + second;
}

std::int64_t days_from_civil(int year, int month, int day) noexcept {
    const std::int64_t y = static_cast<std::int64_t>(year) - (month <= 2 ? 1 : 0);
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const std::int64_t yoe = y - era * 400;
    const std::int64_t m = month;
    const std::int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

Date civil_from_days(std::int64_t days) noexcept {
    const std::int64_t z = days + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    const std::int64_t d = doy - (153 * mp + 2) / 5 + 1;
    const std::int64_t m = mp < 10 ? mp + 3 : mp - 9;
    const std::int64_t y = yoe + era * 400 + (m <= 2 ? 1 : 0);
    return Date{static_cast<int>(y), static_cast<int>(m), static_cast<int>(d)};
}

double to_unix_seconds(const Date& date, const UtcTime& time) noexcept {
    return static_cast<double>(days_from_civil(date.year, date.month, date.day)) * kSecondsPerDay +
           time.seconds_of_day();
}

namespace {

struct SplitTime {
    std::int64_t days;
    int hour;
    int minute;
    int second;
    int millis;
};

[[nodiscard]] SplitTime split(double unix_seconds) noexcept {
    const auto total_ms = static_cast<std::int64_t>(std::llround(unix_seconds * 1000.0));
    std::int64_t days = total_ms / 86'400'000;
    std::int64_t rem = total_ms % 86'400'000;
    if (rem < 0) {
        rem += 86'400'000;
        --days;
    }
    return SplitTime{days, static_cast<int>(rem / 3'600'000), static_cast<int>(rem / 60'000 % 60),
                     static_cast<int>(rem / 1000 % 60), static_cast<int>(rem % 1000)};
}

}  // namespace

std::string format_iso8601(double unix_seconds) {
    const SplitTime t = split(unix_seconds);
    const Date d = civil_from_days(t.days);
    std::array<char, 40> buf{};
    if (t.millis != 0) {
        static_cast<void>(std::snprintf(buf.data(), buf.size(), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", d.year, d.month,
                      d.day, t.hour, t.minute, t.second, t.millis));
    } else {
        static_cast<void>(std::snprintf(buf.data(), buf.size(), "%04d-%02d-%02dT%02d:%02d:%02dZ", d.year, d.month,
                      d.day, t.hour, t.minute, t.second));
    }
    return std::string(buf.data());
}

std::string format_time_of_day(double seconds_of_day) {
    const SplitTime t = split(seconds_of_day);
    std::array<char, 16> buf{};
    static_cast<void>(std::snprintf(buf.data(), buf.size(), "%02d:%02d:%02d", t.hour, t.minute, t.second));
    return std::string(buf.data());
}

std::string format_duration(double seconds) {
    const auto total = static_cast<std::int64_t>(std::llround(seconds < 0.0 ? 0.0 : seconds));
    std::array<char, 32> buf{};
    static_cast<void>(std::snprintf(buf.data(), buf.size(), "%lld:%02lld:%02lld", static_cast<long long>(total / 3600),
                  static_cast<long long>(total / 60 % 60), static_cast<long long>(total % 60)));
    return std::string(buf.data());
}

}  // namespace nmea
