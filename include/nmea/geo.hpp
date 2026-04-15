#pragma once

#include <optional>

namespace nmea {

/// WGS-84 position in decimal degrees (north and east positive).
struct GeoPoint {
    double lat_deg{};
    double lon_deg{};
};

/// Mean Earth radius (IUGG) used by spherical formulas.
inline constexpr double kEarthRadiusM = 6371008.8;
/// WGS-84 semi-major axis.
inline constexpr double kWgs84A = 6378137.0;
/// WGS-84 flattening.
inline constexpr double kWgs84F = 1.0 / 298.257223563;

inline constexpr double kKnotsToMps = 1852.0 / 3600.0;
inline constexpr double kKmhToMps = 1000.0 / 3600.0;

enum class DistanceMethod {
    Haversine,  ///< Great-circle on a sphere; fast, error up to ~0.5 %.
    Vincenty,   ///< Geodesic on the WGS-84 ellipsoid; sub-millimetre accuracy.
};

/// Result of the Vincenty inverse problem.
struct Geodesic {
    double distance_m{};
    double initial_bearing_deg{};  ///< [0, 360)
    double final_bearing_deg{};    ///< [0, 360)
};

[[nodiscard]] double deg_to_rad(double deg) noexcept;
[[nodiscard]] double rad_to_deg(double rad) noexcept;

/// Normalises an angle to [0, 360).
[[nodiscard]] double normalize_bearing(double deg) noexcept;

/// Great-circle distance in metres.
[[nodiscard]] double haversine_m(GeoPoint a, GeoPoint b) noexcept;

/// Initial great-circle bearing from @p a to @p b in degrees [0, 360).
[[nodiscard]] double initial_bearing_deg(GeoPoint a, GeoPoint b) noexcept;

/// Point reached travelling @p range_m metres along @p bearing_deg on a sphere.
[[nodiscard]] GeoPoint destination(GeoPoint origin, double bearing_deg, double range_m) noexcept;

/// Vincenty inverse on WGS-84. Returns nullopt when the iteration fails to
/// converge (nearly antipodal points).
[[nodiscard]] std::optional<Geodesic> vincenty_inverse(GeoPoint a, GeoPoint b) noexcept;

/// Distance using the chosen method; Vincenty falls back to haversine on
/// non-convergence so the result is always defined.
[[nodiscard]] double distance_m(GeoPoint a, GeoPoint b, DistanceMethod method) noexcept;

}  // namespace nmea
