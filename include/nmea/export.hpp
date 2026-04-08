#pragma once

#include "nmea/geofence.hpp"
#include "nmea/track.hpp"

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace nmea {

/// Writes a GPX 1.1 document with a single track segment.
void write_gpx(std::ostream& out, const std::vector<TrackPoint>& points, std::string_view name);

/// Writes a GeoJSON FeatureCollection: the track as a LineString, each
/// geofence as a Polygon and each fence event as a Point.
void write_geojson(std::ostream& out, const std::vector<TrackPoint>& points, std::string_view name,
                   const std::vector<Geofence>& fences = {},
                   const std::vector<FenceEvent>& events = {});

/// Escapes text for XML character data and attributes.
[[nodiscard]] std::string xml_escape(std::string_view text);

/// Escapes text for a JSON string literal (without the surrounding quotes).
[[nodiscard]] std::string json_escape(std::string_view text);

}  // namespace nmea
