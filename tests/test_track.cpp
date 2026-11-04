#include "nmea/parser.hpp"
#include "nmea/track.hpp"
#include "test_framework.hpp"

#include <string>
#include <vector>

using namespace nmea;

namespace {

std::vector<TrackPoint> run(FixAssembler& a, const std::vector<std::string>& bodies, bool flush = true) {
    std::vector<TrackPoint> out;
    for (const auto& b : bodies) {
        const auto r = parse(make_sentence(b));
        REQUIRE(r.ok());
        if (auto p = a.feed(r.value())) {
            out.push_back(*p);
        }
    }
    if (flush) {
        if (auto p = a.flush()) {
            out.push_back(*p);
        }
    }
    return out;
}

}  // namespace

TEST(merges_one_epoch) {
    FixAssembler a;
    const auto pts = run(a, {
        "GPGGA,083000.00,3953.5200,N,03246.8000,E,1,09,0.8,990.0,M,36.4,M,,",
        "GPGSA,A,3,02,05,07,,,,,,,,,,1.4,0.9,1.2",
        "GPRMC,083000.00,A,3953.5200,N,03246.8000,E,10.0,90.0,140526,,,A",
        "GPVTG,90.0,T,,M,10.0,N,18.5,K,A",
        "GPGSV,1,1,01,02,41,063,44",
    });
    REQUIRE(pts.size() == 1U);
    const TrackPoint& p = pts[0];
    CHECK(p.has_date);
    CHECK_NEAR(p.time_s, to_unix_seconds({2026, 5, 14}, {8, 30, 0.0}), 1e-6);
    CHECK_NEAR(p.position.lat_deg, 39.0 + 53.52 / 60.0, 1e-9);
    CHECK_NEAR(p.altitude_m.value_or(0), 990.0, 1e-9);
    CHECK_NEAR(p.speed_mps.value_or(0), 10.0 * kKnotsToMps, 1e-9);  // RMC wins over VTG.
    CHECK_NEAR(p.course_deg.value_or(0), 90.0, 1e-9);
    CHECK_NEAR(p.hdop.value_or(0), 0.8, 1e-9);  // GGA HDOP preferred to GSA.
    CHECK_EQ(p.satellites.value_or(0), 9);
    CHECK(p.quality == FixQuality::Gps);
}

TEST(epoch_closes_on_new_timestamp_in_any_order) {
    FixAssembler a;
    const auto pts = run(a, {
        "GPRMC,083000.00,A,3953.5200,N,03246.8000,E,1.0,90.0,140526,,,A",
        "GPGGA,083000.00,3953.5200,N,03246.8000,E,1,09,0.8,990.0,M,36.4,M,,",
        "GPRMC,083001.00,A,3953.5300,N,03246.8100,E,1.0,90.0,140526,,,A",
        "GPGGA,083001.00,3953.5300,N,03246.8100,E,1,09,0.8,991.0,M,36.4,M,,",
    }, false);
    REQUIRE(pts.size() == 1U);
    CHECK_NEAR(pts[0].altitude_m.value_or(0), 990.0, 1e-9);
    const auto last = a.flush();
    REQUIRE(last.has_value());
    CHECK_NEAR(last->altitude_m.value_or(0), 991.0, 1e-9);
    CHECK(!a.flush().has_value());
}

TEST(invalid_epochs_are_dropped) {
    FixAssembler a;
    const auto pts = run(a, {
        "GPGGA,083000.00,,,,,0,00,99.9,,M,,M,,",
        "GPRMC,083000.00,V,,,,,,,140526,,,N",
        "GPGGA,083001.00,3953.5200,N,03246.8000,E,1,05,2.1,990.0,M,36.4,M,,",
    });
    REQUIRE(pts.size() == 1U);
    CHECK_NEAR(pts[0].hdop.value_or(0), 2.1, 1e-9);
}

TEST(rmc_only_stream_and_vtg_speed) {
    FixAssembler a;
    const auto pts = run(a, {
        "GNRMC,120000.00,A,3953.5200,N,03246.8000,E,,,010126,,,D",
        "GNVTG,45.0,T,,M,,N,36.0,K,D",
    });
    REQUIRE(pts.size() == 1U);
    CHECK(pts[0].quality == FixQuality::Dgps);
    CHECK_NEAR(pts[0].speed_mps.value_or(0), 10.0, 1e-9);
    CHECK_NEAR(pts[0].course_deg.value_or(0), 45.0, 1e-9);
    CHECK(!pts[0].altitude_m);
}

TEST(gga_only_stream_has_no_date) {
    FixAssembler a;
    const auto pts = run(a, {
        "GPGGA,235959.00,3953.5200,N,03246.8000,E,1,09,0.8,990.0,M,36.4,M,,",
        "GPGSA,A,3,02,,,,,,,,,,,,1.4,1.1,1.2",
    });
    REQUIRE(pts.size() == 1U);
    CHECK(!pts[0].has_date);
    CHECK_NEAR(pts[0].time_s, 86399.0, 1e-9);
}

TEST(midnight_rollover_after_last_rmc) {
    FixAssembler a;
    const auto pts = run(a, {
        "GPRMC,235959.00,A,3953.5200,N,03246.8000,E,1.0,90.0,311226,,,A",
        "GPGGA,000000.00,3953.5200,N,03246.8000,E,1,09,0.8,990.0,M,36.4,M,,",
        "GPGGA,000001.00,3953.5200,N,03246.8000,E,1,09,0.8,990.0,M,36.4,M,,",
    });
    REQUIRE(pts.size() == 3U);
    const double day = to_unix_seconds({2026, 12, 31}, {0, 0, 0.0});
    CHECK_NEAR(pts[0].time_s, day + 86399.0, 1e-6);
    CHECK_NEAR(pts[1].time_s, day + 86400.0, 1e-6);
    CHECK_NEAR(pts[2].time_s, day + 86401.0, 1e-6);
}

TEST(gsa_hdop_used_when_gga_lacks_it) {
    FixAssembler a;
    const auto pts = run(a, {
        "GPRMC,083000.00,A,3953.5200,N,03246.8000,E,1.0,90.0,140526,,,A",
        "GPGSA,A,3,02,,,,,,,,,,,,1.4,1.3,1.2",
    });
    REQUIRE(pts.size() == 1U);
    CHECK_NEAR(pts[0].hdop.value_or(0), 1.3, 1e-9);
}
