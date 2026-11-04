#include "nmea/parser.hpp"

#include <charconv>
#include <cstddef>
#include <system_error>

namespace nmea {

std::string_view to_string(FixQuality quality) noexcept {
    switch (quality) {
        case FixQuality::Invalid: return "NOFIX";
        case FixQuality::Gps: return "GPS";
        case FixQuality::Dgps: return "DGPS";
        case FixQuality::Pps: return "PPS";
        case FixQuality::RtkFixed: return "RTK";
        case FixQuality::RtkFloat: return "FRTK";
        case FixQuality::Estimated: return "DR";
        case FixQuality::Manual: return "MANUAL";
        case FixQuality::Simulation: return "SIM";
    }
    return "UNKNOWN";
}

std::string_view message_type(const Message& message) noexcept {
    switch (message.index()) {
        case 0: return "GGA";
        case 1: return "RMC";
        case 2: return "GSA";
        case 3: return "GSV";
        default: return "VTG";
    }
}

namespace {

[[nodiscard]] bool to_double(std::string_view text, double& out) noexcept {
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
    }
    const char* const end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, out);
    return ec == std::errc{} && ptr == end;
}

[[nodiscard]] bool to_int(std::string_view text, int& out) noexcept {
    const char* const end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, out);
    return ec == std::errc{} && ptr == end;
}

/// Field accessor that records the first decoding failure.
class FieldReader {
public:
    explicit FieldReader(const std::vector<std::string>& fields) noexcept : fields_(&fields) {}

    [[nodiscard]] std::size_t size() const noexcept { return fields_->size(); }

    [[nodiscard]] std::string_view text(std::size_t i) const noexcept {
        return i < fields_->size() ? std::string_view((*fields_)[i]) : std::string_view{};
    }

    [[nodiscard]] std::optional<double> real(std::size_t i) {
        const auto s = text(i);
        double v = 0.0;
        if (s.empty()) {
            return std::nullopt;
        }
        if (!to_double(s, v)) {
            bad_ = true;
            return std::nullopt;
        }
        return v;
    }

    [[nodiscard]] std::optional<int> integer(std::size_t i) {
        const auto s = text(i);
        int v = 0;
        if (s.empty()) {
            return std::nullopt;
        }
        if (!to_int(s, v)) {
            bad_ = true;
            return std::nullopt;
        }
        return v;
    }

    [[nodiscard]] std::optional<char> letter(std::size_t i) {
        const auto s = text(i);
        if (s.empty()) {
            return std::nullopt;
        }
        if (s.size() != 1) {
            bad_ = true;
            return std::nullopt;
        }
        return s.front();
    }

    /// "hhmmss[.sss]"
    [[nodiscard]] std::optional<UtcTime> time(std::size_t i) {
        const auto s = text(i);
        if (s.empty()) {
            return std::nullopt;
        }
        int hh = 0;
        int mm = 0;
        double ss = 0.0;
        if (s.size() < 6 || !to_int(s.substr(0, 2), hh) || !to_int(s.substr(2, 2), mm) ||
            !to_double(s.substr(4), ss) || hh > 23 || mm > 59 || ss < 0.0 || ss >= 61.0 || hh < 0 || mm < 0) {
            bad_ = true;
            return std::nullopt;
        }
        return UtcTime{hh, mm, ss};
    }

    /// "ddmmyy"; two-digit years 80..99 map to 19xx (GPS epoch is 1980).
    [[nodiscard]] std::optional<Date> date(std::size_t i) {
        const auto s = text(i);
        if (s.empty()) {
            return std::nullopt;
        }
        int dd = 0;
        int mo = 0;
        int yy = 0;
        if (s.size() != 6 || !to_int(s.substr(0, 2), dd) || !to_int(s.substr(2, 2), mo) ||
            !to_int(s.substr(4, 2), yy) || dd < 1 || dd > 31 || mo < 1 || mo > 12 || yy < 0) {
            bad_ = true;
            return std::nullopt;
        }
        return Date{yy >= 80 ? 1900 + yy : 2000 + yy, mo, dd};
    }

    /// Latitude or longitude in "(d)ddmm.mmmm" plus hemisphere letter.
    [[nodiscard]] std::optional<double> coordinate(std::size_t value_index, std::size_t hemi_index,
                                                   bool latitude) {
        const auto value = real(value_index);
        const auto hemi = letter(hemi_index);
        if (!value && !hemi) {
            return std::nullopt;
        }
        if (!value || !hemi || *value < 0.0) {
            bad_ = true;
            return std::nullopt;
        }
        const double degrees = static_cast<double>(static_cast<long>(*value / 100.0));
        const double minutes = *value - degrees * 100.0;
        const double limit = latitude ? 90.0 : 180.0;
        double result = degrees + minutes / 60.0;
        if (minutes >= 60.0 || result > limit) {
            bad_ = true;
            return std::nullopt;
        }
        const char positive = latitude ? 'N' : 'E';
        const char negative = latitude ? 'S' : 'W';
        if (*hemi == negative) {
            result = -result;
        } else if (*hemi != positive) {
            bad_ = true;
            return std::nullopt;
        }
        return result;
    }

    [[nodiscard]] std::optional<GeoPoint> position(std::size_t lat_index) {
        const auto lat = coordinate(lat_index, lat_index + 1, true);
        const auto lon = coordinate(lat_index + 2, lat_index + 3, false);
        if (lat && lon) {
            return GeoPoint{*lat, *lon};
        }
        if (lat || lon) {
            bad_ = true;  // Half a position is a corrupt sentence.
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<FaaMode> faa_mode(std::size_t i) {
        const auto c = letter(i);
        if (!c) {
            return std::nullopt;
        }
        switch (*c) {
            case 'A': return FaaMode::Autonomous;
            case 'D': return FaaMode::Differential;
            case 'E': return FaaMode::Estimated;
            case 'M': return FaaMode::Manual;
            case 'S': return FaaMode::Simulated;
            case 'N': return FaaMode::NotValid;
            case 'P': return FaaMode::Precise;
            case 'R': return FaaMode::RtkFixed;
            case 'F': return FaaMode::RtkFloat;
            default: bad_ = true; return std::nullopt;
        }
    }

    [[nodiscard]] bool bad() const noexcept { return bad_; }

private:
    const std::vector<std::string>* fields_;
    bool bad_{false};
};

[[nodiscard]] Result<Message> parse_gga(const Sentence& s) {
    FieldReader f(s.fields);
    if (f.size() < 14) {
        return ParseError::MissingField;
    }
    Gga m;
    m.talker = s.talker;
    m.time = f.time(0);
    m.position = f.position(1);
    const int quality = f.integer(5).value_or(0);
    if (quality < 0 || quality > 8) {
        return ParseError::BadField;
    }
    m.quality = static_cast<FixQuality>(quality);
    m.satellites = f.integer(6);
    m.hdop = f.real(7);
    m.altitude_m = f.real(8);
    m.geoid_separation_m = f.real(10);
    m.dgps_age_s = f.real(12);
    m.dgps_station = f.integer(13);
    if (f.bad() || (m.satellites && *m.satellites < 0) || (m.hdop && *m.hdop < 0.0)) {
        return ParseError::BadField;
    }
    return Message{std::move(m)};
}

[[nodiscard]] Result<Message> parse_rmc(const Sentence& s) {
    FieldReader f(s.fields);
    if (f.size() < 11) {
        return ParseError::MissingField;
    }
    Rmc m;
    m.talker = s.talker;
    m.time = f.time(0);
    const auto status = f.letter(1);
    if (status && *status != 'A' && *status != 'V') {
        return ParseError::BadField;
    }
    m.valid = status == 'A';
    m.position = f.position(2);
    m.speed_knots = f.real(6);
    m.course_deg = f.real(7);
    m.date = f.date(8);
    const auto variation = f.real(9);
    const auto variation_dir = f.letter(10);
    if (variation) {
        m.magnetic_variation_deg = variation_dir == 'W' ? -*variation : *variation;
    }
    m.mode = f.faa_mode(11);
    if (f.bad() || (m.speed_knots && *m.speed_knots < 0.0)) {
        return ParseError::BadField;
    }
    if (m.mode == FaaMode::NotValid) {
        m.valid = false;
    }
    return Message{std::move(m)};
}

[[nodiscard]] Result<Message> parse_gsa(const Sentence& s) {
    FieldReader f(s.fields);
    if (f.size() < 17) {
        return ParseError::MissingField;
    }
    Gsa m;
    m.talker = s.talker;
    const auto selection = f.letter(0);
    if (selection && *selection != 'A' && *selection != 'M') {
        return ParseError::BadField;
    }
    m.automatic = selection != 'M';
    const int fix = f.integer(1).value_or(1);
    if (fix < 1 || fix > 3) {
        return ParseError::BadField;
    }
    m.fix_type = static_cast<GsaFixType>(fix);
    for (std::size_t i = 2; i < 14; ++i) {
        if (const auto prn = f.integer(i)) {
            m.satellites.push_back(*prn);
        }
    }
    m.pdop = f.real(14);
    m.hdop = f.real(15);
    m.vdop = f.real(16);
    m.system_id = f.integer(17);
    if (f.bad()) {
        return ParseError::BadField;
    }
    return Message{std::move(m)};
}

[[nodiscard]] Result<Message> parse_gsv(const Sentence& s) {
    FieldReader f(s.fields);
    if (f.size() < 3) {
        return ParseError::MissingField;
    }
    Gsv m;
    m.talker = s.talker;
    const auto total = f.integer(0);
    const auto number = f.integer(1);
    const auto in_view = f.integer(2);
    if (!total || !number || !in_view || *total < 1 || *number < 1 || *number > *total || *in_view < 0) {
        return ParseError::BadField;
    }
    m.total_messages = *total;
    m.message_number = *number;
    m.satellites_in_view = *in_view;
    // Satellite blocks of four fields; an odd trailing field is the NMEA 4.1 signal ID.
    const std::size_t data = f.size() - 3;
    const std::size_t blocks = data / 4;
    for (std::size_t b = 0; b < blocks; ++b) {
        const std::size_t base = 3 + b * 4;
        const auto prn = f.integer(base);
        if (!prn) {
            continue;  // Empty padding block.
        }
        SatelliteInfo sat;
        sat.prn = *prn;
        sat.elevation_deg = f.integer(base + 1);
        sat.azimuth_deg = f.integer(base + 2);
        sat.snr_db = f.integer(base + 3);
        m.satellites.push_back(sat);
    }
    if (data % 4 == 1) {
        m.signal_id = f.integer(f.size() - 1);
    } else if (data % 4 != 0) {
        return ParseError::MissingField;
    }
    if (f.bad()) {
        return ParseError::BadField;
    }
    return Message{std::move(m)};
}

[[nodiscard]] Result<Message> parse_vtg(const Sentence& s) {
    FieldReader f(s.fields);
    if (f.size() < 8) {
        return ParseError::MissingField;
    }
    Vtg m;
    m.talker = s.talker;
    m.course_true_deg = f.real(0);
    m.course_magnetic_deg = f.real(2);
    m.speed_knots = f.real(4);
    m.speed_kmh = f.real(6);
    m.mode = f.faa_mode(8);
    if (f.bad() || (m.speed_knots && *m.speed_knots < 0.0) || (m.speed_kmh && *m.speed_kmh < 0.0)) {
        return ParseError::BadField;
    }
    return Message{std::move(m)};
}

}  // namespace

Result<Message> parse_sentence(const Sentence& sentence) {
    const std::string& t = sentence.type;
    if (t == "GGA") {
        return parse_gga(sentence);
    }
    if (t == "RMC") {
        return parse_rmc(sentence);
    }
    if (t == "GSA") {
        return parse_gsa(sentence);
    }
    if (t == "GSV") {
        return parse_gsv(sentence);
    }
    if (t == "VTG") {
        return parse_vtg(sentence);
    }
    return ParseError::UnsupportedType;
}

Result<Message> parse(std::string_view line, const ParseOptions& options) {
    auto sentence = split_sentence(line, options.require_checksum);
    if (!sentence) {
        return sentence.error();
    }
    return parse_sentence(sentence.value());
}

}  // namespace nmea
