# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| 1.0.x | Yes |
| < 1.0 | No |

## Threat model

`nmea-logger` parses untrusted bytes from serial ports and from the network
(the UDP listener accepts datagrams from any host that can reach the port).
Relevant safeguards:

- Lines are capped (128 bytes per sentence, 1024 bytes in the line buffer); overlong input is discarded.
- Every sentence is checksum-validated before decoding; numeric fields are parsed with `std::from_chars` and range-checked.
- No dynamic code paths depend on input content; all buffers are bounds-checked standard containers.
- CI runs AddressSanitizer/UndefinedBehaviorSanitizer, cppcheck, clang-tidy and CodeQL on every push.

Bind the UDP listener to a specific interface (`--bind 127.0.0.1` or your LAN
address) rather than `0.0.0.0` on untrusted networks.

## Reporting a vulnerability

Please report vulnerabilities privately via
[GitHub Security Advisories](https://github.com/bekirhankarakurt/nmea-gps-logger/security/advisories/new)
rather than a public issue. Include the input that triggers the problem (a
short NMEA capture is ideal), the version and your platform. You can expect an
acknowledgement within 7 days and a fix or mitigation plan within 30 days.
