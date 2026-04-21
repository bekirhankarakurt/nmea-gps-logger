# nmea-gps-logger

**Turn any NMEA 0183 GNSS stream — USB receiver, phone over UDP, or a recorded log — into validated fixes, GPX/GeoJSON tracks, trip statistics and real-time geofence alerts.**

[![CI](https://github.com/bekirhankarakurt/nmea-gps-logger/actions/workflows/ci.yml/badge.svg)](https://github.com/bekirhankarakurt/nmea-gps-logger/actions/workflows/ci.yml)
[![CodeQL](https://github.com/bekirhankarakurt/nmea-gps-logger/actions/workflows/codeql.yml/badge.svg)](https://github.com/bekirhankarakurt/nmea-gps-logger/actions/workflows/codeql.yml)
[![Coverage](https://img.shields.io/endpoint?url=https://bekirhankarakurt.github.io/nmea-gps-logger/coverage/badge.json)](https://bekirhankarakurt.github.io/nmea-gps-logger/coverage/)
[![Release](https://img.shields.io/github/v/release/bekirhankarakurt/nmea-gps-logger)](https://github.com/bekirhankarakurt/nmea-gps-logger/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20POSIX-lightgrey.svg)](#requirements)
[![Docs](https://img.shields.io/badge/docs-API%20reference-informational.svg)](https://bekirhankarakurt.github.io/nmea-gps-logger/)

![nmea-logger replaying a recorded ride with a geofence](docs/demo.gif)

**Documentation:** [API reference](https://bekirhankarakurt.github.io/nmea-gps-logger/) · [Coverage report](https://bekirhankarakurt.github.io/nmea-gps-logger/coverage/) · [Changelog](CHANGELOG.md)

## Why

- **Trustworthy input.** Every sentence is checksum-verified and every field range-checked; corrupt lines are counted, never silently turned into bogus positions.
- **One pipeline, three sources.** The same parse → assemble → filter → statistics → geofence chain runs on a termios serial port, a UDP socket or a file, so whatever you test on a replay behaves identically in the field.
- **Replay first.** Record live data with `--tee`, then replay it at real time or 100× to reproduce field issues without hardware.
- **Geodesy done properly.** Haversine for speed, Vincenty on WGS-84 for sub-millimetre distances, antimeridian-safe polygon fences.
- **Small and auditable.** ~2 k lines of dependency-free C++17, zero warnings under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, sanitizer-clean, CodeQL/cppcheck/clang-tidy gated.

## Quick start (60 seconds)

```bash
git clone https://github.com/bekirhankarakurt/nmea-gps-logger.git
cd nmea-gps-logger
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
./build/nmea-logger --file tests/fixtures/metu_ride.nmea --quiet --gpx ride.gpx
```

```text
--- summary ---
sentences   1367 ok, 3 rejected (2 checksum, 1 unsupported)
fixes       314
distance    1.922 km
duration    0:05:18 (moving 0:05:18)
speed       max 23.2 km/h, avg 21.8 km/h, moving avg 21.8 km/h
hdop        mean 1.03, max 3.40
altitude    977.6 .. 1002.6 m, gain 66.0 m
```

Prebuilt `linux-x86_64` and `linux-aarch64` binaries (Raspberry Pi 4/5, Jetson) are on the [releases page](https://github.com/bekirhankarakurt/nmea-gps-logger/releases/latest).

## Requirements

| Requirement | Version | Notes |
|---|---|---|
| C++ compiler | GCC ≥ 9 or Clang ≥ 10 | C++17 with floating-point `std::from_chars` (GCC ≥ 11 / libstdc++ recommended) |
| CMake | ≥ 3.16 | |
| OS | Linux (any POSIX with termios + BSD sockets) | Tested on Ubuntu 24.04 x86_64 and aarch64 |
| Hardware | optional | Any NMEA 0183 receiver (u-blox, MediaTek, SiRF, Quectel…) or a phone app that streams NMEA over UDP |

### Build options

| CMake option | Default | Effect |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Release` | `Debug`, `Release`, `RelWithDebInfo` |
| `NMEA_BUILD_TESTS` | `ON` | Build unit and end-to-end tests (CTest) |
| `NMEA_WARNINGS_AS_ERRORS` | `OFF` | Add `-Werror` (enabled in CI) |
| `NMEA_SANITIZE` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `NMEA_COVERAGE` | `OFF` | `--coverage` instrumentation for gcovr |

Install system-wide with `cmake --install build --prefix /usr/local`.

## Usage

```text
nmea-logger INPUT [OPTIONS]
```

| Option | Argument | Description |
|---|---|---|
| `-f`, `--file` | `PATH` | Read a log file; `-` reads stdin |
| `-s`, `--serial` | `DEVICE` | Serial receiver, e.g. `/dev/ttyUSB0`, `/dev/ttyACM0` |
| `-b`, `--baud` | `RATE` | 1200 … 921600, default `9600` |
| `-u`, `--udp` | `PORT` | Listen for NMEA datagrams (one or more sentences each) |
| `--bind` | `ADDRESS` | IPv4 address for `--udp`, default `0.0.0.0` |
| `-r`, `--replay-speed` | `X` | Pace file input by its timestamps at X× real time |
| `--gpx` | `PATH` | Write GPX 1.1 track on exit |
| `--geojson` | `PATH` | Write GeoJSON with track, fence polygons and event points |
| `--tee` | `PATH` | Append every valid raw sentence (record for later replay) |
| `--name` | `TEXT` | Track name in exports |
| `--json` | | JSON-lines output for fixes, events and summary |
| `-q`, `--quiet` | | Only events and summary |
| `-v`, `--verbose` | | Print rejected sentences with the reason on stderr |
| `--max-hdop` | `X` | Drop fixes with HDOP above X |
| `--vincenty` | | Vincenty (WGS-84 ellipsoid) distances instead of haversine |
| `--no-checksum` | | Accept sentences without `*hh` |
| `--duration` | `SECONDS` | Stop after a time limit (live inputs) |
| `--fence-circle` | `NAME:LAT,LON,RADIUS_M` | Circular geofence (repeatable) |
| `--fence-polygon` | `NAME:LAT,LON;LAT,LON;…` | Polygon geofence, ≥ 3 vertices (repeatable) |
| `--fence-confirm` | `N` | Consecutive fixes needed to confirm a transition, default `1` |
| `-h`, `--help` / `-V`, `--version` | | Help / version |

Exit status: `0` success, `1` runtime error (device, socket, file), `2` usage error. `Ctrl-C` stops cleanly and still writes exports.

### Recipes

**1. Log a USB receiver and keep a raw recording**

```bash
nmea-logger --serial /dev/ttyACM0 --baud 9600 --tee "raw-$(date +%F).nmea" --gpx "track-$(date +%F).gpx"
```

Add yourself to the `dialout` group once (`sudo usermod -aG dialout "$USER"`) to access the port without root.

**2. Use a phone as the GNSS receiver**

Install any app that forwards NMEA over UDP (e.g. *GPSd Forwarder* or *NMEA over UDP*), point it at your computer's IP and port 10110, then:

```bash
nmea-logger --udp 10110 --geojson phone.geojson --max-hdop 3
```

**3. Geofence alerts for a site perimeter, as JSON lines for another program**

```bash
nmea-logger --file tests/fixtures/metu_ride.nmea --json --quiet --fence-confirm 2 \
  --fence-circle ne-corner:39.8920,32.7860,60 \
  --fence-polygon "south-road:39.8885,32.7790;39.8885,32.7870;39.8875,32.7870;39.8875,32.7790" \
  | grep '"event"'
```

```text
{"type":"event","fence":"ne-corner","transition":"ENTER","time":"2026-05-14T08:31:17Z","lat":39.8919966,"lon":32.7854299}
{"type":"event","fence":"ne-corner","transition":"EXIT","time":"2026-05-14T08:31:37Z","lat":39.8913531,"lon":32.7859993}
{"type":"event","fence":"south-road","transition":"ENTER","time":"2026-05-14T08:32:31Z","lat":39.8884365,"lon":32.7859993}
{"type":"event","fence":"south-road","transition":"EXIT","time":"2026-05-14T08:34:15Z","lat":39.8885984,"lon":32.7799957}
```

**4. Reproduce a field run at 10× speed**

```bash
nmea-logger --file raw-2026-05-14.nmea --replay-speed 10 --fence-circle base:39.8920,32.7860,60
# try it on the bundled recording:
nmea-logger --file tests/fixtures/metu_ride.nmea --replay-speed 10 --quiet --fence-circle base:39.8920,32.7860,60
```

**5. Survey-grade distance and a quality audit of a log**

```bash
nmea-logger --file tests/fixtures/metu_ride.nmea --quiet --vincenty --max-hdop 2.0 --verbose
```

Rejected sentences are listed with their reason (`checksum-mismatch`, `unsupported-type`, …); the summary shows how many fixes the HDOP filter removed.

## Architecture

```text
 ┌──────────────┐  ┌───────────────┐  ┌──────────────┐
 │ SerialSource │  │   UdpSource   │  │  FileSource  │   io.hpp   (LineSource, UniqueFd, LineBuffer)
 │ termios 8N1  │  │ poll + recv   │  │ file / stdin │
 └──────┬───────┘  └───────┬───────┘  └──────┬───────┘
        └──────────────────┼─────────────────┘
                           │ text lines            ┌──────────────┐
                           ▼                       │ ReplayPacer  │ replay.hpp (file input only)
 ┌─────────────────────────────────────────────────┴──────────────┴─┐
 │ Pipeline (pipeline.hpp) — no I/O, fully unit-testable              │
 │                                                                    │
 │  split_sentence ─► parse ─► FixAssembler ─► HDOP ─► StatsAccumulator│
 │  (checksum)       (GGA/RMC/  (epoch merge,  filter   (geo.hpp:      │
 │                    GSA/GSV/   date rollover)          haversine /   │
 │                    VTG)                               Vincenty)     │
 │                                         └──────────► GeofenceMonitor│
 └────────────────────────────────────┬───────────────────────────────┘
                                      ▼
            stdout (text / JSON lines) · GPX 1.1 · GeoJSON · --tee raw log
```

### Design trade-offs

| Decision | Why | Cost |
|---|---|---|
| Epoch assembly keyed on UTC time-of-day | Receivers emit GGA/RMC/GSA/VTG in vendor-specific order; keying on time merges them regardless | A fix is emitted when the *next* epoch starts (one-epoch latency, ~1 s at 1 Hz) |
| `Result<T>` value-or-error instead of exceptions in the parser | Bad input is expected, not exceptional; zero-cost rejection at line rate | Slightly more verbose call sites |
| `std::variant` message type | Exhaustive `std::visit`, no heap allocation, no RTTI | Adding a sentence touches the variant |
| Haversine by default, Vincenty on request | Haversine is ~10× faster and its ≤ 0.5 % error is below GNSS noise at walking/driving scale | Long baselines need `--vincenty` |
| Polygon test in a local equirectangular frame | Exact enough for fences up to tens of km, handles the antimeridian | Not suitable for continent-sized polygons |
| Points kept in memory, exports written on exit via temp file + rename | Simple, atomic files; 24 h at 1 Hz is only ~8 MB | A crash loses the export — `--tee` keeps the raw data |
| `poll()` with 200 ms timeout on serial/UDP | Clean `Ctrl-C`/`--duration` without threads | Up to 200 ms shutdown latency |

## Data formats

### Supported sentences

| Sentence | Content used | Notes |
|---|---|---|
| `GGA` | time, position, fix quality, satellites, HDOP, altitude, geoid separation, DGPS age/station | Quality 0 discards the fix |
| `RMC` | time, status, position, speed (kn), course, date, magnetic variation, FAA mode | Status `V` or mode `N` marks invalid |
| `GSA` | selection mode, 2D/3D, PRNs used, PDOP/HDOP/VDOP, NMEA 4.1 system ID | HDOP used when GGA lacks it |
| `GSV` | satellites in view: PRN, elevation, azimuth, SNR, NMEA 4.1 signal ID | Parsed and counted, not part of fixes |
| `VTG` | true/magnetic course, speed (kn and km/h), FAA mode | Speed/course fallback when RMC is absent |

Any talker ID is accepted (`GP`, `GN`, `GL`, `GA`, `GB`, `BD`, …). Proprietary (`$P…`) and other well-formed sentences are counted as `unsupported` and still recorded by `--tee`.

### Fix quality (GGA field 6)

| Value | Output | Meaning |
|---|---|---|
| 0 | — | No fix (dropped) |
| 1 | `GPS` | Autonomous GNSS |
| 2 | `DGPS` | Differential / SBAS |
| 3 | `PPS` | Precise positioning service |
| 4 / 5 | `RTK` / `FRTK` | RTK fixed / float |
| 6 | `DR` | Dead reckoning |
| 7 / 8 | `MANUAL` / `SIM` | Manual input / simulator |

### JSON-lines output (`--json`)

| `type` | Fields |
|---|---|
| `fix` | `time`, `lat`, `lon`, `alt_m`?, `speed_kmh`?, `course_deg`?, `hdop`?, `sats`?, `quality` |
| `event` | `fence`, `transition` (`ENTER`/`EXIT`), `time`, `lat`, `lon` |
| `summary` | `lines`, `accepted`, `rejected`, `checksum_errors`, `unsupported`, `fixes`, `filtered`, `distance_m`, `duration_s`, `moving_time_s`, `max_speed_kmh`, `avg_speed_kmh`, `mean_hdop`?, `max_hdop`?, `events` |

`time` is ISO 8601 UTC when a date is known (from RMC), otherwise `hh:mm:ssZ`. GeoJSON features carry `kind` = `track`, `geofence` or `fence-event`.

## Testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DNMEA_SANITIZE=ON -DNMEA_WARNINGS_AS_ERRORS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

| Suite | Covers |
|---|---|
| `test_sentence`, `test_parser` | Checksums, framing errors, all five sentence types incl. NMEA 4.1 fields, corrupt/out-of-range fields |
| `test_geo`, `test_time` | Vincenty against the published Flinders Peak–Buninyong reference, antipodal fallback, calendar maths |
| `test_track`, `test_stats` | Epoch merging in any order, outages, midnight rollover, moving time, Vincenty vs haversine |
| `test_geofence` | Circles, concave and antimeridian polygons, enter/exit sequencing, jitter confirmation |
| `test_export` | GPX/GeoJSON structure, escaping, edge cases |
| `test_io` | Line reassembly, `UniqueFd`, **serial over a pseudo-terminal**, **UDP over loopback** |
| `test_replay`, `test_pipeline` | Pacing maths, full fixture replay with known counts, distance and fence events |
| `test_cli`, `cli_replay_end_to_end` | Argument validation; real binary run → GPX/GeoJSON/tee checked, tee replays identically |

Line coverage is ~93 % ([report](https://bekirhankarakurt.github.io/nmea-gps-logger/coverage/)). The fixture `tests/fixtures/metu_ride.nmea` is generated deterministically by `tools/gen_fixture.py`. CI runs GCC and Clang, an ASan/UBSan job, cppcheck, clang-tidy and CodeQL.

## Troubleshooting / FAQ

| Symptom | Fix |
|---|---|
| `cannot open serial device ... Permission denied` | `sudo usermod -aG dialout "$USER"`, then log out and in |
| Serial output is garbage, everything `rejected` | Wrong baud rate — try `--baud 4800`, `9600`, `38400` or `115200`; check with `--verbose` |
| `0 fixes` but sentences are accepted | Receiver has no fix yet (GGA quality 0 / RMC `V`). Wait for sky view; `--json` + GSV in the raw log show satellites |
| Nothing arrives on `--udp` | Check the phone and computer are on the same network, the app targets the right IP/port, and the firewall allows UDP (`sudo ufw allow 10110/udp`) |
| Log has no `*hh` checksums | Add `--no-checksum` |
| GPX has no `<time>` elements | The stream has no RMC, so no date is known; enable RMC on the receiver |
| Many fixes dropped with `--max-hdop` | Raise the threshold; HDOP > 5 is poor, < 2 is good |
| Fence events flicker at the boundary | Use `--fence-confirm 3` |

**Does it work on Windows/macOS?** The library and file replay are portable C++17; serial/UDP sources use POSIX termios and sockets, so macOS works and Windows needs WSL.

## Project layout

```text
nmea-gps-logger/
├── app/main.cpp              CLI: sources, signal handling, output, exports
├── include/nmea/             Public headers (one per module)
│   ├── sentence.hpp          framing + checksum
│   ├── parser.hpp            GGA/RMC/GSA/GSV/VTG decoding
│   ├── messages.hpp          message structs and enums
│   ├── track.hpp             epoch → TrackPoint assembly
│   ├── stats.hpp             trip statistics
│   ├── geo.hpp               haversine, Vincenty, bearings
│   ├── geofence.hpp          fences + monitor
│   ├── export.hpp            GPX / GeoJSON
│   ├── io.hpp                serial, UDP, file sources
│   ├── replay.hpp            replay pacing
│   ├── pipeline.hpp          hardware-independent processing chain
│   └── cli.hpp               argument parsing
├── src/                      Implementations
├── tests/                    Unit tests, end-to-end test, fixtures/
├── tools/gen_fixture.py      Deterministic replay fixture generator
├── docs/                     demo.tape (VHS), demo.gif, API main page
├── .github/workflows/        CI, CodeQL, docs/coverage Pages, release
├── Doxyfile  .clang-tidy  CMakeLists.txt
└── CHANGELOG.md  CONTRIBUTING.md  SECURITY.md  LICENSE
```

## Roadmap

- [x] GGA, RMC, GSA, GSV, VTG with checksum validation
- [x] Serial, UDP and file inputs with replay and recording
- [x] GPX and GeoJSON export, trip statistics
- [x] Circle and polygon geofences with jitter confirmation
- [ ] GLL, ZDA and GST (position error estimates) sentences
- [ ] Per-constellation satellite sky view from GSV groups
- [ ] Streaming GPX writer for multi-day sessions
- [ ] TCP client input (gpsd, NTRIP casters) and NMEA forwarding
- [ ] KML export and geofence definitions from GeoJSON files

## License

[MIT](LICENSE) © Bekirhan Karakurt

Bekirhan Karakurt - [github.com/bekirhankarakurt](https://github.com/bekirhankarakurt)
