#include "nmea/geo.hpp"
#include "test_framework.hpp"

using namespace nmea;

namespace {
constexpr double kPi = 3.14159265358979323846;

double dms(int d, int m, double s) {
    const double v = std::abs(d) + m / 60.0 + s / 3600.0;
    return d < 0 ? -v : v;
}

double angle_diff(double a, double b) {
    const double d = std::fmod(std::fabs(a - b), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}
}  // namespace

TEST(haversine_one_degree_meridian) {
    CHECK_NEAR(haversine_m({0.0, 0.0}, {1.0, 0.0}), kEarthRadiusM * kPi / 180.0, 1e-6);
    CHECK_NEAR(haversine_m({10.0, 20.0}, {10.0, 20.0}), 0.0, 1e-9);
}

TEST(haversine_is_symmetric) {
    const GeoPoint a{39.8920, 32.7800};
    const GeoPoint b{41.0082, 28.9784};
    CHECK_NEAR(haversine_m(a, b), haversine_m(b, a), 1e-6);
    CHECK_NEAR(haversine_m(a, b), 350000.0, 10000.0);  // Ankara-Istanbul ~350 km.
}

TEST(vincenty_reference_flinders_buninyong) {
    // Vincenty (1975) / Geoscience Australia worked example.
    const GeoPoint flinders{dms(-37, 57, 3.72030), dms(144, 25, 29.52440)};
    const GeoPoint buninyong{dms(-37, 39, 10.15610), dms(143, 55, 35.38390)};
    const auto g = vincenty_inverse(flinders, buninyong);
    REQUIRE(g.has_value());
    CHECK_NEAR(g->distance_m, 54972.271, 1e-3);
    CHECK_NEAR(g->initial_bearing_deg, dms(306, 52, 5.37), 1e-5);
    CHECK_NEAR(g->final_bearing_deg, dms(307, 10, 25.07), 1e-5);
}

TEST(vincenty_equator_and_coincident) {
    const auto g = vincenty_inverse({0.0, 0.0}, {0.0, 1.0});
    REQUIRE(g.has_value());
    CHECK_NEAR(g->distance_m, kWgs84A * kPi / 180.0, 1e-6);
    CHECK_NEAR(g->initial_bearing_deg, 90.0, 1e-9);
    const auto z = vincenty_inverse({39.9, 32.8}, {39.9, 32.8});
    REQUIRE(z.has_value());
    CHECK_NEAR(z->distance_m, 0.0, 1e-12);
}

TEST(vincenty_nearly_antipodal_falls_back) {
    const GeoPoint a{0.0, 0.0};
    const GeoPoint b{0.5, 179.7};
    CHECK(!vincenty_inverse(a, b).has_value());
    CHECK_NEAR(distance_m(a, b, DistanceMethod::Vincenty), haversine_m(a, b), 1e-6);
}

TEST(haversine_and_vincenty_agree_locally) {
    const GeoPoint a{39.8920, 32.7800};
    const GeoPoint b{39.8880, 32.7860};
    const double h = distance_m(a, b, DistanceMethod::Haversine);
    const double v = distance_m(a, b, DistanceMethod::Vincenty);
    CHECK_NEAR(h, v, v * 0.005);
}

TEST(bearing_and_destination_roundtrip) {
    const GeoPoint origin{39.8920, 32.7800};
    CHECK_NEAR(initial_bearing_deg(origin, {40.8920, 32.7800}), 0.0, 1e-9);
    CHECK_NEAR(initial_bearing_deg(origin, {38.8920, 32.7800}), 180.0, 1e-9);
    for (const double brg : {0.0, 45.0, 135.0, 270.0, 359.0}) {
        const GeoPoint p = destination(origin, brg, 2500.0);
        CHECK_NEAR(haversine_m(origin, p), 2500.0, 1e-6);
        CHECK_NEAR(angle_diff(initial_bearing_deg(origin, p), brg), 0.0, 1e-6);
    }
    const GeoPoint wrapped = destination({0.0, 179.9}, 90.0, 50000.0);
    CHECK(wrapped.lon_deg < -179.0);
}

TEST(angle_helpers) {
    CHECK_NEAR(normalize_bearing(-90.0), 270.0, 1e-12);
    CHECK_NEAR(normalize_bearing(720.0), 0.0, 1e-12);
    CHECK_NEAR(rad_to_deg(deg_to_rad(123.456)), 123.456, 1e-12);
}
