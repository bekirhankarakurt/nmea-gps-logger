#include "nmea/replay.hpp"
#include "test_framework.hpp"

#include <stdexcept>

using namespace nmea;
using namespace std::chrono;

namespace {
double ms(ReplayPacer::Clock::duration d) { return static_cast<double>(duration_cast<milliseconds>(d).count()); }
}  // namespace

TEST(paces_at_real_time) {
    ReplayPacer p(1.0);
    const auto t0 = ReplayPacer::Clock::time_point{} + hours(1);
    CHECK_NEAR(ms(p.delay_for(100.0, t0)), 0.0, 0.0);
    CHECK_NEAR(ms(p.delay_for(101.0, t0)), 1000.0, 1.0);
    CHECK_NEAR(ms(p.delay_for(101.0, t0 + milliseconds(400))), 600.0, 1.0);
    CHECK_NEAR(ms(p.delay_for(102.0, t0 + seconds(5))), 0.0, 0.0);  // Behind schedule: no wait.
}

TEST(speed_factor_scales_delays) {
    ReplayPacer p(4.0);
    const auto t0 = ReplayPacer::Clock::time_point{} + hours(1);
    (void)p.delay_for(0.0, t0);
    CHECK_NEAR(ms(p.delay_for(2.0, t0)), 500.0, 1.0);
}

TEST(midnight_wrap_and_reanchor) {
    ReplayPacer p(1.0);
    const auto t0 = ReplayPacer::Clock::time_point{} + hours(1);
    (void)p.delay_for(86399.0, t0);
    CHECK_NEAR(ms(p.delay_for(1.0, t0)), 2000.0, 1.0);
    // A backwards jump (second log appended) restarts pacing.
    CHECK_NEAR(ms(p.delay_for(82800.0, t0 + seconds(10))), 0.0, 0.0);
    CHECK_NEAR(ms(p.delay_for(82801.0, t0 + seconds(10))), 1000.0, 1.0);
}

TEST(rejects_non_positive_speed) {
    CHECK_THROWS(ReplayPacer(0.0), std::invalid_argument);
    CHECK_THROWS(ReplayPacer(-2.0), std::invalid_argument);
}
