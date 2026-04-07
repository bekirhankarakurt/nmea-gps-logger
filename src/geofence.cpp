#include "nmea/geofence.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace nmea {

std::string_view to_string(FenceTransition transition) noexcept {
    return transition == FenceTransition::Enter ? "ENTER" : "EXIT";
}

Geofence::Geofence(std::string name, FenceShape shape, GeoPoint center, double radius_m,
                   std::vector<GeoPoint> vertices)
    : name_(std::move(name)),
      shape_(shape),
      center_(center),
      radius_m_(radius_m),
      vertices_(std::move(vertices)) {}

Geofence Geofence::circle(std::string name, GeoPoint center, double radius_m) {
    if (!(radius_m > 0.0)) {
        throw std::invalid_argument("geofence radius must be positive");
    }
    return Geofence(std::move(name), FenceShape::Circle, center, radius_m, {});
}

Geofence Geofence::polygon(std::string name, std::vector<GeoPoint> vertices) {
    // Drop an explicit closing vertex; the ring is implicitly closed.
    if (vertices.size() > 1 && vertices.front().lat_deg == vertices.back().lat_deg &&
        vertices.front().lon_deg == vertices.back().lon_deg) {
        vertices.pop_back();
    }
    if (vertices.size() < 3) {
        throw std::invalid_argument("geofence polygon needs at least three vertices");
    }
    GeoPoint centroid;
    for (const auto& v : vertices) {
        centroid.lat_deg += v.lat_deg;
        centroid.lon_deg += v.lon_deg;
    }
    const auto n = static_cast<double>(vertices.size());
    centroid.lat_deg /= n;
    centroid.lon_deg /= n;
    return Geofence(std::move(name), FenceShape::Polygon, centroid, 0.0, std::move(vertices));
}

namespace {

/// Longitude difference wrapped into [-180, 180) so fences may straddle the antimeridian.
[[nodiscard]] double wrap_lon(double dlon) noexcept {
    return std::fmod(dlon + 540.0, 360.0) - 180.0;
}

}  // namespace

bool Geofence::contains(GeoPoint point) const noexcept {
    if (shape_ == FenceShape::Circle) {
        return haversine_m(center_, point) <= radius_m_;
    }
    // Ray casting in a local equirectangular frame centred on the query point.
    const double k = std::cos(deg_to_rad(point.lat_deg));
    bool inside = false;
    const std::size_t n = vertices_.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = wrap_lon(vertices_[i].lon_deg - point.lon_deg) * k;
        const double yi = vertices_[i].lat_deg - point.lat_deg;
        const double xj = wrap_lon(vertices_[j].lon_deg - point.lon_deg) * k;
        const double yj = vertices_[j].lat_deg - point.lat_deg;
        if ((yi > 0.0) != (yj > 0.0)) {
            const double x_cross = xi + (0.0 - yi) * (xj - xi) / (yj - yi);
            if (x_cross > 0.0) {
                inside = !inside;
            }
        }
    }
    return inside;
}

std::vector<GeoPoint> Geofence::outline(std::size_t segments) const {
    std::vector<GeoPoint> ring;
    if (shape_ == FenceShape::Polygon) {
        ring = vertices_;
    } else {
        const std::size_t n = segments < 8 ? 8 : segments;
        ring.reserve(n + 1);
        for (std::size_t i = 0; i < n; ++i) {
            const double bearing = 360.0 * static_cast<double>(i) / static_cast<double>(n);
            ring.push_back(destination(center_, bearing, radius_m_));
        }
    }
    ring.push_back(ring.front());
    return ring;
}

GeofenceMonitor::GeofenceMonitor(int confirm_fixes) : confirm_fixes_(confirm_fixes) {
    if (confirm_fixes < 1) {
        throw std::invalid_argument("confirm_fixes must be at least 1");
    }
}

void GeofenceMonitor::add(Geofence fence) {
    fences_.push_back(std::move(fence));
    states_.emplace_back();
}

std::optional<bool> GeofenceMonitor::is_inside(std::size_t index) const {
    return states_.at(index).inside;
}

std::vector<FenceEvent> GeofenceMonitor::update(const TrackPoint& point) {
    std::vector<FenceEvent> events;
    for (std::size_t i = 0; i < fences_.size(); ++i) {
        const bool now_inside = fences_[i].contains(point.position);
        State& st = states_[i];
        if (!st.inside) {
            st.inside = now_inside;
            st.pending = 0;
            if (now_inside) {
                events.push_back(FenceEvent{fences_[i].name(), FenceTransition::Enter, point});
            }
            continue;
        }
        if (now_inside == *st.inside) {
            st.pending = 0;
            continue;
        }
        if (++st.pending >= confirm_fixes_) {
            st.inside = now_inside;
            st.pending = 0;
            events.push_back(FenceEvent{fences_[i].name(),
                                        now_inside ? FenceTransition::Enter : FenceTransition::Exit,
                                        point});
        }
    }
    return events;
}

}  // namespace nmea
