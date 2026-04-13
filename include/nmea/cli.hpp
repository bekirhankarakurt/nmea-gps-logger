#pragma once

#include "nmea/geofence.hpp"
#include "nmea/pipeline.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nmea {

enum class InputKind { None, File, Serial, Udp };

/// Fully validated command-line configuration.
struct CliOptions {
    InputKind input{InputKind::None};
    std::string file_path;
    std::string serial_device;
    int baud{9600};
    std::uint16_t udp_port{10110};
    std::string udp_bind{"0.0.0.0"};
    double replay_speed{0.0};          ///< 0 = no pacing.
    std::string gpx_path;
    std::string geojson_path;
    std::string tee_path;
    std::string track_name{"nmea-gps-logger track"};
    std::optional<double> duration_s;
    PipelineConfig pipeline;
    std::vector<Geofence> fences;
    bool quiet{false};
    bool verbose{false};
    bool json_lines{false};
    bool show_help{false};
    bool show_version{false};
};

/// Either parsed options or a usage error message.
struct CliParseResult {
    std::optional<CliOptions> options;
    std::string error;
};

[[nodiscard]] CliParseResult parse_cli(const std::vector<std::string>& args);

/// "NAME:LAT,LON,RADIUS_M"
[[nodiscard]] std::optional<Geofence> parse_circle_spec(const std::string& spec);
/// "NAME:LAT,LON;LAT,LON;LAT,LON[;...]"
[[nodiscard]] std::optional<Geofence> parse_polygon_spec(const std::string& spec);

[[nodiscard]] std::string usage_text();

}  // namespace nmea
