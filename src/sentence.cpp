#include "nmea/sentence.hpp"

#include <array>
#include <cctype>
#include <optional>

namespace nmea {

std::string_view to_string(ParseError error) noexcept {
    switch (error) {
        case ParseError::Empty: return "empty";
        case ParseError::MissingStart: return "missing-start";
        case ParseError::TooLong: return "too-long";
        case ParseError::MissingChecksum: return "missing-checksum";
        case ParseError::BadChecksumFormat: return "bad-checksum-format";
        case ParseError::ChecksumMismatch: return "checksum-mismatch";
        case ParseError::BadAddress: return "bad-address";
        case ParseError::UnsupportedType: return "unsupported-type";
        case ParseError::MissingField: return "missing-field";
        case ParseError::BadField: return "bad-field";
    }
    return "unknown";
}

namespace {

[[nodiscard]] bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

[[nodiscard]] std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] std::optional<std::uint8_t> hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return static_cast<std::uint8_t>(c - '0');
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<std::uint8_t>(c - 'A' + 10);
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<std::uint8_t>(c - 'a' + 10);
    }
    return std::nullopt;
}

[[nodiscard]] bool is_upper_alnum(char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

}  // namespace

std::uint8_t compute_checksum(std::string_view payload) noexcept {
    std::uint8_t sum = 0;
    for (const char c : payload) {
        sum = static_cast<std::uint8_t>(sum ^ static_cast<std::uint8_t>(c));
    }
    return sum;
}

Result<Sentence> split_sentence(std::string_view line, bool require_checksum) {
    line = trim(line);
    if (line.empty()) {
        return ParseError::Empty;
    }
    if (line.size() > kMaxSentenceLength) {
        return ParseError::TooLong;
    }
    if (line.front() != '$' && line.front() != '!') {
        return ParseError::MissingStart;
    }
    line.remove_prefix(1);

    std::string_view payload = line;
    const auto star = line.rfind('*');
    if (star == std::string_view::npos) {
        if (require_checksum) {
            return ParseError::MissingChecksum;
        }
    } else {
        const std::string_view digits = line.substr(star + 1);
        if (digits.size() != 2) {
            return ParseError::BadChecksumFormat;
        }
        const auto hi = hex_value(digits[0]);
        const auto lo = hex_value(digits[1]);
        if (!hi || !lo) {
            return ParseError::BadChecksumFormat;
        }
        payload = line.substr(0, star);
        const auto expected = static_cast<std::uint8_t>((*hi << 4U) | *lo);
        if (compute_checksum(payload) != expected) {
            return ParseError::ChecksumMismatch;
        }
    }

    const auto comma = payload.find(',');
    const std::string_view address = payload.substr(0, comma);
    for (const char c : address) {
        if (!is_upper_alnum(c)) {
            return ParseError::BadAddress;
        }
    }
    // Proprietary sentences ("$PGRMZ", "$PUBX") use 'P' plus a vendor-defined type.
    const bool proprietary = address.size() >= 2 && address.front() == 'P';
    if (!proprietary && address.size() != 5) {
        return ParseError::BadAddress;
    }

    Sentence sentence;
    const std::size_t talker_length = proprietary ? 1 : 2;
    sentence.talker = std::string(address.substr(0, talker_length));
    sentence.type = std::string(address.substr(talker_length));
    if (comma != std::string_view::npos) {
        std::string_view rest = payload.substr(comma + 1);
        while (true) {
            const auto next = rest.find(',');
            sentence.fields.emplace_back(rest.substr(0, next));
            if (next == std::string_view::npos) {
                break;
            }
            rest.remove_prefix(next + 1);
        }
    }
    return sentence;
}

std::string make_sentence(std::string_view body) {
    static constexpr std::array<char, 16> kHex{'0', '1', '2', '3', '4', '5', '6', '7',
                                               '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    const std::uint8_t sum = compute_checksum(body);
    std::string out;
    out.reserve(body.size() + 4);
    out.push_back('$');
    out.append(body);
    out.push_back('*');
    out.push_back(kHex.at(static_cast<std::size_t>(sum >> 4U)));
    out.push_back(kHex.at(static_cast<std::size_t>(sum & 0x0FU)));
    return out;
}

}  // namespace nmea
