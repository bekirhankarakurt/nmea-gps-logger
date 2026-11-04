#pragma once

#include "nmea/geo.hpp"
#include "nmea/track.hpp"

#include <cstddef>
#include <optional>

namespace nmea {

/// Aggregate statistics of a track.
struct TrackStats {
    std::size_t points{};
    double distance_m{};
    double duration_s{};
    double moving_time_s{};
    double max_speed_mps{};
    double avg_speed_mps{};         ///< distance / duration.
    double avg_moving_speed_mps{};  ///< distance / moving time.
    std::optional<double> mean_hdop;
    std::optional<double> max_hdop;
    std::optional<double> min_altitude_m;
    std::optional<double> max_altitude_m;
    double elevation_gain_m{};
    std::optional<GeoPoint> bbox_min;
    std::optional<GeoPoint> bbox_max;
};

/// Incrementally computes TrackStats; O(1) memory.
class StatsAccumulator {
public:
    /// Segments slower than this are counted as stationary (GNSS jitter).
    static constexpr double kMovingThresholdMps = 0.5;

    explicit StatsAccumulator(DistanceMethod method = DistanceMethod::Haversine) noexcept;

    void add(const TrackPoint& point);
    [[nodiscard]] const TrackStats& stats() const noexcept { return stats_; }

private:
    DistanceMethod method_;
    TrackStats stats_;
    std::optional<TrackPoint> last_;
    std::optional<double> first_time_;
    double hdop_sum_{};
    std::size_t hdop_count_{};
};

}  // namespace nmea
