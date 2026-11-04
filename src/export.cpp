#include "nmea/export.hpp"

#include "nmea/time.hpp"

#include <array>
#include <cstdio>
#include <locale>
#include <sstream>

namespace nmea {

std::string xml_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string json_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20U) {
                    std::array<char, 8> buf{};
                    std::snprintf(buf.data(), buf.size(), "\\u%04x", static_cast<unsigned>(c));
                    out += buf.data();
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

namespace {

/// Locale-independent fixed-point formatting.
[[nodiscard]] std::string num(double value, int decimals) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os.setf(std::ios::fixed);
    os.precision(decimals);
    os << value;
    return os.str();
}

void write_ring(std::ostream& out, const std::vector<GeoPoint>& ring) {
    out << "[[";
    for (std::size_t i = 0; i < ring.size(); ++i) {
        out << (i == 0 ? "" : ",") << '[' << num(ring[i].lon_deg, 7) << ',' << num(ring[i].lat_deg, 7)
            << ']';
    }
    out << "]]";
}

}  // namespace

void write_gpx(std::ostream& out, const std::vector<TrackPoint>& points, std::string_view name) {
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<gpx version=\"1.1\" creator=\"nmea-gps-logger\" "
           "xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
           "  <trk>\n"
        << "    <name>" << xml_escape(name) << "</name>\n"
        << "    <trkseg>\n";
    for (const auto& p : points) {
        out << "      <trkpt lat=\"" << num(p.position.lat_deg, 7) << "\" lon=\""
            << num(p.position.lon_deg, 7) << "\">";
        if (p.altitude_m) {
            out << "<ele>" << num(*p.altitude_m, 1) << "</ele>";
        }
        if (p.has_date) {
            out << "<time>" << format_iso8601(p.time_s) << "</time>";
        }
        if (p.quality == FixQuality::Dgps) {
            out << "<fix>dgps</fix>";
        } else if (p.quality == FixQuality::Pps) {
            out << "<fix>pps</fix>";
        }
        if (p.satellites) {
            out << "<sat>" << *p.satellites << "</sat>";
        }
        if (p.hdop) {
            out << "<hdop>" << num(*p.hdop, 1) << "</hdop>";
        }
        out << "</trkpt>\n";
    }
    out << "    </trkseg>\n  </trk>\n</gpx>\n";
}

void write_geojson(std::ostream& out, const std::vector<TrackPoint>& points, std::string_view name,
                   const std::vector<Geofence>& fences, const std::vector<FenceEvent>& events) {
    out << "{\"type\":\"FeatureCollection\",\"features\":[";
    bool first = true;
    const auto sep = [&]() {
        out << (first ? "\n" : ",\n");
        first = false;
    };

    if (!points.empty()) {
        sep();
        out << "{\"type\":\"Feature\",\"properties\":{\"kind\":\"track\",\"name\":\"" << json_escape(name)
            << "\",\"points\":" << points.size();
        if (points.front().has_date) {
            out << ",\"start\":\"" << format_iso8601(points.front().time_s) << "\",\"end\":\""
                << format_iso8601(points.back().time_s) << '"';
        }
        // A LineString needs two positions; a single fix becomes a Point.
        if (points.size() == 1) {
            out << "},\"geometry\":{\"type\":\"Point\",\"coordinates\":[" << num(points[0].position.lon_deg, 7)
                << ',' << num(points[0].position.lat_deg, 7) << "]}}";
        } else {
            out << "},\"geometry\":{\"type\":\"LineString\",\"coordinates\":[";
            for (std::size_t i = 0; i < points.size(); ++i) {
                const auto& p = points[i];
                out << (i == 0 ? "" : ",") << '[' << num(p.position.lon_deg, 7) << ','
                    << num(p.position.lat_deg, 7);
                if (p.altitude_m) {
                    out << ',' << num(*p.altitude_m, 1);
                }
                out << ']';
            }
            out << "]}}";
        }
    }

    for (const auto& fence : fences) {
        sep();
        out << "{\"type\":\"Feature\",\"properties\":{\"kind\":\"geofence\",\"name\":\""
            << json_escape(fence.name()) << "\",\"shape\":\""
            << (fence.shape() == FenceShape::Circle ? "circle" : "polygon") << '"';
        if (fence.shape() == FenceShape::Circle) {
            out << ",\"radius_m\":" << num(fence.radius_m(), 1);
        }
        out << "},\"geometry\":{\"type\":\"Polygon\",\"coordinates\":";
        write_ring(out, fence.outline());
        out << "}}";
    }

    for (const auto& ev : events) {
        sep();
        out << "{\"type\":\"Feature\",\"properties\":{\"kind\":\"fence-event\",\"fence\":\""
            << json_escape(ev.fence) << "\",\"transition\":\"" << to_string(ev.transition) << '"';
        if (ev.point.has_date) {
            out << ",\"time\":\"" << format_iso8601(ev.point.time_s) << '"';
        }
        out << "},\"geometry\":{\"type\":\"Point\",\"coordinates\":[" << num(ev.point.position.lon_deg, 7)
            << ',' << num(ev.point.position.lat_deg, 7) << "]}}";
    }
    out << "\n]}\n";
}

}  // namespace nmea
