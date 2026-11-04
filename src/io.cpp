#include "nmea/io.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <termios.h>
#include <unistd.h>
#include <array>
#include <utility>

namespace nmea {

UniqueFd::~UniqueFd() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

void LineBuffer::append(std::string_view bytes) {
    for (const char c : bytes) {
        if (c == '\n') {
            if (!discarding_ && !partial_.empty()) {
                lines_.push_back(std::move(partial_));
            }
            partial_.clear();
            discarding_ = false;
        } else if (c == '\r' || c == '\0') {
            continue;
        } else if (!discarding_) {
            if (partial_.size() >= max_line_) {
                partial_.clear();
                discarding_ = true;
            } else {
                partial_.push_back(c);
            }
        }
    }
}

std::optional<std::string> LineBuffer::pop() {
    if (lines_.empty()) {
        return std::nullopt;
    }
    std::string line = std::move(lines_.front());
    lines_.pop_front();
    return line;
}

std::optional<std::string> LineBuffer::take_remainder() {
    if (discarding_ || partial_.empty()) {
        partial_.clear();
        discarding_ = false;
        return std::nullopt;
    }
    std::string rest = std::move(partial_);
    partial_.clear();
    return rest;
}

FileSource::FileSource(std::string path) : path_(std::move(path)) {
    if (path_ != "-") {
        file_.open(path_);
        if (!file_) {
            throw std::runtime_error("cannot open input file: " + path_);
        }
    }
}

std::optional<std::string> FileSource::next_line() {
    std::istream& in = path_ == "-" ? std::cin : file_;
    std::string line;
    if (!std::getline(in, line)) {
        return std::nullopt;
    }
    return line;
}

std::string FileSource::describe() const { return path_ == "-" ? "stdin" : "file " + path_; }

namespace {

constexpr int kPollTimeoutMs = 200;

[[noreturn]] void throw_errno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

/// Waits until @p fd is readable or the stop flag is raised.
/// Returns false when stopped.
[[nodiscard]] bool wait_readable(int fd, const std::atomic<bool>& stop) {
    while (!stop.load()) {
        pollfd p{};
        p.fd = fd;
        p.events = POLLIN;
        const int rc = ::poll(&p, 1, kPollTimeoutMs);
        if (rc > 0) {
            return true;
        }
        if (rc < 0 && errno != EINTR) {
            throw_errno("poll");
        }
    }
    return false;
}

}  // namespace

std::optional<unsigned int> baud_to_speed(int baud) noexcept {
    switch (baud) {
        case 1200: return B1200;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default: return std::nullopt;
    }
}

SerialSource::SerialSource(const std::string& device, int baud, const std::atomic<bool>& stop)
    : device_(device), baud_(baud), stop_(&stop) {
    const auto speed = baud_to_speed(baud);
    if (!speed) {
        throw std::invalid_argument("unsupported baud rate: " + std::to_string(baud));
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg): POSIX open() is variadic.
    fd_ = UniqueFd(::open(device.c_str(), O_RDONLY | O_NOCTTY | O_CLOEXEC));
    if (!fd_.valid()) {
        throw_errno("cannot open serial device " + device);
    }
    termios tty{};
    if (::tcgetattr(fd_.get(), &tty) != 0) {
        throw_errno("tcgetattr " + device);
    }
    ::cfmakeraw(&tty);
    tty.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    tty.c_cflag &= ~static_cast<tcflag_t>(CSTOPB | PARENB | CRTSCTS);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    if (::cfsetispeed(&tty, static_cast<speed_t>(*speed)) != 0 ||
        ::cfsetospeed(&tty, static_cast<speed_t>(*speed)) != 0) {
        throw_errno("cfsetspeed " + device);
    }
    if (::tcsetattr(fd_.get(), TCSANOW, &tty) != 0) {
        throw_errno("tcsetattr " + device);
    }
    ::tcflush(fd_.get(), TCIFLUSH);
}

std::optional<std::string> SerialSource::next_line() {
    std::array<char, 512> buf{};
    while (true) {
        if (auto line = buffer_.pop()) {
            return line;
        }
        if (!wait_readable(fd_.get(), *stop_)) {
            return std::nullopt;
        }
        const ssize_t n = ::read(fd_.get(), buf.data(), buf.size());
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            throw_errno("read " + device_);
        }
        if (n == 0) {
            // Device hung up (USB receiver unplugged or pty closed).
            return buffer_.take_remainder();
        }
        buffer_.append(std::string_view(buf.data(), static_cast<std::size_t>(n)));
    }
}

std::string SerialSource::describe() const {
    return "serial " + device_ + " @ " + std::to_string(baud_) + " baud";
}

UdpSource::UdpSource(const std::string& bind_address, std::uint16_t port, const std::atomic<bool>& stop)
    : address_(bind_address), port_(port), stop_(&stop), fd_(::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)) {
    if (!fd_.valid()) {
        throw_errno("socket");
    }
    const int reuse = 1;
    if (::setsockopt(fd_.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) != 0) {
        throw_errno("setsockopt SO_REUSEADDR");
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr) != 1) {
        throw std::invalid_argument("invalid IPv4 bind address: " + bind_address);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): BSD socket API.
    if (::bind(fd_.get(), reinterpret_cast<const sockaddr*>(&addr), sizeof addr) != 0) {
        throw_errno("bind " + bind_address + ":" + std::to_string(port));
    }
    sockaddr_in bound{};
    socklen_t len = sizeof bound;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): BSD socket API.
    if (::getsockname(fd_.get(), reinterpret_cast<sockaddr*>(&bound), &len) != 0) {
        throw_errno("getsockname");
    }
    port_ = ntohs(bound.sin_port);
}

std::optional<std::string> UdpSource::next_line() {
    std::array<char, 2048> buf{};
    while (true) {
        if (auto line = buffer_.pop()) {
            return line;
        }
        if (!wait_readable(fd_.get(), *stop_)) {
            return std::nullopt;
        }
        const ssize_t n = ::recv(fd_.get(), buf.data(), buf.size(), 0);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            throw_errno("recv");
        }
        buffer_.append(std::string_view(buf.data(), static_cast<std::size_t>(n)));
        // Each datagram is self-contained; a missing final newline still ends a line.
        if (auto rest = buffer_.take_remainder()) {
            buffer_.append(*rest + "\n");
        }
    }
}

std::string UdpSource::describe() const {
    return "udp " + address_ + ":" + std::to_string(port_);
}

}  // namespace nmea
