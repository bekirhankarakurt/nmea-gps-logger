# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-11-04

### Added
- NMEA 0183 sentence framing with XOR checksum validation, talker/proprietary address handling and a 128-byte line limit.
- Parsers for GGA, RMC, GSA, GSV (including NMEA 4.1 signal ID) and VTG with strict field validation.
- Epoch-based fix assembler that merges GGA/RMC/GSA/VTG bursts in any order and rolls the date over midnight.
- Track statistics: distance (haversine or Vincenty), duration, moving time, max/average speed, HDOP, altitude and elevation gain.
- Geofence monitor for circles and polygons (antimeridian-safe) with ENTER/EXIT events and optional jitter confirmation.
- GPX 1.1 and GeoJSON export (track, fences and events), written atomically.
- Inputs: log file or stdin, termios serial port (1200-921600 baud) and UDP listener for phone GPS apps.
- Replay mode that reproduces recorded timing at any speed, plus `--tee` recording of live input.
- `nmea-logger` CLI with human-readable and JSON-lines output, HDOP filter and duration limit.
- Dependency-free unit tests, pseudo-terminal serial test, UDP loopback test and an end-to-end CLI test on a replay fixture.
- CI on GCC and Clang with an ASan/UBSan job, cppcheck, clang-tidy, CodeQL, coverage and API docs on GitHub Pages.
- Release workflow producing linux-x86_64 and linux-aarch64 tarballs with SHA256SUMS.

[Unreleased]: https://github.com/bekirhankarakurt/nmea-gps-logger/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/bekirhankarakurt/nmea-gps-logger/releases/tag/v1.0.0
