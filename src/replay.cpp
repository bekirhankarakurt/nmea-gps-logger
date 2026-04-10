#include "nmea/replay.hpp"

#include "nmea/time.hpp"

#include <stdexcept>

namespace nmea {

ReplayPacer::ReplayPacer(double speed) : speed_(speed) {
    if (!(speed > 0.0)) {
        throw std::invalid_argument("replay speed must be positive");
    }
}

ReplayPacer::Clock::duration ReplayPacer::delay_for(double seconds_of_day, Clock::time_point now) {
    if (!anchor_tod_) {
        anchor_tod_ = seconds_of_day;
        anchor_wall_ = now;
        last_offset_ = 0.0;
        return Clock::duration::zero();
    }
    double offset = seconds_of_day - *anchor_tod_;
    // Crossing midnight: keep counting forward from the anchor.
    while (offset + kSecondsPerDay / 2.0 < last_offset_) {
        offset += kSecondsPerDay;
    }
    if (offset < last_offset_ - 1.0) {
        // Timestamps went backwards (concatenated logs): restart pacing here.
        anchor_tod_ = seconds_of_day;
        anchor_wall_ = now;
        last_offset_ = 0.0;
        return Clock::duration::zero();
    }
    last_offset_ = offset;
    const auto target = anchor_wall_ + std::chrono::duration_cast<Clock::duration>(
                                           std::chrono::duration<double>(offset / speed_));
    return target > now ? target - now : Clock::duration::zero();
}

}  // namespace nmea
