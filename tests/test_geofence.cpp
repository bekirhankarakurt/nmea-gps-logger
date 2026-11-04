#include "nmea/geofence.hpp"
#include "test_framework.hpp"

#include <stdexcept>

using namespace nmea;

namespace {
TrackPoint at(GeoPoint p) {
    TrackPoint t;
    t.position = p;
    t.quality = FixQuality::Gps;
    return t;
}
const GeoPoint kCenter{39.8900, 32.7830};
}  // namespace

TEST(circle_contains) {
    const auto f = Geofence::circle("lab", kCenter, 100.0);
    CHECK(f.contains(kCenter));
    CHECK(f.contains(destination(kCenter, 33.0, 99.0)));
    CHECK(!f.contains(destination(kCenter, 33.0, 101.0)));
    CHECK(f.shape() == FenceShape::Circle);
    CHECK_THROWS(Geofence::circle("x", kCenter, 0.0), std::invalid_argument);
    CHECK_THROWS(Geofence::circle("x", kCenter, -5.0), std::invalid_argument);
}

TEST(polygon_contains_convex_and_concave) {
    const auto square = Geofence::polygon("sq", {{39.0, 32.0}, {39.0, 33.0}, {40.0, 33.0}, {40.0, 32.0}});
    CHECK(square.contains({39.5, 32.5}));
    CHECK(!square.contains({40.5, 32.5}));
    CHECK(!square.contains({39.5, 31.9}));

    // U shape: the notch between the arms is outside.
    const auto u = Geofence::polygon("u", {{0.0, 0.0}, {0.0, 3.0}, {3.0, 3.0}, {3.0, 2.0},
                                           {1.0, 2.0}, {1.0, 1.0}, {3.0, 1.0}, {3.0, 0.0}});
    CHECK(u.contains({0.5, 1.5}));
    CHECK(u.contains({2.0, 0.5}));
    CHECK(u.contains({2.0, 2.5}));
    CHECK(!u.contains({2.0, 1.5}));
}

TEST(polygon_across_antimeridian) {
    const auto f = Geofence::polygon("fiji", {{-17.0, 179.0}, {-17.0, -179.0}, {-19.0, -179.0}, {-19.0, 179.0}});
    CHECK(f.contains({-18.0, 179.9}));
    CHECK(f.contains({-18.0, -179.9}));
    CHECK(!f.contains({-18.0, 178.0}));
}

TEST(polygon_validation_and_outline) {
    CHECK_THROWS(Geofence::polygon("x", {{0, 0}, {1, 1}}), std::invalid_argument);
    CHECK_THROWS(Geofence::polygon("x", {{0, 0}, {1, 1}, {0, 0}}), std::invalid_argument);
    const auto closed = Geofence::polygon("t", {{0, 0}, {0, 1}, {1, 0}, {0, 0}});
    CHECK_EQ(closed.vertices().size(), 3U);
    const auto ring = closed.outline();
    CHECK_EQ(ring.size(), 4U);
    CHECK_NEAR(ring.front().lat_deg, ring.back().lat_deg, 0.0);

    const auto c = Geofence::circle("c", kCenter, 250.0);
    const auto circle_ring = c.outline(16);
    CHECK_EQ(circle_ring.size(), 17U);
    for (const auto& p : circle_ring) {
        CHECK_NEAR(haversine_m(kCenter, p), 250.0, 1e-6);
    }
    CHECK_EQ(c.outline(2).size(), 9U);  // Clamped to at least 8 segments.
}

TEST(monitor_enter_exit_sequence) {
    GeofenceMonitor m;
    m.add(Geofence::circle("home", kCenter, 50.0));
    CHECK(!m.is_inside(0).has_value());
    CHECK(m.update(at(destination(kCenter, 0.0, 200.0))).empty());  // First fix outside: silent.
    CHECK(m.is_inside(0) == false);
    auto ev = m.update(at(kCenter));
    REQUIRE(ev.size() == 1U);
    CHECK_EQ(ev[0].fence, "home");
    CHECK(ev[0].transition == FenceTransition::Enter);
    CHECK(m.update(at(kCenter)).empty());
    ev = m.update(at(destination(kCenter, 90.0, 80.0)));
    REQUIRE(ev.size() == 1U);
    CHECK(ev[0].transition == FenceTransition::Exit);
    CHECK_EQ(to_string(FenceTransition::Exit), "EXIT");
}

TEST(monitor_first_fix_inside_enters) {
    GeofenceMonitor m;
    m.add(Geofence::circle("a", kCenter, 50.0));
    m.add(Geofence::circle("b", kCenter, 500.0));
    m.add(Geofence::circle("c", destination(kCenter, 0.0, 5000.0), 50.0));
    const auto ev = m.update(at(destination(kCenter, 0.0, 100.0)));
    REQUIRE(ev.size() == 1U);
    CHECK_EQ(ev[0].fence, "b");
}

TEST(monitor_confirmation_suppresses_jitter) {
    GeofenceMonitor m(3);
    m.add(Geofence::circle("gate", kCenter, 50.0));
    const auto in = at(kCenter);
    const auto out = at(destination(kCenter, 0.0, 60.0));
    CHECK(m.update(out).empty());
    CHECK(m.update(in).empty());
    CHECK(m.update(out).empty());  // Jitter resets the counter.
    CHECK(m.update(in).empty());
    CHECK(m.update(in).empty());
    const auto ev = m.update(in);
    REQUIRE(ev.size() == 1U);
    CHECK(ev[0].transition == FenceTransition::Enter);
    CHECK_THROWS(GeofenceMonitor(0), std::invalid_argument);
}
