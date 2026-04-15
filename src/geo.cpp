#include "nmea/geo.hpp"

#include <algorithm>
#include <cmath>

namespace nmea {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

double deg_to_rad(double deg) noexcept { return deg * kPi / 180.0; }
double rad_to_deg(double rad) noexcept { return rad * 180.0 / kPi; }

double normalize_bearing(double deg) noexcept {
    double r = std::fmod(deg, 360.0);
    if (r < 0.0) {
        r += 360.0;
    }
    return r >= 360.0 ? 0.0 : r;
}

double haversine_m(GeoPoint a, GeoPoint b) noexcept {
    const double phi1 = deg_to_rad(a.lat_deg);
    const double phi2 = deg_to_rad(b.lat_deg);
    const double dphi = phi2 - phi1;
    const double dlambda = deg_to_rad(b.lon_deg - a.lon_deg);
    const double s1 = std::sin(dphi / 2.0);
    const double s2 = std::sin(dlambda / 2.0);
    const double h = std::clamp(s1 * s1 + std::cos(phi1) * std::cos(phi2) * s2 * s2, 0.0, 1.0);
    return 2.0 * kEarthRadiusM * std::asin(std::sqrt(h));
}

double initial_bearing_deg(GeoPoint a, GeoPoint b) noexcept {
    const double phi1 = deg_to_rad(a.lat_deg);
    const double phi2 = deg_to_rad(b.lat_deg);
    const double dlambda = deg_to_rad(b.lon_deg - a.lon_deg);
    const double y = std::sin(dlambda) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(dlambda);
    return normalize_bearing(rad_to_deg(std::atan2(y, x)));
}

GeoPoint destination(GeoPoint origin, double bearing_deg, double range_m) noexcept {
    const double delta = range_m / kEarthRadiusM;
    const double theta = deg_to_rad(bearing_deg);
    const double phi1 = deg_to_rad(origin.lat_deg);
    const double lambda1 = deg_to_rad(origin.lon_deg);
    const double sin_phi2 =
        std::sin(phi1) * std::cos(delta) + std::cos(phi1) * std::sin(delta) * std::cos(theta);
    const double phi2 = std::asin(std::clamp(sin_phi2, -1.0, 1.0));
    const double lambda2 =
        lambda1 + std::atan2(std::sin(theta) * std::sin(delta) * std::cos(phi1),
                             std::cos(delta) - std::sin(phi1) * sin_phi2);
    double lon = rad_to_deg(lambda2);
    lon = std::fmod(lon + 540.0, 360.0) - 180.0;
    return GeoPoint{rad_to_deg(phi2), lon};
}

std::optional<Geodesic> vincenty_inverse(GeoPoint a, GeoPoint b) noexcept {
    constexpr double kB = kWgs84A * (1.0 - kWgs84F);
    constexpr int kMaxIterations = 200;
    constexpr double kTolerance = 1e-12;

    const double l = deg_to_rad(b.lon_deg - a.lon_deg);
    const double u1 = std::atan((1.0 - kWgs84F) * std::tan(deg_to_rad(a.lat_deg)));
    const double u2 = std::atan((1.0 - kWgs84F) * std::tan(deg_to_rad(b.lat_deg)));
    const double sin_u1 = std::sin(u1);
    const double cos_u1 = std::cos(u1);
    const double sin_u2 = std::sin(u2);
    const double cos_u2 = std::cos(u2);

    double lambda = l;
    double sin_sigma = 0.0;
    double cos_sigma = 1.0;
    double sigma = 0.0;
    double cos_sq_alpha = 1.0;
    double cos_2sigma_m = 0.0;
    double sin_lambda = 0.0;
    double cos_lambda = 1.0;

    bool converged = false;
    for (int i = 0; i < kMaxIterations; ++i) {
        sin_lambda = std::sin(lambda);
        cos_lambda = std::cos(lambda);
        const double t1 = cos_u2 * sin_lambda;
        const double t2 = cos_u1 * sin_u2 - sin_u1 * cos_u2 * cos_lambda;
        sin_sigma = std::sqrt(t1 * t1 + t2 * t2);
        if (sin_sigma == 0.0) {
            return Geodesic{0.0, 0.0, 0.0};  // Coincident points.
        }
        cos_sigma = sin_u1 * sin_u2 + cos_u1 * cos_u2 * cos_lambda;
        sigma = std::atan2(sin_sigma, cos_sigma);
        const double sin_alpha = cos_u1 * cos_u2 * sin_lambda / sin_sigma;
        cos_sq_alpha = 1.0 - sin_alpha * sin_alpha;
        // Equatorial lines have cos^2(alpha) == 0.
        cos_2sigma_m = cos_sq_alpha != 0.0 ? cos_sigma - 2.0 * sin_u1 * sin_u2 / cos_sq_alpha : 0.0;
        const double c = kWgs84F / 16.0 * cos_sq_alpha * (4.0 + kWgs84F * (4.0 - 3.0 * cos_sq_alpha));
        const double previous = lambda;
        lambda = l + (1.0 - c) * kWgs84F * sin_alpha *
                         (sigma + c * sin_sigma *
                                      (cos_2sigma_m + c * cos_sigma * (-1.0 + 2.0 * cos_2sigma_m * cos_2sigma_m)));
        if (std::fabs(lambda - previous) < kTolerance) {
            converged = true;
            break;
        }
        if (std::fabs(lambda) > 4.0 * kPi) {
            break;  // Diverging (antipodal).
        }
    }
    if (!converged) {
        return std::nullopt;
    }

    const double u_sq = cos_sq_alpha * (kWgs84A * kWgs84A - kB * kB) / (kB * kB);
    const double big_a = 1.0 + u_sq / 16384.0 * (4096.0 + u_sq * (-768.0 + u_sq * (320.0 - 175.0 * u_sq)));
    const double big_b = u_sq / 1024.0 * (256.0 + u_sq * (-128.0 + u_sq * (74.0 - 47.0 * u_sq)));
    const double c2 = cos_2sigma_m * cos_2sigma_m;
    const double delta_sigma =
        big_b * sin_sigma *
        (cos_2sigma_m + big_b / 4.0 *
                            (cos_sigma * (-1.0 + 2.0 * c2) -
                             big_b / 6.0 * cos_2sigma_m * (-3.0 + 4.0 * sin_sigma * sin_sigma) * (-3.0 + 4.0 * c2)));

    Geodesic result;
    result.distance_m = kB * big_a * (sigma - delta_sigma);
    result.initial_bearing_deg = normalize_bearing(
        rad_to_deg(std::atan2(cos_u2 * sin_lambda, cos_u1 * sin_u2 - sin_u1 * cos_u2 * cos_lambda)));
    result.final_bearing_deg = normalize_bearing(
        rad_to_deg(std::atan2(cos_u1 * sin_lambda, -sin_u1 * cos_u2 + cos_u1 * sin_u2 * cos_lambda)));
    return result;
}

double distance_m(GeoPoint a, GeoPoint b, DistanceMethod method) noexcept {
    if (method == DistanceMethod::Vincenty) {
        if (const auto g = vincenty_inverse(a, b)) {
            return g->distance_m;
        }
    }
    return haversine_m(a, b);
}

}  // namespace nmea
