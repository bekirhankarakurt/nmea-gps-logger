#pragma once

#include <string_view>
#include <utility>
#include <variant>

namespace nmea {

/// Reasons a sentence can be rejected by the splitter or a message parser.
enum class ParseError {
    Empty,             ///< Line is empty or whitespace only.
    MissingStart,      ///< Line does not start with '$' or '!'.
    TooLong,           ///< Line exceeds the accepted maximum length.
    MissingChecksum,   ///< No '*hh' suffix and checksums are required.
    BadChecksumFormat, ///< '*' is not followed by exactly two hex digits.
    ChecksumMismatch,  ///< Computed and transmitted checksums differ.
    BadAddress,        ///< Talker/type address field is malformed.
    UnsupportedType,   ///< Well-formed sentence of a type this parser ignores.
    MissingField,      ///< Fewer fields than the sentence type requires.
    BadField,          ///< A field could not be decoded or is out of range.
};

/// Human-readable, stable identifier for a parse error.
[[nodiscard]] std::string_view to_string(ParseError error) noexcept;

/// Minimal value-or-error type (C++17 stand-in for std::expected).
template <typename T>
class Result {
public:
    // Implicit by design so parsers can `return value;` or `return ParseError::X;`.
    // cppcheck-suppress noExplicitConstructor
    Result(T value) : data_(std::move(value)) {}  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
    // cppcheck-suppress noExplicitConstructor
    Result(ParseError error) : data_(error) {}    // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    [[nodiscard]] bool ok() const noexcept { return std::holds_alternative<T>(data_); }
    [[nodiscard]] explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] const T& value() const& { return std::get<T>(data_); }
    [[nodiscard]] T& value() & { return std::get<T>(data_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(data_)); }
    [[nodiscard]] ParseError error() const { return std::get<ParseError>(data_); }

private:
    std::variant<T, ParseError> data_;
};

}  // namespace nmea
