#pragma once

#include "nmea/messages.hpp"
#include "nmea/result.hpp"
#include "nmea/sentence.hpp"

#include <string_view>

namespace nmea {

struct ParseOptions {
    /// Reject sentences without a '*hh' checksum. Some log files strip it.
    bool require_checksum{true};
};

/// Decodes an already split sentence. Unknown types yield UnsupportedType.
[[nodiscard]] Result<Message> parse_sentence(const Sentence& sentence);

/// Validates, splits and decodes one raw NMEA line.
[[nodiscard]] Result<Message> parse(std::string_view line, const ParseOptions& options = {});

}  // namespace nmea
