#include "nmea/cli.hpp"

#include "nmea/io.hpp"

#include <charconv>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace nmea {

namespace {

[[nodiscard]] std::optional<double> parse_double(std::string_view text) {
    double v = 0.0;
    const char* const end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, v);
    if (text.empty() || ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return v;
}

[[nodiscard]] std::optional<long> parse_long(std::string_view text) {
    long v = 0;
    const char* const end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, v);
    if (text.empty() || ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return v;
}

[[nodiscard]] std::vector<std::string_view> split(std::string_view text, char delim) {
    std::vector<std::string_view> parts;
    while (true) {
        const auto pos = text.find(delim);
        parts.push_back(text.substr(0, pos));
        if (pos == std::string_view::npos) {
            return parts;
        }
        text.remove_prefix(pos + 1);
    }
}

[[nodiscard]] std::optional<GeoPoint> parse_point(std::string_view text) {
    const auto parts = split(text, ',');
    if (parts.size() != 2) {
        return std::nullopt;
    }
    const auto lat = parse_double(parts[0]);
    const auto lon = parse_double(parts[1]);
    if (!lat || !lon || *lat < -90.0 || *lat > 90.0 || *lon < -180.0 || *lon > 180.0) {
        return std::nullopt;
    }
    return GeoPoint{*lat, *lon};
}

/// Splits "NAME:REST"; the name must be non-empty.
[[nodiscard]] std::optional<std::pair<std::string, std::string_view>> split_name(const std::string& spec) {
    const auto colon = spec.find(':');
    if (colon == std::string::npos || colon == 0) {
        return std::nullopt;
    }
    return std::make_pair(spec.substr(0, colon), std::string_view(spec).substr(colon + 1));
}

}  // namespace

std::optional<Geofence> parse_circle_spec(const std::string& spec) {
    const auto named = split_name(spec);
    if (!named) {
        return std::nullopt;
    }
    const auto parts = split(named->second, ',');
    if (parts.size() != 3) {
        return std::nullopt;
    }
    const auto lat = parse_double(parts[0]);
    const auto lon = parse_double(parts[1]);
    const auto radius = parse_double(parts[2]);
    if (!lat || !lon || !radius || *radius <= 0.0 || *lat < -90.0 || *lat > 90.0 || *lon < -180.0 ||
        *lon > 180.0) {
        return std::nullopt;
    }
    return Geofence::circle(named->first, GeoPoint{*lat, *lon}, *radius);
}

std::optional<Geofence> parse_polygon_spec(const std::string& spec) {
    const auto named = split_name(spec);
    if (!named) {
        return std::nullopt;
    }
    std::vector<GeoPoint> vertices;
    for (const auto part : split(named->second, ';')) {
        const auto p = parse_point(part);
        if (!p) {
            return std::nullopt;
        }
        vertices.push_back(*p);
    }
    try {
        return Geofence::polygon(named->first, std::move(vertices));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

std::string usage_text() {
    return R"(Usage: nmea-logger INPUT [OPTIONS]

Input (exactly one):
  -f, --file PATH            Read NMEA from a log file ('-' = stdin)
  -s, --serial DEVICE        Read from a serial GNSS receiver (e.g. /dev/ttyUSB0)
  -b, --baud RATE            Serial baud rate (default 9600)
  -u, --udp PORT             Listen for NMEA datagrams on a UDP port
      --bind ADDRESS         UDP bind address (default 0.0.0.0)

Replay:
  -r, --replay-speed X       Pace file input by NMEA timestamps, X times real time

Output:
      --gpx PATH             Write the track as GPX 1.1
      --geojson PATH         Write track, fences and events as GeoJSON
      --tee PATH             Append every valid raw sentence to PATH (record for replay)
      --name TEXT            Track name used in exports
      --json                 Print fixes and events as JSON lines
  -q, --quiet                Print only events and the summary
  -v, --verbose              Report rejected sentences on stderr

Processing:
      --max-hdop X           Drop fixes with HDOP above X
      --vincenty             Use Vincenty (WGS-84) instead of haversine distances
      --no-checksum          Accept sentences without a '*hh' checksum
      --duration SECONDS     Stop after this many seconds (live inputs)

Geofences (repeatable):
      --fence-circle NAME:LAT,LON,RADIUS_M
      --fence-polygon NAME:LAT,LON;LAT,LON;LAT,LON[;...]
      --fence-confirm N      Fixes required to confirm a transition (default 1)

  -h, --help                 Show this help
  -V, --version              Show version

Exit status: 0 success, 1 runtime error, 2 usage error.
)";
}

CliParseResult parse_cli(const std::vector<std::string>& args) {
    CliOptions o;
    CliParseResult result;
    int inputs = 0;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        const auto value = [&]() -> std::optional<std::string> {
            if (i + 1 >= args.size()) {
                return std::nullopt;
            }
            return args[++i];
        };
        const auto fail = [&](const std::string& message) {
            result.error = message;
            return result;
        };
        const auto needs = [&](const std::string& name) { return "option " + name + " requires a value"; };

        if (a == "-h" || a == "--help") {
            o.show_help = true;
        } else if (a == "-V" || a == "--version") {
            o.show_version = true;
        } else if (a == "-f" || a == "--file") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            o.input = InputKind::File;
            o.file_path = *v;
            ++inputs;
        } else if (a == "-s" || a == "--serial") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            o.input = InputKind::Serial;
            o.serial_device = *v;
            ++inputs;
        } else if (a == "-b" || a == "--baud") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            const auto n = parse_long(*v);
            if (!n || *n > std::numeric_limits<int>::max() || !baud_to_speed(static_cast<int>(*n))) {
                return fail("unsupported baud rate: " + *v);
            }
            o.baud = static_cast<int>(*n);
        } else if (a == "-u" || a == "--udp") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            const auto n = parse_long(*v);
            if (!n || *n < 1 || *n > 65535) {
                return fail("invalid UDP port: " + *v);
            }
            o.input = InputKind::Udp;
            o.udp_port = static_cast<std::uint16_t>(*n);
            ++inputs;
        } else if (a == "--bind") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            o.udp_bind = *v;
        } else if (a == "-r" || a == "--replay-speed") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            const auto x = parse_double(*v);
            if (!x || *x <= 0.0) {
                return fail("replay speed must be a positive number: " + *v);
            }
            o.replay_speed = *x;
        } else if (a == "--gpx") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            o.gpx_path = *v;
        } else if (a == "--geojson") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            o.geojson_path = *v;
        } else if (a == "--tee") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            o.tee_path = *v;
        } else if (a == "--name") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            o.track_name = *v;
        } else if (a == "--json") {
            o.json_lines = true;
        } else if (a == "-q" || a == "--quiet") {
            o.quiet = true;
        } else if (a == "-v" || a == "--verbose") {
            o.verbose = true;
        } else if (a == "--max-hdop") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            const auto x = parse_double(*v);
            if (!x || *x <= 0.0) {
                return fail("max HDOP must be a positive number: " + *v);
            }
            o.pipeline.max_hdop = x;
        } else if (a == "--vincenty") {
            o.pipeline.distance = DistanceMethod::Vincenty;
        } else if (a == "--no-checksum") {
            o.pipeline.parse.require_checksum = false;
        } else if (a == "--duration") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            const auto x = parse_double(*v);
            if (!x || *x <= 0.0) {
                return fail("duration must be a positive number: " + *v);
            }
            o.duration_s = x;
        } else if (a == "--fence-circle") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            auto fence = parse_circle_spec(*v);
            if (!fence) {
                return fail("invalid circle fence (NAME:LAT,LON,RADIUS_M): " + *v);
            }
            o.fences.push_back(std::move(*fence));
        } else if (a == "--fence-polygon") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            auto fence = parse_polygon_spec(*v);
            if (!fence) {
                return fail("invalid polygon fence (NAME:LAT,LON;LAT,LON;LAT,LON): " + *v);
            }
            o.fences.push_back(std::move(*fence));
        } else if (a == "--fence-confirm") {
            const auto v = value();
            if (!v) {
                return fail(needs(a));
            }
            const auto n = parse_long(*v);
            if (!n || *n < 1 || *n > 1000) {
                return fail("fence confirm count must be 1..1000: " + *v);
            }
            o.pipeline.fence_confirm_fixes = static_cast<int>(*n);
        } else {
            return fail("unknown option: " + a);
        }
    }

    if (!o.show_help && !o.show_version) {
        if (inputs == 0) {
            result.error = "no input given (use --file, --serial or --udp)";
            return result;
        }
        if (inputs > 1) {
            result.error = "only one input may be given";
            return result;
        }
        if (o.replay_speed > 0.0 && o.input != InputKind::File) {
            result.error = "--replay-speed applies to --file input only";
            return result;
        }
    }
    o.pipeline.keep_points = !o.gpx_path.empty() || !o.geojson_path.empty();
    result.options = std::move(o);
    return result;
}

}  // namespace nmea
