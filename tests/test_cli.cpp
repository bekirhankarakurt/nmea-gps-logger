#include "nmea/cli.hpp"
#include "test_framework.hpp"

using namespace nmea;

namespace {
CliParseResult cli(std::initializer_list<const char*> args) {
    std::vector<std::string> v;
    for (const char* a : args) {
        v.emplace_back(a);
    }
    return parse_cli(v);
}
}  // namespace

TEST(file_input_with_outputs) {
    const auto r = cli({"--file", "log.nmea", "--gpx", "t.gpx", "--geojson", "t.geojson", "-r", "10", "--vincenty",
                        "--max-hdop", "2.5", "--no-checksum", "--name", "Ride", "-q", "--json", "-v"});
    REQUIRE(r.options.has_value());
    const auto& o = *r.options;
    CHECK(o.input == InputKind::File);
    CHECK_EQ(o.file_path, "log.nmea");
    CHECK_EQ(o.gpx_path, "t.gpx");
    CHECK_EQ(o.geojson_path, "t.geojson");
    CHECK_NEAR(o.replay_speed, 10.0, 0.0);
    CHECK(o.pipeline.distance == DistanceMethod::Vincenty);
    CHECK_NEAR(o.pipeline.max_hdop.value_or(0), 2.5, 0.0);
    CHECK(!o.pipeline.parse.require_checksum);
    CHECK(o.pipeline.keep_points);
    CHECK_EQ(o.track_name, "Ride");
    CHECK(o.quiet && o.json_lines && o.verbose);
}

TEST(serial_and_udp_inputs) {
    const auto s = cli({"-s", "/dev/ttyUSB0", "-b", "4800", "--tee", "raw.nmea", "--duration", "60"});
    REQUIRE(s.options.has_value());
    CHECK(s.options->input == InputKind::Serial);
    CHECK_EQ(s.options->baud, 4800);
    CHECK_EQ(s.options->tee_path, "raw.nmea");
    CHECK_NEAR(s.options->duration_s.value_or(0), 60.0, 0.0);
    CHECK(!s.options->pipeline.keep_points);

    const auto u = cli({"--udp", "10110", "--bind", "127.0.0.1"});
    REQUIRE(u.options.has_value());
    CHECK(u.options->input == InputKind::Udp);
    CHECK_EQ(u.options->udp_port, 10110);
    CHECK_EQ(u.options->udp_bind, "127.0.0.1");
}

TEST(fences_from_cli) {
    const auto r = cli({"-f", "x", "--fence-circle", "home:39.89,32.78,150", "--fence-polygon",
                        "campus:39.0,32.0;39.0,33.0;40.0,33.0", "--fence-confirm", "3"});
    REQUIRE(r.options.has_value());
    REQUIRE(r.options->fences.size() == 2U);
    CHECK_EQ(r.options->fences[0].name(), "home");
    CHECK_NEAR(r.options->fences[0].radius_m(), 150.0, 0.0);
    CHECK(r.options->fences[1].shape() == FenceShape::Polygon);
    CHECK_EQ(r.options->pipeline.fence_confirm_fixes, 3);
}

TEST(fence_spec_validation) {
    CHECK(parse_circle_spec("a:1,2,3").has_value());
    CHECK(!parse_circle_spec("1,2,3").has_value());
    CHECK(!parse_circle_spec(":1,2,3").has_value());
    CHECK(!parse_circle_spec("a:1,2").has_value());
    CHECK(!parse_circle_spec("a:91,2,3").has_value());
    CHECK(!parse_circle_spec("a:1,2,-3").has_value());
    CHECK(!parse_circle_spec("a:1,x,3").has_value());
    CHECK(parse_polygon_spec("p:0,0;0,1;1,1").has_value());
    CHECK(!parse_polygon_spec("p:0,0;0,1").has_value());
    CHECK(!parse_polygon_spec("p:0,0;0,1;1").has_value());
    CHECK(!parse_polygon_spec("p:0,0;0,200;1,1").has_value());
}

TEST(usage_errors) {
    CHECK(!cli({}).options);
    CHECK(!cli({"--bogus"}).options);
    CHECK(!cli({"-f"}).options);
    CHECK(!cli({"-f", "a", "-u", "1"}).options);
    CHECK(!cli({"-u", "70000"}).options);
    CHECK(!cli({"-u", "abc"}).options);
    CHECK(!cli({"-s", "/dev/ttyS0", "-b", "1234"}).options);
    CHECK(!cli({"-u", "1", "-r", "2"}).options);
    CHECK(!cli({"-f", "a", "-r", "0"}).options);
    CHECK(!cli({"-f", "a", "--max-hdop", "-1"}).options);
    CHECK(!cli({"-f", "a", "--duration", "x"}).options);
    CHECK(!cli({"-f", "a", "--fence-confirm", "0"}).options);
    CHECK(!cli({"-f", "a", "--fence-circle", "bad"}).options);
    CHECK(!cli({"-f", "a", "--fence-polygon", "bad"}).options);
    CHECK(cli({"--bogus"}).error.find("unknown option") != std::string::npos);
}

TEST(help_and_version_need_no_input) {
    const auto h = cli({"--help"});
    REQUIRE(h.options.has_value());
    CHECK(h.options->show_help);
    CHECK(cli({"-V"}).options->show_version);
    CHECK(usage_text().find("--fence-circle") != std::string::npos);
}
