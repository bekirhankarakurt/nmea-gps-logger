#include "nmea/export.hpp"
#include "nmea/time.hpp"
#include "test_framework.hpp"

#include <sstream>

using namespace nmea;

namespace {
std::vector<TrackPoint> sample() {
    std::vector<TrackPoint> pts;
    for (int i = 0; i < 3; ++i) {
        TrackPoint p;
        p.time_s = to_unix_seconds({2026, 5, 14}, {8, 30, static_cast<double>(i)});
        p.has_date = true;
        p.position = {39.892 + 0.0001 * i, 32.78};
        p.altitude_m = 990.0 + i;
        p.hdop = 0.8;
        p.satellites = 9;
        p.quality = i == 1 ? FixQuality::Dgps : FixQuality::Gps;
        pts.push_back(p);
    }
    return pts;
}

std::size_t count(const std::string& hay, const std::string& needle) {
    std::size_t n = 0;
    for (auto pos = hay.find(needle); pos != std::string::npos; pos = hay.find(needle, pos + 1)) {
        ++n;
    }
    return n;
}
}  // namespace

TEST(escaping) {
    CHECK_EQ(xml_escape("a<b>&\"c'"), "a&lt;b&gt;&amp;&quot;c&apos;");
    CHECK_EQ(json_escape("q\"\\\n\t\x01"), "q\\\"\\\\\\n\\t\\u0001");
}

TEST(gpx_document) {
    std::ostringstream os;
    write_gpx(os, sample(), "Ride <1>");
    const std::string g = os.str();
    CHECK(g.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 0) == 0);
    CHECK(g.find("xmlns=\"http://www.topografix.com/GPX/1/1\"") != std::string::npos);
    CHECK(g.find("<name>Ride &lt;1&gt;</name>") != std::string::npos);
    CHECK_EQ(count(g, "<trkpt "), 3U);
    CHECK(g.find("<trkpt lat=\"39.8920000\" lon=\"32.7800000\"><ele>990.0</ele>"
                 "<time>2026-05-14T08:30:00Z</time><sat>9</sat><hdop>0.8</hdop></trkpt>") != std::string::npos);
    CHECK_EQ(count(g, "<fix>dgps</fix>"), 1U);
    CHECK(g.find("</gpx>") != std::string::npos);
}

TEST(gpx_without_dates_omits_time) {
    auto pts = sample();
    pts[0].has_date = false;
    std::ostringstream os;
    write_gpx(os, pts, "t");
    CHECK_EQ(count(os.str(), "<time>"), 2U);
}

TEST(geojson_collection) {
    const auto pts = sample();
    std::vector<Geofence> fences{Geofence::circle("lab", {39.892, 32.78}, 30.0),
                                 Geofence::polygon("yard", {{39.0, 32.0}, {39.0, 33.0}, {40.0, 33.0}})};
    FenceEvent ev{"lab", FenceTransition::Enter, pts[0]};
    std::ostringstream os;
    write_geojson(os, pts, "ride \"x\"", fences, {ev});
    const std::string j = os.str();
    CHECK(j.rfind("{\"type\":\"FeatureCollection\"", 0) == 0);
    CHECK(j.find("\"name\":\"ride \\\"x\\\"\"") != std::string::npos);
    CHECK(j.find("\"type\":\"LineString\",\"coordinates\":[[32.7800000,39.8920000,990.0],") != std::string::npos);
    CHECK(j.find("\"start\":\"2026-05-14T08:30:00Z\",\"end\":\"2026-05-14T08:30:02Z\"") != std::string::npos);
    CHECK_EQ(count(j, "\"kind\":\"geofence\""), 2U);
    CHECK(j.find("\"radius_m\":30.0") != std::string::npos);
    CHECK(j.find("\"kind\":\"fence-event\",\"fence\":\"lab\",\"transition\":\"ENTER\"") != std::string::npos);
    std::size_t depth = 0;
    std::size_t max_depth = 0;
    for (const char c : j) {
        if (c == '{' || c == '[') {
            max_depth = std::max(max_depth, ++depth);
        } else if (c == '}' || c == ']') {
            REQUIRE(depth > 0U);
            --depth;
        }
    }
    CHECK_EQ(depth, 0U);
}

TEST(geojson_single_and_empty) {
    std::ostringstream one;
    write_geojson(one, {sample()[0]}, "p");
    CHECK(one.str().find("\"type\":\"Point\"") != std::string::npos);
    std::ostringstream none;
    write_geojson(none, {}, "p");
    CHECK_EQ(none.str(), "{\"type\":\"FeatureCollection\",\"features\":[\n]}\n");
}
