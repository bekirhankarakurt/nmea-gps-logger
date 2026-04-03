#pragma once

#include "nmea/geo.hpp"
#include "nmea/time.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace nmea {

/// GGA fix quality indicator.
enum class FixQuality {
    Invalid = 0,
    Gps = 1,
    Dgps = 2,
    Pps = 3,
    RtkFixed = 4,
    RtkFloat = 5,
    Estimated = 6,
    Manual = 7,
    Simulation = 8,
};

/// FAA mode indicator (NMEA 2.3+), trailing field of RMC/VTG.
enum class FaaMode {
    Autonomous,
    Differential,
    Estimated,
    Manual,
    Simulated,
    NotValid,
    Precise,
    RtkFixed,
    RtkFloat,
};

/// GSA fix type.
enum class GsaFixType {
    NoFix = 1,
    Fix2D = 2,
    Fix3D = 3,
};

/// GGA - Global Positioning System Fix Data.
struct Gga {
    std::string talker;
    std::optional<UtcTime> time;
    std::optional<GeoPoint> position;
    FixQuality quality{FixQuality::Invalid};
    std::optional<int> satellites;
    std::optional<double> hdop;
    std::optional<double> altitude_m;          ///< Above mean sea level.
    std::optional<double> geoid_separation_m;
    std::optional<double> dgps_age_s;
    std::optional<int> dgps_station;
};

/// RMC - Recommended Minimum Specific GNSS Data.
struct Rmc {
    std::string talker;
    std::optional<UtcTime> time;
    bool valid{false};                          ///< Status 'A' (active) vs 'V' (void).
    std::optional<GeoPoint> position;
    std::optional<double> speed_knots;
    std::optional<double> course_deg;           ///< True course over ground.
    std::optional<Date> date;
    std::optional<double> magnetic_variation_deg;  ///< East positive.
    std::optional<FaaMode> mode;
};

/// GSA - GNSS DOP and Active Satellites.
struct Gsa {
    std::string talker;
    bool automatic{true};                       ///< 'A' automatic vs 'M' manual 2D/3D selection.
    GsaFixType fix_type{GsaFixType::NoFix};
    std::vector<int> satellites;                ///< PRNs used in the solution.
    std::optional<double> pdop;
    std::optional<double> hdop;
    std::optional<double> vdop;
    std::optional<int> system_id;               ///< NMEA 4.1 GNSS system ID.
};

/// One satellite entry inside a GSV sentence.
struct SatelliteInfo {
    int prn{};
    std::optional<int> elevation_deg;
    std::optional<int> azimuth_deg;
    std::optional<int> snr_db;
};

/// GSV - GNSS Satellites in View (one sentence of a multi-sentence group).
struct Gsv {
    std::string talker;
    int total_messages{};
    int message_number{};
    int satellites_in_view{};
    std::vector<SatelliteInfo> satellites;
    std::optional<int> signal_id;               ///< NMEA 4.1 signal ID.
};

/// VTG - Course Over Ground and Ground Speed.
struct Vtg {
    std::string talker;
    std::optional<double> course_true_deg;
    std::optional<double> course_magnetic_deg;
    std::optional<double> speed_knots;
    std::optional<double> speed_kmh;
    std::optional<FaaMode> mode;
};

using Message = std::variant<Gga, Rmc, Gsa, Gsv, Vtg>;

/// Short name of a fix quality, e.g. "GPS", "DGPS", "RTK".
[[nodiscard]] std::string_view to_string(FixQuality quality) noexcept;

/// Sentence formatter ("GGA", "RMC", ...) of a parsed message.
[[nodiscard]] std::string_view message_type(const Message& message) noexcept;

}  // namespace nmea
