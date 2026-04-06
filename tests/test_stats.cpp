#include "nmea/stats.hpp"
#include "test_framework.hpp"

using namespace nmea;

namespace {
TrackPoint pt(double t, GeoPoint pos, std::optional<double> speed = std::nullopt,
              std::optional<double> hdop = std::nullopt, std::optional<double> alt = std::nullopt) {
    TrackPoint p;
    p.time_s = t;
    p.has_date = true;
    p.position = pos;
    p.speed_mps = speed;
    p.hdop = hdop;
    p.altitude_m = alt;
    p.quality = FixQuality::Gps;
    return p;
}
}  // namespace

TEST(empty_stats) {
    const StatsAccumulator acc;
    CHECK_EQ(acc.stats().points, 0U);
    CHECK_NEAR(acc.stats().distance_m, 0.0, 0.0);
    CHECK(!acc.stats().mean_hdop);
    CHECK(!acc.stats().bbox_min);
}

TEST(straight_line_track) {
    StatsAccumulator acc;
    const GeoPoint start{39.89, 32.78};
    for (int i = 0; i <= 10; ++i) {
        const GeoPoint p = destination(start, 90.0, 10.0 * i);
        acc.add(pt(1000.0 + i, p, std::nullopt, 0.8 + 0.1 * (i % 2), 900.0 + i));
    }
    const auto& s = acc.stats();
    CHECK_EQ(s.points, 11U);
    CHECK_NEAR(s.distance_m, 100.0, 1e-3);
    CHECK_NEAR(s.duration_s, 10.0, 1e-9);
    CHECK_NEAR(s.avg_speed_mps, 10.0, 1e-3);
    CHECK_NEAR(s.max_speed_mps, 10.0, 1e-3);  // Derived when receivers omit speed.
    CHECK_NEAR(s.moving_time_s, 10.0, 1e-9);
    CHECK_NEAR(s.mean_hdop.value_or(0), (6 * 0.8 + 5 * 0.9) / 11.0, 1e-9);
    CHECK_NEAR(s.max_hdop.value_or(0), 0.9, 1e-9);
    CHECK_NEAR(s.elevation_gain_m, 10.0, 1e-9);
    CHECK_NEAR(s.min_altitude_m.value_or(0), 900.0, 1e-9);
    CHECK_NEAR(s.max_altitude_m.value_or(0), 910.0, 1e-9);
    CHECK_NEAR(s.bbox_min->lon_deg, 32.78, 1e-12);
    CHECK(s.bbox_max->lon_deg > 32.78);
}

TEST(stationary_jitter_is_not_moving_time) {
    StatsAccumulator acc;
    acc.add(pt(0.0, {39.89, 32.78}, 0.1));
    acc.add(pt(1.0, {39.89, 32.78}, 0.2));
    acc.add(pt(2.0, {39.89001, 32.78}, 5.0));
    const auto& s = acc.stats();
    CHECK_NEAR(s.moving_time_s, 1.0, 1e-9);
    CHECK_NEAR(s.max_speed_mps, 5.0, 1e-9);
    CHECK_NEAR(s.avg_moving_speed_mps, s.distance_m, 1e-9);
}

TEST(vincenty_method_is_used) {
    StatsAccumulator h(DistanceMethod::Haversine);
    StatsAccumulator v(DistanceMethod::Vincenty);
    for (auto* acc : {&h, &v}) {
        acc->add(pt(0.0, {0.0, 0.0}));
        acc->add(pt(60.0, {0.0, 1.0}));
    }
    CHECK_NEAR(v.stats().distance_m, 111319.49, 0.01);
    CHECK(std::fabs(h.stats().distance_m - v.stats().distance_m) > 100.0);
}
