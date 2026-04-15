#pragma once

#include <chrono>
#include <optional>

namespace nmea {

/// Computes wall-clock delays that reproduce the original timing of a
/// recorded stream, scaled by a speed factor (2.0 = twice as fast).
class ReplayPacer {
public:
    using Clock = std::chrono::steady_clock;

    /// Throws std::invalid_argument unless speed > 0.
    explicit ReplayPacer(double speed);

    /// Delay before releasing a sentence stamped @p seconds_of_day, given the
    /// current time. Midnight wrap is handled; a backwards jump re-anchors.
    [[nodiscard]] Clock::duration delay_for(double seconds_of_day, Clock::time_point now);

private:
    double speed_;
    std::optional<double> anchor_tod_;
    double last_offset_{};
    Clock::time_point anchor_wall_;
};

}  // namespace nmea
