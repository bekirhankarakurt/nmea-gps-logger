#include "nmea/pipeline.hpp"
#include "test_framework.hpp"

#include <fstream>
#include <string>

using namespace nmea;

namespace {
const std::string kFixture = std::string(NMEA_FIXTURE_DIR) + "/metu_ride.nmea";

Pipeline replay(PipelineConfig cfg, const std::vector<Geofence>& fences = {}) {
    Pipeline p(cfg);
    for (const auto& f : fences) {
        p.add_fence(f);
    }
    std::ifstream in(kFixture);
    REQUIRE(in.good());
    std::string line;
    while (std::getline(in, line)) {
        (void)p.process_line(line);
    }
    (void)p.finish();
    return p;
}
}  // namespace

TEST(fixture_replay_counts) {
    const auto p = replay({});
    const auto& c = p.counters();
    CHECK_EQ(c.error_count(ParseError::ChecksumMismatch), 2U);
    CHECK_EQ(c.error_count(ParseError::UnsupportedType), 1U);
    CHECK_EQ(c.lines, 1370U);  // Blank line not counted.
    CHECK_EQ(c.accepted, c.lines - 3U);
    CHECK_EQ(c.fixes, 314U);   // 319 epochs minus a 5 s outage.
    CHECK_EQ(p.points().size(), 314U);
    CHECK(p.points().front().has_date);
}

TEST(fixture_statistics) {
    const auto p = replay({});
    const auto& s = p.stats();
    // Loop perimeter is 1.91 km; position jitter adds a little.
    CHECK_NEAR(s.distance_m, 1920.0, 20.0);
    CHECK_NEAR(s.duration_s, 318.0, 1e-6);
    CHECK_NEAR(s.max_speed_mps, 6.5, 0.7);
    CHECK_NEAR(s.avg_speed_mps, 6.2, 0.3);
    REQUIRE(s.mean_hdop.has_value());
    CHECK(*s.mean_hdop > 0.8 && *s.mean_hdop < 1.2);
    CHECK_NEAR(s.max_hdop.value_or(0), 3.4, 0.2);
    CHECK(s.elevation_gain_m > 10.0);
}

TEST(hdop_filter_drops_poor_fixes) {
    PipelineConfig cfg;
    cfg.max_hdop = 2.0;
    const auto p = replay(cfg);
    CHECK_EQ(p.counters().filtered, 5U);
    CHECK_EQ(p.counters().fixes, 309U);
    CHECK(p.stats().max_hdop.value_or(99) <= 2.0);
}

TEST(geofence_events_on_fixture) {
    // Circle around the north-east corner and polygon over the southern edge.
    const auto ne = Geofence::circle("ne-corner", {39.8920, 32.7860}, 60.0);
    const auto south = Geofence::polygon("south-road", {{39.8885, 32.7790}, {39.8885, 32.7870},
                                                        {39.8875, 32.7870}, {39.8875, 32.7790}});
    const auto p = replay({}, {ne, south});
    const auto& ev = p.events();
    REQUIRE(ev.size() == 4U);
    CHECK_EQ(ev[0].fence, "ne-corner");
    CHECK(ev[0].transition == FenceTransition::Enter);
    CHECK_EQ(ev[1].fence, "ne-corner");
    CHECK(ev[1].transition == FenceTransition::Exit);
    CHECK_EQ(ev[2].fence, "south-road");
    CHECK(ev[2].transition == FenceTransition::Enter);
    CHECK(ev[3].transition == FenceTransition::Exit);
    CHECK(ev[0].point.time_s < ev[1].point.time_s);
}

TEST(points_not_kept_when_disabled) {
    PipelineConfig cfg;
    cfg.keep_points = false;
    const auto p = replay(cfg);
    CHECK(p.points().empty());
    CHECK_EQ(p.stats().points, 314U);
}

TEST(process_line_outcome) {
    Pipeline p;
    auto o = p.process_line(make_sentence("GPGGA,083000.00,3953.5200,N,03246.8000,E,1,09,0.8,990.0,M,36.4,M,,"));
    CHECK(o.message.has_value());
    CHECK(o.points.empty());
    o = p.process_line("garbage");
    CHECK(o.error == ParseError::MissingStart);
    o = p.process_line("");
    CHECK(o.error == ParseError::Empty);
    o = p.process_line(make_sentence("GPGGA,083001.00,3953.5200,N,03246.8000,E,1,09,0.8,990.0,M,36.4,M,,"));
    CHECK_EQ(o.points.size(), 1U);
    CHECK_EQ(p.finish().points.size(), 1U);
    CHECK_EQ(p.counters().lines, 3U);
    CHECK_EQ(p.counters().error_count(ParseError::MissingStart), 1U);
}
