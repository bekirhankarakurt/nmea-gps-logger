#include "nmea/stats.hpp"

#include <algorithm>

namespace nmea {

StatsAccumulator::StatsAccumulator(DistanceMethod method) noexcept : method_(method) {}

void StatsAccumulator::add(const TrackPoint& point) {
    ++stats_.points;
    const GeoPoint pos = point.position;

    if (!stats_.bbox_min) {
        stats_.bbox_min = pos;
        stats_.bbox_max = pos;
    } else {
        stats_.bbox_min->lat_deg = std::min(stats_.bbox_min->lat_deg, pos.lat_deg);
        stats_.bbox_min->lon_deg = std::min(stats_.bbox_min->lon_deg, pos.lon_deg);
        stats_.bbox_max->lat_deg = std::max(stats_.bbox_max->lat_deg, pos.lat_deg);
        stats_.bbox_max->lon_deg = std::max(stats_.bbox_max->lon_deg, pos.lon_deg);
    }

    if (point.hdop) {
        hdop_sum_ += *point.hdop;
        ++hdop_count_;
        stats_.mean_hdop = hdop_sum_ / static_cast<double>(hdop_count_);
        stats_.max_hdop = std::max(stats_.max_hdop.value_or(*point.hdop), *point.hdop);
    }

    if (point.altitude_m) {
        const double alt = *point.altitude_m;
        stats_.min_altitude_m = std::min(stats_.min_altitude_m.value_or(alt), alt);
        stats_.max_altitude_m = std::max(stats_.max_altitude_m.value_or(alt), alt);
        if (last_ && last_->altitude_m && alt > *last_->altitude_m) {
            stats_.elevation_gain_m += alt - *last_->altitude_m;
        }
    }

    if (point.speed_mps) {
        stats_.max_speed_mps = std::max(stats_.max_speed_mps, *point.speed_mps);
    }

    if (!first_time_) {
        first_time_ = point.time_s;
    }

    if (last_) {
        const double segment = distance_m(last_->position, pos, method_);
        const double dt = point.time_s - last_->time_s;
        stats_.distance_m += segment;
        if (dt > 0.0) {
            const double derived = segment / dt;
            const double speed = point.speed_mps.value_or(derived);
            if (!point.speed_mps) {
                stats_.max_speed_mps = std::max(stats_.max_speed_mps, derived);
            }
            if (speed >= kMovingThresholdMps) {
                stats_.moving_time_s += dt;
            }
        }
        stats_.duration_s = std::max(0.0, point.time_s - *first_time_);
    }

    stats_.avg_speed_mps = stats_.duration_s > 0.0 ? stats_.distance_m / stats_.duration_s : 0.0;
    stats_.avg_moving_speed_mps =
        stats_.moving_time_s > 0.0 ? stats_.distance_m / stats_.moving_time_s : 0.0;
    last_ = point;
}

}  // namespace nmea
