#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace nmea {

/// Owning POSIX file descriptor (move-only, closes on destruction).
class UniqueFd {
public:
    UniqueFd() noexcept = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    ~UniqueFd();
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept;
    UniqueFd& operator=(UniqueFd&& other) noexcept;

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

private:
    int fd_{-1};
};

/// Reassembles lines from arbitrary byte chunks (serial reads, datagrams).
/// CR is stripped; lines longer than the limit are discarded as noise.
class LineBuffer {
public:
    explicit LineBuffer(std::size_t max_line = 1024) noexcept : max_line_(max_line) {}

    void append(std::string_view bytes);
    [[nodiscard]] std::optional<std::string> pop();
    /// Returns a trailing partial line (used at end of stream).
    [[nodiscard]] std::optional<std::string> take_remainder();

private:
    std::size_t max_line_;
    std::string partial_;
    bool discarding_{false};
    std::deque<std::string> lines_;
};

/// Blocking source of text lines. Implementations return nullopt at end of
/// input or once the stop flag is raised.
class LineSource {
public:
    LineSource() = default;
    virtual ~LineSource() = default;
    LineSource(const LineSource&) = delete;
    LineSource& operator=(const LineSource&) = delete;
    LineSource(LineSource&&) = delete;
    LineSource& operator=(LineSource&&) = delete;

    [[nodiscard]] virtual std::optional<std::string> next_line() = 0;
    [[nodiscard]] virtual std::string describe() const = 0;
};

/// Reads a log file, or standard input for path "-".
class FileSource final : public LineSource {
public:
    explicit FileSource(std::string path);
    [[nodiscard]] std::optional<std::string> next_line() override;
    [[nodiscard]] std::string describe() const override;

private:
    std::string path_;
    std::ifstream file_;
};

/// Maps a numeric baud rate to its termios constant; nullopt if unsupported.
[[nodiscard]] std::optional<unsigned int> baud_to_speed(int baud) noexcept;

/// Serial port configured raw 8N1 via termios.
class SerialSource final : public LineSource {
public:
    SerialSource(const std::string& device, int baud, const std::atomic<bool>& stop);
    [[nodiscard]] std::optional<std::string> next_line() override;
    [[nodiscard]] std::string describe() const override;

private:
    std::string device_;
    int baud_;
    const std::atomic<bool>* stop_;
    UniqueFd fd_;
    LineBuffer buffer_;
};

/// UDP listener for phone GPS apps that stream NMEA datagrams.
class UdpSource final : public LineSource {
public:
    /// Port 0 binds an ephemeral port (see port()).
    UdpSource(const std::string& bind_address, std::uint16_t port, const std::atomic<bool>& stop);
    [[nodiscard]] std::optional<std::string> next_line() override;
    [[nodiscard]] std::string describe() const override;
    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
    std::string address_;
    std::uint16_t port_;
    const std::atomic<bool>* stop_;
    UniqueFd fd_;
    LineBuffer buffer_;
};

}  // namespace nmea
