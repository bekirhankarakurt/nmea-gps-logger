#include "nmea/track.hpp"

#include <cmath>
#include <type_traits>

namespace nmea {

namespace {
constexpr double kSameEpochTolerance = 1e-3;
constexpr double kHalfDay = kSecondsPerDay / 2.0;
}  // namespace

std::optional<TrackPoint> FixAssembler::begin_epoch(double tod) {
    std::optional<TrackPoint> closed;
    if (epoch_.tod && std::fabs(*epoch_.tod - tod) <= kSameEpochTolerance) {
        return closed;
    }
    if (epoch_.any) {
        closed = close_epoch();
    }
    epoch_ = Epoch{};
    epoch_.tod = tod;
    return closed;
}

std::optional<TrackPoint> FixAssembler::feed(const Message& message) {
    std::optional<TrackPoint> closed;
    std::visit(
        [&](const auto& m) {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, Gga>) {
                if (m.time) {
                    closed = begin_epoch(m.time->seconds_of_day());
                }
                epoch_.has_gga = true;
                epoch_.gga_quality = m.quality;
                epoch_.gga_position = m.position;
                epoch_.altitude_m = m.altitude_m;
                epoch_.gga_hdop = m.hdop;
                epoch_.satellites = m.satellites;
            } else if constexpr (std::is_same_v<T, Rmc>) {
                if (m.time) {
                    closed = begin_epoch(m.time->seconds_of_day());
                    if (m.date) {
                        date_ = m.date;
                        date_tod_ = m.time->seconds_of_day();
                    }
                }
                epoch_.rmc_valid = m.valid;
                epoch_.rmc_mode = m.mode;
                epoch_.rmc_position = m.position;
                if (m.speed_knots) {
                    epoch_.speed_mps = *m.speed_knots * kKnotsToMps;
                }
                if (m.course_deg) {
                    epoch_.course_deg = m.course_deg;
                }
            } else if constexpr (std::is_same_v<T, Gsa>) {
                if (m.hdop) {
                    epoch_.gsa_hdop = m.hdop;
                }
            } else if constexpr (std::is_same_v<T, Vtg>) {
                if (!epoch_.speed_mps) {
                    if (m.speed_kmh) {
                        epoch_.speed_mps = *m.speed_kmh * kKmhToMps;
                    } else if (m.speed_knots) {
                        epoch_.speed_mps = *m.speed_knots * kKnotsToMps;
                    }
                }
                if (!epoch_.course_deg && m.course_true_deg) {
                    epoch_.course_deg = m.course_true_deg;
                }
            }
            // GSV carries no fix information.
            if constexpr (!std::is_same_v<T, Gsv>) {
                epoch_.any = true;
            }
        },
        message);
    return closed;
}

std::optional<TrackPoint> FixAssembler::flush() {
    std::optional<TrackPoint> closed;
    if (epoch_.any) {
        closed = close_epoch();
    }
    epoch_ = Epoch{};
    return closed;
}

std::optional<TrackPoint> FixAssembler::close_epoch() {
    const Epoch e = epoch_;
    epoch_.any = false;
    if (!e.tod) {
        return std::nullopt;
    }
    const bool gga_ok = e.has_gga && e.gga_quality != FixQuality::Invalid && e.gga_position;
    const bool rmc_ok = e.rmc_valid && e.rmc_position;
    if (!gga_ok && !rmc_ok) {
        return std::nullopt;
    }

    TrackPoint p;
    p.position = gga_ok ? *e.gga_position : *e.rmc_position;
    if (gga_ok) {
        p.quality = e.gga_quality;
    } else {
        p.quality = e.rmc_mode == FaaMode::Differential ? FixQuality::Dgps : FixQuality::Gps;
    }
    p.altitude_m = e.altitude_m;
    p.speed_mps = e.speed_mps;
    p.course_deg = e.course_deg;
    p.hdop = e.gga_hdop ? e.gga_hdop : e.gsa_hdop;
    p.satellites = e.satellites;

    if (date_ && date_tod_) {
        double unix = static_cast<double>(days_from_civil(date_->year, date_->month, date_->day)) *
                          kSecondsPerDay +
                      *e.tod;
        // A GGA-only stream may cross midnight after the last RMC set the date.
        if (*e.tod + kHalfDay < *date_tod_) {
            unix += kSecondsPerDay;
        }
        if (last_unix_ && unix + kHalfDay < *last_unix_) {
            unix += kSecondsPerDay;
        }
        p.time_s = unix;
        p.has_date = true;
        last_unix_ = unix;
    } else {
        p.time_s = *e.tod;
        p.has_date = false;
    }
    return p;
}

}  // namespace nmea
