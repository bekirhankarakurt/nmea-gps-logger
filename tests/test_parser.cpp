#include "nmea/parser.hpp"
#include "test_framework.hpp"

using namespace nmea;

namespace {
template <typename T>
T as(const Result<Message>& r) {
    REQUIRE(r.ok());
    REQUIRE(std::holds_alternative<T>(r.value()));
    return std::get<T>(r.value());
}
}  // namespace

TEST(gga_full) {
    const auto r = parse("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47");
    const auto m = as<Gga>(r);
    CHECK_EQ(m.talker, "GP");
    REQUIRE(m.time.has_value());
    CHECK_EQ(m.time->hour, 12);
    CHECK_EQ(m.time->minute, 35);
    CHECK_NEAR(m.time->second, 19.0, 1e-9);
    REQUIRE(m.position.has_value());
    CHECK_NEAR(m.position->lat_deg, 48.0 + 7.038 / 60.0, 1e-9);
    CHECK_NEAR(m.position->lon_deg, 11.0 + 31.0 / 60.0, 1e-9);
    CHECK(m.quality == FixQuality::Gps);
    CHECK_EQ(m.satellites.value_or(-1), 8);
    CHECK_NEAR(m.hdop.value_or(0), 0.9, 1e-9);
    CHECK_NEAR(m.altitude_m.value_or(0), 545.4, 1e-9);
    CHECK_NEAR(m.geoid_separation_m.value_or(0), 46.9, 1e-9);
    CHECK(!m.dgps_age_s);
    CHECK_EQ(message_type(r.value()), "GGA");
}

TEST(gga_southern_western_and_dgps) {
    const auto r = parse(make_sentence("GNGGA,000001.50,3351.4300,S,15112.6500,W,2,12,0.6,10.0,M,20.0,M,1.2,0137"));
    const auto m = as<Gga>(r);
    CHECK_NEAR(m.position->lat_deg, -(33.0 + 51.43 / 60.0), 1e-9);
    CHECK_NEAR(m.position->lon_deg, -(151.0 + 12.65 / 60.0), 1e-9);
    CHECK(m.quality == FixQuality::Dgps);
    CHECK_NEAR(m.time->second, 1.5, 1e-9);
    CHECK_NEAR(m.dgps_age_s.value_or(0), 1.2, 1e-9);
    CHECK_EQ(m.dgps_station.value_or(0), 137);
}

TEST(gga_no_fix_has_empty_fields) {
    const auto m = as<Gga>(parse(make_sentence("GPGGA,083140.00,,,,,0,00,99.9,,M,,M,,")));
    CHECK(m.quality == FixQuality::Invalid);
    CHECK(!m.position);
    CHECK(!m.altitude_m);
}

TEST(gga_rejects_corrupt_fields) {
    CHECK(parse(make_sentence("GPGGA,123519,4807.038,X,01131.000,E,1,08,0.9,545.4,M,46.9,M,,")).error() ==
          ParseError::BadField);
    CHECK(parse(make_sentence("GPGGA,123519,4867.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,")).error() ==
          ParseError::BadField);
    CHECK(parse(make_sentence("GPGGA,123519,4807.038,N,,,1,08,0.9,545.4,M,46.9,M,,")).error() ==
          ParseError::BadField);
    CHECK(parse(make_sentence("GPGGA,256019,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,")).error() ==
          ParseError::BadField);
    CHECK(parse(make_sentence("GPGGA,123519,4807.038,N,01131.000,E,9,08,0.9,545.4,M,46.9,M,,")).error() ==
          ParseError::BadField);
    CHECK(parse(make_sentence("GPGGA,123519,4807.038,N,01131.000,E,1,08,abc,545.4,M,46.9,M,,")).error() ==
          ParseError::BadField);
    CHECK(parse(make_sentence("GPGGA,123519,4807.038,N")).error() == ParseError::MissingField);
}

TEST(rmc_full) {
    const auto r = parse("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
    const auto m = as<Rmc>(r);
    CHECK(m.valid);
    CHECK_NEAR(m.speed_knots.value_or(0), 22.4, 1e-9);
    CHECK_NEAR(m.course_deg.value_or(0), 84.4, 1e-9);
    REQUIRE(m.date.has_value());
    CHECK_EQ(m.date->year, 1994);
    CHECK_EQ(m.date->month, 3);
    CHECK_EQ(m.date->day, 23);
    CHECK_NEAR(m.magnetic_variation_deg.value_or(0), -3.1, 1e-9);
    CHECK(!m.mode);
}

TEST(rmc_mode_and_void) {
    const auto a = as<Rmc>(parse(make_sentence("GNRMC,083000.00,A,3953.5196,N,03246.8000,E,1.0,90.0,140526,,,D")));
    CHECK(a.mode == FaaMode::Differential);
    CHECK_EQ(a.date->year, 2026);
    const auto v = as<Rmc>(parse(make_sentence("GPRMC,083000.00,V,,,,,,,140526,,,N")));
    CHECK(!v.valid);
    CHECK(!v.position);
    const auto n = as<Rmc>(parse(make_sentence("GPRMC,083000.00,A,3953.5196,N,03246.8000,E,,,140526,,,N")));
    CHECK(!n.valid);  // Mode 'N' overrides status.
    CHECK(parse(make_sentence("GPRMC,083000.00,X,,,,,,,140526,,")).error() == ParseError::BadField);
    CHECK(parse(make_sentence("GPRMC,083000.00,A,,,,,,,141326,,")).error() == ParseError::BadField);
    CHECK(parse(make_sentence("GPRMC,083000.00,A,,,,,,,140526,,,Q")).error() == ParseError::BadField);
}

TEST(gsa_full) {
    const auto r = parse(make_sentence("GNGSA,A,3,02,05,07,09,13,15,,,,,,,1.4,0.8,1.2,1"));
    const auto m = as<Gsa>(r);
    CHECK(m.automatic);
    CHECK(m.fix_type == GsaFixType::Fix3D);
    CHECK_EQ(m.satellites.size(), 6U);
    CHECK_EQ(m.satellites.front(), 2);
    CHECK_NEAR(m.pdop.value_or(0), 1.4, 1e-9);
    CHECK_NEAR(m.hdop.value_or(0), 0.8, 1e-9);
    CHECK_NEAR(m.vdop.value_or(0), 1.2, 1e-9);
    CHECK_EQ(m.system_id.value_or(0), 1);
    CHECK(parse(make_sentence("GPGSA,A,7,,,,,,,,,,,,,1,1,1")).error() == ParseError::BadField);
    CHECK(parse(make_sentence("GPGSA,Z,3,,,,,,,,,,,,,1,1,1")).error() == ParseError::BadField);
    CHECK(parse(make_sentence("GPGSA,A,3")).error() == ParseError::MissingField);
}

TEST(gsv_blocks_padding_and_signal_id) {
    const auto m = as<Gsv>(parse(make_sentence("GPGSV,3,3,10,30,21,330,36,24,04,210,")));
    CHECK_EQ(m.total_messages, 3);
    CHECK_EQ(m.message_number, 3);
    CHECK_EQ(m.satellites_in_view, 10);
    REQUIRE(m.satellites.size() == 2U);
    CHECK_EQ(m.satellites[1].prn, 24);
    CHECK(!m.satellites[1].snr_db);
    CHECK(!m.signal_id);

    const auto s = as<Gsv>(parse(make_sentence("GLGSV,1,1,01,65,40,100,35,1")));
    CHECK_EQ(s.signal_id.value_or(0), 1);
    CHECK_EQ(s.talker, "GL");

    const auto empty = as<Gsv>(parse(make_sentence("GPGSV,1,1,00")));
    CHECK(empty.satellites.empty());

    CHECK(parse(make_sentence("GPGSV,1,2,04,01,02,003,40")).error() == ParseError::BadField);
    CHECK(parse(make_sentence("GPGSV,1,1,04,01,02")).error() == ParseError::MissingField);
}

TEST(vtg_full) {
    const auto m = as<Vtg>(parse(make_sentence("GPVTG,054.7,T,034.4,M,005.5,N,010.2,K,A")));
    CHECK_NEAR(m.course_true_deg.value_or(0), 54.7, 1e-9);
    CHECK_NEAR(m.course_magnetic_deg.value_or(0), 34.4, 1e-9);
    CHECK_NEAR(m.speed_knots.value_or(0), 5.5, 1e-9);
    CHECK_NEAR(m.speed_kmh.value_or(0), 10.2, 1e-9);
    CHECK(m.mode == FaaMode::Autonomous);
    CHECK(parse(make_sentence("GPVTG,054.7,T,034.4,M,-1,N,010.2,K")).error() == ParseError::BadField);
    CHECK(parse(make_sentence("GPVTG,054.7,T")).error() == ParseError::MissingField);
}

TEST(unsupported_and_checksum_options) {
    CHECK(parse(make_sentence("GPGLL,4916.45,N,12311.12,W,225444,A")).error() == ParseError::UnsupportedType);
    CHECK(parse(make_sentence("PGRMZ,3250,f,3")).error() == ParseError::UnsupportedType);
    CHECK(parse("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K").error() == ParseError::MissingChecksum);
    ParseOptions lenient;
    lenient.require_checksum = false;
    CHECK(parse("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K", lenient).ok());
}

TEST(fix_quality_names) {
    CHECK_EQ(to_string(FixQuality::RtkFixed), "RTK");
    CHECK_EQ(to_string(FixQuality::Invalid), "NOFIX");
    CHECK_EQ(to_string(FixQuality::Gps), "GPS");
}
