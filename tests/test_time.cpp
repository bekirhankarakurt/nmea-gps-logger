#include "nmea/time.hpp"
#include "test_framework.hpp"

using namespace nmea;

TEST(civil_day_conversions) {
    CHECK_EQ(days_from_civil(1970, 1, 1), 0);
    CHECK_EQ(days_from_civil(2000, 3, 1), 11017);
    CHECK_EQ(days_from_civil(1969, 12, 31), -1);
    for (std::int64_t d = -800000; d <= 800000; d += 997) {
        const Date c = civil_from_days(d);
        CHECK_EQ(days_from_civil(c.year, c.month, c.day), d);
    }
    const Date leap = civil_from_days(days_from_civil(2024, 2, 29));
    CHECK_EQ(leap.month, 2);
    CHECK_EQ(leap.day, 29);
}

TEST(unix_seconds_and_iso_format) {
    const Date d{2026, 5, 14};
    const UtcTime t{8, 30, 1.5};
    const double unix = to_unix_seconds(d, t);
    CHECK_NEAR(unix, 1778747401.5, 1e-6);
    CHECK_EQ(format_iso8601(unix), "2026-05-14T08:30:01.500Z");
    CHECK_EQ(format_iso8601(0.0), "1970-01-01T00:00:00Z");
    CHECK_EQ(format_iso8601(-1.0), "1969-12-31T23:59:59Z");
}

TEST(time_of_day_format) {
    CHECK_EQ(format_time_of_day(0.0), "00:00:00");
    CHECK_EQ(format_time_of_day(30601.0), "08:30:01");
    CHECK_EQ(format_time_of_day(86399.4), "23:59:59");
    CHECK_NEAR((UtcTime{23, 59, 59.5}).seconds_of_day(), 86399.5, 1e-9);
}

TEST(duration_format) {
    CHECK_EQ(format_duration(0.0), "0:00:00");
    CHECK_EQ(format_duration(318.0), "0:05:18");
    CHECK_EQ(format_duration(93790.4), "26:03:10");
    CHECK_EQ(format_duration(-5.0), "0:00:00");
}
