#include "nmea/sentence.hpp"
#include "test_framework.hpp"

using namespace nmea;

TEST(checksum_matches_reference_sentence) {
    // Reference GGA from the NMEA 0183 documentation examples.
    const std::string body = "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,";
    CHECK_EQ(static_cast<int>(compute_checksum(body)), 0x47);
    CHECK_EQ(make_sentence(body), "$" + body + "*47");
}

TEST(split_valid_sentence) {
    const auto r = split_sentence("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
    REQUIRE(r.ok());
    CHECK_EQ(r.value().talker, "GP");
    CHECK_EQ(r.value().type, "GGA");
    CHECK_EQ(r.value().fields.size(), 14U);
    CHECK_EQ(r.value().fields[0], "123519");
    CHECK(r.value().fields[13].empty());
}

TEST(lowercase_checksum_accepted) {
    std::string s = make_sentence("GPGSA,A,3,,,,,,,,,,,,,1.4,0.8,1.2");
    for (std::size_t i = s.size() - 2; i < s.size(); ++i) {
        if (s[i] >= 'A' && s[i] <= 'F') {
            s[i] = static_cast<char>(s[i] - 'A' + 'a');
        }
    }
    CHECK(split_sentence(s).ok());
}

TEST(rejects_bad_framing) {
    CHECK(split_sentence("").error() == ParseError::Empty);
    CHECK(split_sentence("   \r\n").error() == ParseError::Empty);
    CHECK(split_sentence("GPGGA,1*00").error() == ParseError::MissingStart);
    CHECK(split_sentence("$GPGGA,1,2,3").error() == ParseError::MissingChecksum);
    CHECK(split_sentence("$GPGGA,1*4").error() == ParseError::BadChecksumFormat);
    CHECK(split_sentence("$GPGGA,1*ZZ").error() == ParseError::BadChecksumFormat);
    CHECK(split_sentence("$GPGGA,1*00").error() == ParseError::ChecksumMismatch);
    CHECK(split_sentence(make_sentence("GPGG,1")).error() == ParseError::BadAddress);
    CHECK(split_sentence(make_sentence("gpgga,1")).error() == ParseError::BadAddress);
    CHECK(split_sentence("$" + std::string(200, 'A')).error() == ParseError::TooLong);
}

TEST(checksum_optional_when_allowed) {
    const auto r = split_sentence("$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K", false);
    REQUIRE(r.ok());
    CHECK_EQ(r.value().fields.size(), 8U);
}

TEST(proprietary_sentence_address) {
    const auto r = split_sentence(make_sentence("PGRMZ,3250,f,3"));
    REQUIRE(r.ok());
    CHECK_EQ(r.value().talker, "P");
    CHECK_EQ(r.value().type, "GRMZ");
    CHECK(split_sentence(make_sentence("PUBX,00,1")).ok());
}

TEST(sentence_without_fields) {
    const auto r = split_sentence(make_sentence("GPTXT"));
    REQUIRE(r.ok());
    CHECK(r.value().fields.empty());
}

TEST(error_names_are_stable) {
    CHECK_EQ(to_string(ParseError::ChecksumMismatch), "checksum-mismatch");
    CHECK_EQ(to_string(ParseError::UnsupportedType), "unsupported-type");
    CHECK_EQ(to_string(ParseError::Empty), "empty");
}
