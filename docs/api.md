# nmea-gps-logger API {#mainpage}

C++17 library behind the `nmea-logger` command-line tool. Everything lives in
namespace `nmea` and is header-declared under `include/nmea/`.

| Header | Purpose |
|---|---|
| `sentence.hpp` | Framing, checksum validation, field splitting (`split_sentence`, `make_sentence`) |
| `parser.hpp` / `messages.hpp` | GGA, RMC, GSA, GSV and VTG decoding into a `std::variant` `Message` |
| `track.hpp` | `FixAssembler`: merges one epoch of sentences into a `TrackPoint` |
| `stats.hpp` | `StatsAccumulator`: distance, duration, speeds, HDOP, altitude |
| `geo.hpp` | Haversine, Vincenty inverse (WGS-84), bearing, destination |
| `geofence.hpp` | Circle and polygon fences, `GeofenceMonitor` enter/exit events |
| `export.hpp` | GPX 1.1 and GeoJSON writers |
| `io.hpp` | `FileSource`, termios `SerialSource`, `UdpSource`, `LineBuffer`, `UniqueFd` |
| `replay.hpp` | `ReplayPacer`: reproduce recorded timing at any speed |
| `pipeline.hpp` | `Pipeline`: hardware-independent parse → assemble → filter → stats → fences chain |
| `cli.hpp` | Command-line parsing and fence specs |

Minimal use:

```cpp
#include "nmea/pipeline.hpp"

nmea::Pipeline pipeline;
pipeline.add_fence(nmea::Geofence::circle("base", {39.8920, 32.7860}, 60.0));
for (const auto& line : lines) {
    for (const auto& event : pipeline.process_line(line).events) {
        // handle ENTER/EXIT
    }
}
(void)pipeline.finish();
const nmea::TrackStats& stats = pipeline.stats();
```

Source and README: <https://github.com/bekirhankarakurt/nmea-gps-logger>.
Coverage report: [coverage/index.html](coverage/index.html).
