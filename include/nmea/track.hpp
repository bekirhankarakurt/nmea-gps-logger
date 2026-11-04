#pragma once

#include "nmea/geo.hpp"
#include "nmea/messages.hpp"

#include <optional>

namespace nmea {

/// One position fix assembled from all sentences sharing a timestamp.
struct TrackPoint {
    double time_s{};      ///< Unix seconds when has_date, otherwise seconds of day.
    bool has_date{false};
    GeoPoint position;
    FixQuality quality{FixQuality::Invalid};
    std::optional<double> altitude_m;
    std::optional<double> speed_mps;
    std::optional<double> course_deg;
    std::optional<double> hdop;
    std::optional<int> satellites;
};

/// Merges GGA/RMC/GSA/VTG sentences of the same epoch into TrackPoints.
///
/// Receivers emit a burst of sentences per fix in no guaranteed order. The
/// assembler keys epochs by UTC time of day and closes an epoch when a timed
/// sentence (GGA/RMC) with a different time arrives, or on flush(). Untimed
/// sentences (GSA/VTG) enrich the currently open epoch. The date comes from
/// the most recent RMC and is rolled over at midnight for GGA-only streams.
class FixAssembler {
public:
    /// Feeds one message; returns a point when it closes a valid epoch.
    [[nodiscard]] std::optional<TrackPoint> feed(const Message& message);

    /// Closes the open epoch (call at end of input).
    [[nodiscard]] std::optional<TrackPoint> flush();

private:
    struct Epoch {
        std::optional<double> tod;
        std::optional<GeoPoint> gga_position;
        std::optional<GeoPoint> rmc_position;
        FixQuality gga_quality{FixQuality::Invalid};
        bool has_gga{false};
        bool rmc_valid{false};
        std::optional<FaaMode> rmc_mode;
        std::optional<double> altitude_m;
        std::optional<double> speed_mps;
        std::optional<double> course_deg;
        std::optional<double> gga_hdop;
        std::optional<double> gsa_hdop;
        std::optional<int> satellites;
        bool any{false};
    };

    [[nodiscard]] std::optional<TrackPoint> begin_epoch(double tod);
    [[nodiscard]] std::optional<TrackPoint> close_epoch();

    Epoch epoch_;
    std::optional<Date> date_;
    std::optional<double> date_tod_;    ///< Time of day of the RMC that set date_.
    std::optional<double> last_unix_;
};

}  // namespace nmea
