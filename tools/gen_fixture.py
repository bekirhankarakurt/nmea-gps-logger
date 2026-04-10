#!/usr/bin/env python3
"""Generate the deterministic NMEA replay fixture used by the tests and demo.

Simulates a bicycle loop on the METU campus in Ankara at 1 Hz with a
multi-sentence burst per epoch (GGA, GSA, RMC, VTG and periodic GSV),
a short signal outage and a few corrupted lines, the way a real receiver
log looks.

Usage: tools/gen_fixture.py > tests/fixtures/metu_ride.nmea
"""

import math
import random

WAYPOINTS = [(39.8920, 32.7800), (39.8920, 32.7860), (39.8880, 32.7860), (39.8880, 32.7800), (39.8920, 32.7800)]
SPEED_MPS = 6.0
START = (2026, 5, 14, 8, 30, 0)
OUTAGE = range(140, 145)          # epochs without a fix
CORRUPT = {37, 211}               # epochs whose GGA checksum is broken
R = 6371008.8


def checksum(body):
    c = 0
    for ch in body:
        c ^= ord(ch)
    return f"${body}*{c:02X}"


def hav(a, b):
    p1, p2 = math.radians(a[0]), math.radians(b[0])
    dp, dl = p2 - p1, math.radians(b[1] - a[1])
    h = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * R * math.asin(math.sqrt(h))


def bearing(a, b):
    p1, p2 = math.radians(a[0]), math.radians(b[0])
    dl = math.radians(b[1] - a[1])
    y = math.sin(dl) * math.cos(p2)
    x = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
    return (math.degrees(math.atan2(y, x)) + 360) % 360


def lat_field(v):
    hemi = "N" if v >= 0 else "S"
    v = abs(v)
    d = int(v)
    return f"{d:02d}{(v - d) * 60:09.6f}", hemi


def lon_field(v):
    hemi = "E" if v >= 0 else "W"
    v = abs(v)
    d = int(v)
    return f"{d:03d}{(v - d) * 60:09.6f}", hemi


def positions():
    for a, b in zip(WAYPOINTS, WAYPOINTS[1:]):
        seg = hav(a, b)
        steps = int(seg / SPEED_MPS)
        crs = bearing(a, b)
        for i in range(steps):
            t = i / steps
            yield (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t), crs
    yield WAYPOINTS[-1], 270.0


def main():
    rng = random.Random(1923)
    y, mo, d, hh, mm, ss = START
    base = hh * 3600 + mm * 60 + ss
    date = f"{d:02d}{mo:02d}{y % 100:02d}"
    lines = []
    for n, ((lat, lon), crs) in enumerate(positions()):
        tod = base + n
        t = f"{tod // 3600:02d}{tod // 60 % 60:02d}{tod % 60:02d}.00"
        lat += rng.gauss(0, 0.000004)
        lon += rng.gauss(0, 0.000004)
        la, lh = lat_field(lat)
        lo, oh = lon_field(lon)
        alt = 990 + 12 * math.sin(n / 40) + rng.gauss(0, 0.3)
        hdop = round(0.8 + 0.3 * abs(math.sin(n / 25)) + (2.5 if 145 <= n < 150 else 0), 1)
        sats = 9 if hdop < 1.5 else 6
        spd = SPEED_MPS + rng.gauss(0, 0.2)
        kn = spd / 0.514444
        if n in OUTAGE:
            lines.append(checksum(f"GPGGA,{t},,,,,0,00,99.9,,M,,M,,"))
            lines.append(checksum("GPGSA,A,1,,,,,,,,,,,,,99.9,99.9,99.9"))
            lines.append(checksum(f"GPRMC,{t},V,,,,,,,{date},,,N"))
            lines.append(checksum("GPVTG,,T,,M,,N,,K,N"))
            continue
        gga = checksum(f"GPGGA,{t},{la},{lh},{lo},{oh},1,{sats:02d},{hdop},{alt:.1f},M,36.4,M,,")
        if n in CORRUPT:
            gga = gga[:-2] + ("00" if gga[-2:] != "00" else "11")
        lines.append(gga)
        prns = [2, 5, 7, 9, 13, 15, 18, 20, 30][:sats]
        prn_fields = ",".join(f"{p:02d}" for p in prns) + "," * (12 - len(prns))
        lines.append(checksum(f"GPGSA,A,3,{prn_fields},{hdop + 0.6:.1f},{hdop},{hdop + 0.4:.1f}"))
        lines.append(checksum(f"GPRMC,{t},A,{la},{lh},{lo},{oh},{kn:.2f},{crs:.1f},{date},5.6,E,A"))
        lines.append(checksum(f"GPVTG,{crs:.1f},T,{crs - 5.6:.1f},M,{kn:.2f},N,{spd * 3.6:.2f},K,A"))
        if n % 10 == 0:
            sats_all = [(2, 41, 63, 44), (5, 18, 290, 38), (7, 62, 171, 46), (9, 25, 40, 40),
                        (13, 33, 233, 41), (15, 70, 312, 47), (18, 12, 101, 31), (20, 48, 145, 45),
                        (30, 21, 330, 36), (24, 4, 210, None)]
            groups = [sats_all[i:i + 4] for i in range(0, len(sats_all), 4)]
            for gi, g in enumerate(groups, 1):
                body = f"GPGSV,{len(groups)},{gi},{len(sats_all):02d}"
                for prn, el, az, snr in g:
                    body += f",{prn:02d},{el:02d},{az:03d}," + ("" if snr is None else f"{snr:02d}")
                lines.append(checksum(body))
        if n == 60:
            lines.append(checksum("PGRMZ,3250,f,3"))
        if n == 90:
            lines.append("")
    print("\r\n".join(lines), end="\r\n")


if __name__ == "__main__":
    main()
