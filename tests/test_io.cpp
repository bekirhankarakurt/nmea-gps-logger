#include "nmea/io.hpp"
#include "nmea/sentence.hpp"
#include "test_framework.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <thread>

using namespace nmea;

TEST(line_buffer_reassembles_chunks) {
    LineBuffer b;
    b.append("$GPGGA,1");
    CHECK(!b.pop());
    b.append("23\r\n$GPRMC");
    CHECK_EQ(b.pop().value_or(""), "$GPGGA,123");
    CHECK(!b.pop());
    b.append(",x\n\n\r\n");
    CHECK_EQ(b.pop().value_or(""), "$GPRMC,x");
    CHECK(!b.pop());
    b.append("tail");
    CHECK_EQ(b.take_remainder().value_or(""), "tail");
    CHECK(!b.take_remainder());
}

TEST(line_buffer_discards_overlong_noise) {
    LineBuffer b(8);
    b.append("0123456789ABCDEF\nok\n");
    CHECK_EQ(b.pop().value_or(""), "ok");
    CHECK(!b.pop());
    b.append("0123456789");
    CHECK(!b.take_remainder());
}

TEST(unique_fd_moves_and_closes) {
    int fds[2];
    REQUIRE(::pipe(fds) == 0);
    UniqueFd a(fds[0]);
    UniqueFd w(fds[1]);
    UniqueFd b(std::move(a));
    CHECK(!a.valid());  // NOLINT(bugprone-use-after-move)
    CHECK(b.valid());
    UniqueFd c;
    c = std::move(b);
    CHECK_EQ(c.get(), fds[0]);
    {
        UniqueFd scoped(std::move(c));
    }
    CHECK(::fcntl(fds[0], F_GETFD) == -1);  // Closed by destructor.
}

TEST(file_source_reads_lines) {
    const std::string path = "io_test_input.nmea";
    {
        std::ofstream f(path);
        f << "$A\r\n\n$B";
    }
    FileSource s(path);
    CHECK_EQ(s.next_line().value_or("?"), "$A\r");
    CHECK_EQ(s.next_line().value_or("?"), "");
    CHECK_EQ(s.next_line().value_or("?"), "$B");
    CHECK(!s.next_line());
    CHECK_EQ(s.describe(), "file " + path);
    std::remove(path.c_str());
    CHECK_THROWS(FileSource("/nonexistent/dir/file.nmea"), std::runtime_error);
}

TEST(baud_rates) {
    CHECK(baud_to_speed(4800).has_value());
    CHECK(baud_to_speed(9600).has_value());
    CHECK(baud_to_speed(115200).has_value());
    CHECK(!baud_to_speed(1234).has_value());
}

TEST(serial_source_over_pseudo_terminal) {
    // A pty stands in for a USB GNSS receiver so termios setup and reads are exercised.
    const int master = ::posix_openpt(O_RDWR | O_NOCTTY);
    REQUIRE(master >= 0);
    UniqueFd master_fd(master);
    REQUIRE(::grantpt(master) == 0);
    REQUIRE(::unlockpt(master) == 0);
    const char* name = ::ptsname(master);
    REQUIRE(name != nullptr);
    const std::string slave = name;

    std::atomic<bool> stop{false};
    SerialSource serial(slave, 9600, stop);
    CHECK(serial.describe().find("9600") != std::string::npos);

    const std::string data = make_sentence("GPGGA,083000.00,,,,,0,00,99.9,,M,,M,,") + "\r\n" +
                             make_sentence("GPRMC,083000.00,V,,,,,,,140526,,,N") + "\r\n";
    REQUIRE(::write(master, data.data(), data.size()) == static_cast<ssize_t>(data.size()));
    CHECK(serial.next_line().value_or("").rfind("$GPGGA", 0) == 0);
    CHECK(serial.next_line().value_or("").rfind("$GPRMC", 0) == 0);

    std::thread stopper([&stop] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        stop.store(true);
    });
    CHECK(!serial.next_line().has_value());  // Returns once stop is raised.
    stopper.join();

    CHECK_THROWS(SerialSource(slave, 1234, stop), std::invalid_argument);
    CHECK_THROWS(SerialSource("/dev/does-not-exist", 9600, stop), std::system_error);
}

TEST(udp_source_receives_datagrams) {
    std::atomic<bool> stop{false};
    UdpSource udp("127.0.0.1", 0, stop);
    REQUIRE(udp.port() != 0);
    CHECK(udp.describe().find("127.0.0.1") != std::string::npos);

    UniqueFd sender(::socket(AF_INET, SOCK_DGRAM, 0));
    REQUIRE(sender.valid());
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(udp.port());
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const auto send = [&](const std::string& msg) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto n = ::sendto(sender.get(), msg.data(), msg.size(), 0, reinterpret_cast<const sockaddr*>(&to),
                                sizeof to);
        REQUIRE(n == static_cast<ssize_t>(msg.size()));
    };
    send("$ONE*00\r\n$TWO*00\r\n");
    send("$THREE*00");  // Phone apps often omit the final newline.
    CHECK_EQ(udp.next_line().value_or(""), "$ONE*00");
    CHECK_EQ(udp.next_line().value_or(""), "$TWO*00");
    CHECK_EQ(udp.next_line().value_or(""), "$THREE*00");

    stop.store(true);
    CHECK(!udp.next_line().has_value());
    CHECK_THROWS(UdpSource("not-an-ip", 0, stop), std::invalid_argument);
}
