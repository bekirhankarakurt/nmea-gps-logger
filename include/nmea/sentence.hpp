#pragma once

#include "nmea/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nmea {

/// Longest line accepted. NMEA 0183 caps sentences at 82 characters, but
/// multi-constellation receivers routinely exceed that, so allow headroom.
inline constexpr std::size_t kMaxSentenceLength = 128;

/// A checksum-validated sentence split into its address and data fields.
struct Sentence {
    std::string talker;               ///< Two-letter talker ID, e.g. "GP", "GN".
    std::string type;                 ///< Three-letter sentence formatter, e.g. "GGA".
    std::vector<std::string> fields;  ///< Data fields after the address, may be empty strings.
};

/// XOR of all bytes in @p payload (the text between '$' and '*').
[[nodiscard]] std::uint8_t compute_checksum(std::string_view payload) noexcept;

/// Validates framing and checksum, then splits a raw line into a Sentence.
/// Trailing CR/LF and surrounding whitespace are ignored.
[[nodiscard]] Result<Sentence> split_sentence(std::string_view line, bool require_checksum = true);

/// Builds a complete sentence "$<body>*hh" from a body such as "GPGGA,...".
[[nodiscard]] std::string make_sentence(std::string_view body);

}  // namespace nmea
