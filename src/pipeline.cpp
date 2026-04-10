#include "nmea/pipeline.hpp"

#include <utility>

namespace nmea {

std::size_t InputCounters::error_count(ParseError error) const noexcept {
    const auto index = static_cast<std::size_t>(error);
    return index < errors.size() ? errors[index] : 0;  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

Pipeline::Pipeline(PipelineConfig config)
    : config_(config), stats_(config.distance), monitor_(config.fence_confirm_fixes) {}

void Pipeline::add_fence(Geofence fence) { monitor_.add(std::move(fence)); }

void Pipeline::accept(const TrackPoint& point, LineOutcome& outcome) {
    if (config_.max_hdop && point.hdop && *point.hdop > *config_.max_hdop) {
        ++counters_.filtered;
        return;
    }
    ++counters_.fixes;
    stats_.add(point);
    if (config_.keep_points) {
        points_.push_back(point);
    }
    for (auto& ev : monitor_.update(point)) {
        events_.push_back(ev);
        outcome.events.push_back(std::move(ev));
    }
    outcome.points.push_back(point);
}

LineOutcome Pipeline::process_line(std::string_view line) {
    LineOutcome outcome;
    ++counters_.lines;
    auto parsed = parse(line, config_.parse);
    if (!parsed) {
        const ParseError error = parsed.error();
        if (error == ParseError::Empty) {
            --counters_.lines;  // Blank lines are not input.
        } else {
            ++counters_.errors.at(static_cast<std::size_t>(error));
        }
        outcome.error = error;
        return outcome;
    }
    ++counters_.accepted;
    if (auto point = assembler_.feed(parsed.value())) {
        accept(*point, outcome);
    }
    outcome.message = std::move(parsed).value();
    return outcome;
}

LineOutcome Pipeline::finish() {
    LineOutcome outcome;
    if (auto point = assembler_.flush()) {
        accept(*point, outcome);
    }
    return outcome;
}

}  // namespace nmea
