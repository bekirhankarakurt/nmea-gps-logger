#pragma once

#include "nmea/geofence.hpp"
#include "nmea/parser.hpp"
#include "nmea/stats.hpp"
#include "nmea/track.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace nmea {

struct PipelineConfig {
    ParseOptions parse;
    DistanceMethod distance{DistanceMethod::Haversine};
    std::optional<double> max_hdop;  ///< Drop fixes with worse HDOP.
    int fence_confirm_fixes{1};
    bool keep_points{true};          ///< Retain points for export.
};

/// Counters describing input quality.
struct InputCounters {
    std::size_t lines{};
    std::size_t accepted{};
    std::size_t fixes{};
    std::size_t filtered{};
    std::array<std::size_t, 10> errors{};  ///< Indexed by ParseError.

    [[nodiscard]] std::size_t error_count(ParseError error) const noexcept;
};

/// What processing one line produced.
struct LineOutcome {
    std::optional<Message> message;
    std::optional<ParseError> error;
    std::vector<TrackPoint> points;   ///< Usually zero or one.
    std::vector<FenceEvent> events;
};

/// Hardware-independent processing chain: parse -> assemble -> filter ->
/// statistics -> geofences. Sources and outputs live outside so the whole
/// chain runs identically on live and replayed data.
class Pipeline {
public:
    explicit Pipeline(PipelineConfig config = {});

    void add_fence(Geofence fence);
    [[nodiscard]] LineOutcome process_line(std::string_view line);
    /// Closes the last epoch at end of input.
    [[nodiscard]] LineOutcome finish();

    [[nodiscard]] const std::vector<TrackPoint>& points() const noexcept { return points_; }
    [[nodiscard]] const std::vector<FenceEvent>& events() const noexcept { return events_; }
    [[nodiscard]] const std::vector<Geofence>& fences() const noexcept { return monitor_.fences(); }
    [[nodiscard]] const TrackStats& stats() const noexcept { return stats_.stats(); }
    [[nodiscard]] const InputCounters& counters() const noexcept { return counters_; }

private:
    void accept(const TrackPoint& point, LineOutcome& outcome);

    PipelineConfig config_;
    FixAssembler assembler_;
    StatsAccumulator stats_;
    GeofenceMonitor monitor_;
    InputCounters counters_;
    std::vector<TrackPoint> points_;
    std::vector<FenceEvent> events_;
};

}  // namespace nmea
