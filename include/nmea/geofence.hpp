#pragma once

#include "nmea/geo.hpp"
#include "nmea/track.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace nmea {

enum class FenceShape { Circle, Polygon };

/// A named circular or polygonal region.
class Geofence {
public:
    /// Circle of @p radius_m metres around @p center. Throws std::invalid_argument if radius <= 0.
    [[nodiscard]] static Geofence circle(std::string name, GeoPoint center, double radius_m);

    /// Simple polygon (no self-intersections), implicitly closed. Throws
    /// std::invalid_argument for fewer than three distinct vertices.
    [[nodiscard]] static Geofence polygon(std::string name, std::vector<GeoPoint> vertices);

    /// True if @p point lies inside (or on the boundary of a circle).
    [[nodiscard]] bool contains(GeoPoint point) const noexcept;

    /// Closed ring describing the boundary; circles are approximated with @p segments vertices.
    [[nodiscard]] std::vector<GeoPoint> outline(std::size_t segments = 64) const;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] FenceShape shape() const noexcept { return shape_; }
    [[nodiscard]] GeoPoint center() const noexcept { return center_; }
    [[nodiscard]] double radius_m() const noexcept { return radius_m_; }
    [[nodiscard]] const std::vector<GeoPoint>& vertices() const noexcept { return vertices_; }

private:
    Geofence(std::string name, FenceShape shape, GeoPoint center, double radius_m,
             std::vector<GeoPoint> vertices);

    std::string name_;
    FenceShape shape_;
    GeoPoint center_;
    double radius_m_;
    std::vector<GeoPoint> vertices_;
};

enum class FenceTransition { Enter, Exit };

[[nodiscard]] std::string_view to_string(FenceTransition transition) noexcept;

/// An enter/exit transition detected by the monitor.
struct FenceEvent {
    std::string fence;
    FenceTransition transition{FenceTransition::Enter};
    TrackPoint point;
};

/// Tracks inside/outside state per fence and reports transitions.
///
/// The first fix inside a fence produces Enter; the first fix outside sets
/// state silently. A transition is only reported after @p confirm_fixes
/// consecutive fixes agree, which suppresses boundary jitter.
class GeofenceMonitor {
public:
    explicit GeofenceMonitor(int confirm_fixes = 1);

    void add(Geofence fence);
    [[nodiscard]] std::vector<FenceEvent> update(const TrackPoint& point);
    [[nodiscard]] const std::vector<Geofence>& fences() const noexcept { return fences_; }
    /// Current state: nullopt before the first fix.
    [[nodiscard]] std::optional<bool> is_inside(std::size_t index) const;

private:
    struct State {
        std::optional<bool> inside;
        int pending{};  ///< Consecutive fixes contradicting `inside`.
    };

    int confirm_fixes_;
    std::vector<Geofence> fences_;
    std::vector<State> states_;
};

}  // namespace nmea
