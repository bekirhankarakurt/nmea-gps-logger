// nmea-logger: command-line front end for the nmea library.

#include "nmea/cli.hpp"
#include "nmea/export.hpp"
#include "nmea/io.hpp"
#include "nmea/pipeline.hpp"
#include "nmea/replay.hpp"
#include "nmea/time.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <thread>
#include <type_traits>
#include <vector>

#ifndef NMEA_LOGGER_VERSION
#define NMEA_LOGGER_VERSION "0.0.0"
#endif

namespace {

std::atomic<bool> g_stop{false};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void on_signal(int /*signal*/) { g_stop.store(true); }

void install_signal_handlers() {
    struct sigaction sa {};
    sa.sa_handler = on_signal;  // NOLINT(cppcoreguidelines-pro-type-union-access)
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // No SA_RESTART: blocking reads return EINTR on Ctrl-C.
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGALRM, &sa, nullptr);
}

void arm_timer(double seconds) {
    itimerval timer{};
    const double whole = std::floor(seconds);
    timer.it_value.tv_sec = static_cast<time_t>(whole);
    timer.it_value.tv_usec = static_cast<suseconds_t>((seconds - whole) * 1e6);
    setitimer(ITIMER_REAL, &timer, nullptr);
}

[[nodiscard]] std::string fmt(double value, int decimals) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os.setf(std::ios::fixed);
    os.precision(decimals);
    os << value;
    return os.str();
}

[[nodiscard]] std::string time_text(const nmea::TrackPoint& p) {
    return p.has_date ? nmea::format_iso8601(p.time_s) : nmea::format_time_of_day(p.time_s) + "Z";
}

void print_fix(const nmea::TrackPoint& p, bool json) {
    if (json) {
        std::cout << R"({"type":"fix","time":")" << time_text(p) << R"(","lat":)" << fmt(p.position.lat_deg, 7)
                  << R"(,"lon":)" << fmt(p.position.lon_deg, 7);
        if (p.altitude_m) {
            std::cout << R"(,"alt_m":)" << fmt(*p.altitude_m, 1);
        }
        if (p.speed_mps) {
            std::cout << R"(,"speed_kmh":)" << fmt(*p.speed_mps * 3.6, 2);
        }
        if (p.course_deg) {
            std::cout << R"(,"course_deg":)" << fmt(*p.course_deg, 1);
        }
        if (p.hdop) {
            std::cout << R"(,"hdop":)" << fmt(*p.hdop, 2);
        }
        if (p.satellites) {
            std::cout << R"(,"sats":)" << *p.satellites;
        }
        std::cout << R"(,"quality":")" << nmea::to_string(p.quality) << "\"}\n";
        return;
    }
    std::cout << time_text(p) << "  " << fmt(p.position.lat_deg, 6) << ' ' << fmt(p.position.lon_deg, 6);
    if (p.altitude_m) {
        std::cout << "  alt " << fmt(*p.altitude_m, 1) << " m";
    }
    if (p.speed_mps) {
        std::cout << "  " << fmt(*p.speed_mps * 3.6, 1) << " km/h";
    }
    if (p.course_deg) {
        std::cout << "  crs " << fmt(*p.course_deg, 0);
    }
    if (p.hdop) {
        std::cout << "  hdop " << fmt(*p.hdop, 1);
    }
    if (p.satellites) {
        std::cout << "  sats " << *p.satellites;
    }
    std::cout << "  " << nmea::to_string(p.quality) << '\n';
}

void print_event(const nmea::FenceEvent& ev, bool json) {
    if (json) {
        std::cout << R"({"type":"event","fence":")" << nmea::json_escape(ev.fence) << R"(","transition":")"
                  << nmea::to_string(ev.transition) << R"(","time":")" << time_text(ev.point) << R"(","lat":)"
                  << fmt(ev.point.position.lat_deg, 7) << R"(,"lon":)" << fmt(ev.point.position.lon_deg, 7)
                  << "}\n";
        return;
    }
    std::cout << "EVENT " << nmea::to_string(ev.transition) << ' ' << ev.fence << " at " << time_text(ev.point)
              << " (" << fmt(ev.point.position.lat_deg, 6) << ", " << fmt(ev.point.position.lon_deg, 6) << ")\n";
}

void print_summary(const nmea::Pipeline& pipeline, bool json) {
    const auto& s = pipeline.stats();
    const auto& c = pipeline.counters();
    const std::size_t rejected = c.lines - c.accepted;
    const std::size_t checksum = c.error_count(nmea::ParseError::ChecksumMismatch);
    const std::size_t unsupported = c.error_count(nmea::ParseError::UnsupportedType);
    if (json) {
        std::cout << R"({"type":"summary","lines":)" << c.lines << R"(,"accepted":)" << c.accepted
                  << R"(,"rejected":)" << rejected << R"(,"checksum_errors":)" << checksum
                  << R"(,"unsupported":)" << unsupported << R"(,"fixes":)" << c.fixes << R"(,"filtered":)"
                  << c.filtered << R"(,"distance_m":)" << fmt(s.distance_m, 1) << R"(,"duration_s":)"
                  << fmt(s.duration_s, 1) << R"(,"moving_time_s":)" << fmt(s.moving_time_s, 1)
                  << R"(,"max_speed_kmh":)" << fmt(s.max_speed_mps * 3.6, 2) << R"(,"avg_speed_kmh":)"
                  << fmt(s.avg_speed_mps * 3.6, 2);
        if (s.mean_hdop && s.max_hdop) {
            std::cout << R"(,"mean_hdop":)" << fmt(*s.mean_hdop, 2) << R"(,"max_hdop":)" << fmt(*s.max_hdop, 2);
        }
        std::cout << R"(,"events":)" << pipeline.events().size() << "}\n";
        return;
    }
    std::cout << "\n--- summary ---\n"
              << "sentences   " << c.accepted << " ok, " << rejected << " rejected (" << checksum
              << " checksum, " << unsupported << " unsupported)\n"
              << "fixes       " << c.fixes;
    if (c.filtered > 0) {
        std::cout << " (" << c.filtered << " dropped by HDOP filter)";
    }
    std::cout << "\ndistance    " << fmt(s.distance_m / 1000.0, 3) << " km\n"
              << "duration    " << nmea::format_duration(s.duration_s) << " (moving "
              << nmea::format_duration(s.moving_time_s) << ")\n"
              << "speed       max " << fmt(s.max_speed_mps * 3.6, 1) << " km/h, avg "
              << fmt(s.avg_speed_mps * 3.6, 1) << " km/h, moving avg " << fmt(s.avg_moving_speed_mps * 3.6, 1)
              << " km/h\n";
    if (s.mean_hdop && s.max_hdop) {
        std::cout << "hdop        mean " << fmt(*s.mean_hdop, 2) << ", max " << fmt(*s.max_hdop, 2) << '\n';
    }
    if (s.min_altitude_m && s.max_altitude_m) {
        std::cout << "altitude    " << fmt(*s.min_altitude_m, 1) << " .. " << fmt(*s.max_altitude_m, 1)
                  << " m, gain " << fmt(s.elevation_gain_m, 1) << " m\n";
    }
    if (!pipeline.fences().empty()) {
        std::cout << "geofences   " << pipeline.fences().size() << ", " << pipeline.events().size() << " events\n";
    }
}

/// Writes via a temporary file and rename so readers never see a partial export.
template <typename Writer>
void write_atomically(const std::string& path, Writer&& writer) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            throw std::runtime_error("cannot write " + tmp);
        }
        out.imbue(std::locale::classic());
        std::forward<Writer>(writer)(out);
        out.flush();
        if (!out) {
            throw std::runtime_error("write failed: " + tmp);
        }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        throw std::runtime_error("cannot rename " + tmp + " to " + path);
    }
}

[[nodiscard]] std::optional<double> timestamp_of(const nmea::Message& message) {
    return std::visit(
        [](const auto& m) -> std::optional<double> {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, nmea::Gga> || std::is_same_v<T, nmea::Rmc>) {
                if (m.time) {
                    return m.time->seconds_of_day();
                }
            }
            return std::nullopt;
        },
        message);
}

void sleep_interruptible(nmea::ReplayPacer::Clock::duration delay) {
    const auto until = nmea::ReplayPacer::Clock::now() + delay;
    while (!g_stop.load()) {
        const auto now = nmea::ReplayPacer::Clock::now();
        if (now >= until) {
            return;
        }
        std::this_thread::sleep_for(std::min<nmea::ReplayPacer::Clock::duration>(until - now,
                                                                                std::chrono::milliseconds(100)));
    }
}

[[nodiscard]] std::unique_ptr<nmea::LineSource> open_source(const nmea::CliOptions& o) {
    switch (o.input) {
        case nmea::InputKind::File: return std::make_unique<nmea::FileSource>(o.file_path);
        case nmea::InputKind::Serial: return std::make_unique<nmea::SerialSource>(o.serial_device, o.baud, g_stop);
        case nmea::InputKind::Udp: return std::make_unique<nmea::UdpSource>(o.udp_bind, o.udp_port, g_stop);
        case nmea::InputKind::None: break;
    }
    throw std::logic_error("no input selected");
}

int run(const nmea::CliOptions& o) {
    nmea::Pipeline pipeline(o.pipeline);
    for (const auto& fence : o.fences) {
        pipeline.add_fence(fence);
    }

    std::ofstream tee;
    if (!o.tee_path.empty()) {
        tee.open(o.tee_path, std::ios::app);
        if (!tee) {
            throw std::runtime_error("cannot open tee file: " + o.tee_path);
        }
    }

    const auto source = open_source(o);
    std::cerr << "nmea-logger: reading " << source->describe() << '\n';
    std::optional<nmea::ReplayPacer> pacer;
    if (o.replay_speed > 0.0) {
        pacer.emplace(o.replay_speed);
    }
    if (o.duration_s) {
        arm_timer(*o.duration_s);
    }

    const auto report = [&](const nmea::LineOutcome& outcome) {
        if (!o.quiet) {
            for (const auto& p : outcome.points) {
                print_fix(p, o.json_lines);
            }
        }
        for (const auto& ev : outcome.events) {
            print_event(ev, o.json_lines);
        }
        if (!outcome.points.empty() || !outcome.events.empty()) {
            std::cout.flush();
        }
    };

    while (!g_stop.load()) {
        const auto line = source->next_line();
        if (!line) {
            break;
        }
        auto outcome = pipeline.process_line(*line);
        if (outcome.error && o.verbose && *outcome.error != nmea::ParseError::Empty) {
            std::cerr << "rejected (" << nmea::to_string(*outcome.error) << "): " << *line << '\n';
        }
        if (tee.is_open() && (outcome.message || outcome.error == nmea::ParseError::UnsupportedType)) {
            tee << *line << '\n';
            tee.flush();
        }
        if (pacer && outcome.message) {
            if (const auto tod = timestamp_of(*outcome.message)) {
                sleep_interruptible(pacer->delay_for(*tod, nmea::ReplayPacer::Clock::now()));
            }
        }
        report(outcome);
    }
    report(pipeline.finish());

    if (!o.gpx_path.empty()) {
        write_atomically(o.gpx_path, [&](std::ostream& out) { nmea::write_gpx(out, pipeline.points(), o.track_name); });
        std::cerr << "nmea-logger: wrote " << o.gpx_path << '\n';
    }
    if (!o.geojson_path.empty()) {
        write_atomically(o.geojson_path, [&](std::ostream& out) {
            nmea::write_geojson(out, pipeline.points(), o.track_name, pipeline.fences(), pipeline.events());
        });
        std::cerr << "nmea-logger: wrote " << o.geojson_path << '\n';
    }
    print_summary(pipeline, o.json_lines);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::cout.imbue(std::locale::classic());
    const std::vector<std::string> args(argv + 1, argv + argc);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto parsed = nmea::parse_cli(args);
    if (!parsed.options) {
        std::cerr << "nmea-logger: " << parsed.error << "\nTry 'nmea-logger --help'.\n";
        return 2;
    }
    const auto& options = *parsed.options;
    if (options.show_help) {
        std::cout << nmea::usage_text();
        return 0;
    }
    if (options.show_version) {
        std::cout << "nmea-logger " << NMEA_LOGGER_VERSION << '\n';
        return 0;
    }
    install_signal_handlers();
    try {
        return run(options);
    } catch (const std::exception& e) {
        std::cerr << "nmea-logger: error: " << e.what() << '\n';
        return 1;
    }
}
