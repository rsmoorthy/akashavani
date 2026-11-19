"""Unit-safe weather calculations used by archive ingestion.

The formulas are ported from Weewx's ``wxformulas`` / ``uwxutils`` modules so
that the derived values can be compared with the reference Weewx archive. The
stored values are SI; Weewx's US units are only used inside the pressure
formulas where the source implementation is defined in those units.
"""

from __future__ import annotations

import math

INHG_PER_HPA = 0.0295299830714
METER_PER_FOOT = 0.3048


def mean(total: float | None, count: int) -> float | None:
    if total is None or count <= 0:
        return None
    return total / count


def vector_direction(sin_sum: float, cos_sum: float) -> float | None:
    """Return a compass direction from speed-weighted sin/cos sums."""
    if sin_sum == 0.0 and cos_sum == 0.0:
        return None
    degrees = math.degrees(math.atan2(sin_sum, cos_sum))
    return degrees + 360.0 if degrees < 0.0 else degrees


def dewpoint_c(temperature_c: float | None, humidity_pct: float | None) -> float | None:
    if temperature_c is None or humidity_pct is None:
        return None
    if not 0.0 <= humidity_pct <= 100.0:
        return None
    if humidity_pct == 0.0:
        return None
    try:
        gamma = 17.27 * temperature_c / (237.7 + temperature_c) + math.log(humidity_pct / 100.0)
        return 237.7 * gamma / (17.27 - gamma)
    except (ValueError, OverflowError):
        return None


def windchill_c(temperature_c: float | None, wind_speed_ms: float | None) -> float | None:
    if temperature_c is None or wind_speed_ms is None:
        return None
    temperature_f = temperature_c * 9.0 / 5.0 + 32.0
    wind_mph = wind_speed_ms / 0.44704
    if temperature_f >= 50.0 or wind_mph <= 3.0:
        return temperature_c
    chill_f = (
        35.74
        + 0.6215 * temperature_f
        + (-35.75 + 0.4275 * temperature_f) * math.pow(wind_mph, 0.16)
    )
    return (chill_f - 32.0) * 5.0 / 9.0


def heatindex_c(temperature_c: float | None, humidity_pct: float | None) -> float | None:
    if temperature_c is None or humidity_pct is None:
        return None
    temperature_f = temperature_c * 9.0 / 5.0 + 32.0
    humidity = humidity_pct
    if temperature_f <= 40.0:
        return temperature_c
    heat_f = 0.5 * (temperature_f + 61.0 + ((temperature_f - 68.0) * 1.2) + (humidity * 0.094))
    if (heat_f + temperature_f) / 2.0 >= 80.0:
        heat_f = (
            -42.379
            + 2.04901523 * temperature_f
            + 10.14333127 * humidity
            - 0.22475541 * temperature_f * humidity
            - 6.83783e-3 * temperature_f**2
            - 5.481717e-2 * humidity**2
            + 1.22874e-3 * temperature_f**2 * humidity
            + 8.5282e-4 * temperature_f * humidity**2
            - 1.99e-6 * temperature_f**2 * humidity**2
        )
        if humidity < 13 and 80 < temperature_f < 112:
            adjustment = ((13 - humidity) / 4.0) * math.sqrt(
                (17 - abs(temperature_f - 95.0)) / 17.0
            )
            heat_f -= adjustment
        elif humidity > 85 and 80 <= temperature_f < 87:
            adjustment = ((humidity - 85) / 10.0) * ((87 - temperature_f) / 5.0)
            heat_f += adjustment
    return (heat_f - 32.0) * 5.0 / 9.0


def humidex_c(temperature_c: float | None, humidity_pct: float | None) -> float | None:
    if temperature_c is None or humidity_pct is None:
        return None
    dewpoint = dewpoint_c(temperature_c, humidity_pct)
    if dewpoint is None:
        return None
    dewpoint_k = dewpoint + 273.15
    if dewpoint_k <= 0.0:
        return temperature_c
    try:
        vapour_pressure = 6.11 * math.exp(5417.7530 * (1 / 273.15 - 1 / dewpoint_k))
    except (ValueError, OverflowError):
        return None
    humidex = 0.5555 * (vapour_pressure - 10.0)
    return temperature_c + humidex if humidex > 0.0 else temperature_c


def apptemp_c(
    temperature_c: float | None,
    humidity_pct: float | None,
    wind_speed_ms: float | None,
) -> float | None:
    if temperature_c is None or humidity_pct is None or wind_speed_ms is None:
        return None
    if not 0.0 <= humidity_pct <= 100.0 or wind_speed_ms < 0.0:
        return None
    vapour_pressure = (
        (humidity_pct / 100.0) * 6.105 * math.exp(17.27 * temperature_c / (237.7 + temperature_c))
    )
    return temperature_c + 0.33 * vapour_pressure - 0.70 * wind_speed_ms - 4.0


def windrun_km(wind_speed_ms: float | None, interval_minutes: int) -> float | None:
    if wind_speed_ms is None:
        return None
    return wind_speed_ms * interval_minutes * 60.0 / 1000.0


def sealevel_pressure_hpa(
    station_pressure_hpa: float | None,
    altitude_m: float,
    temperature_c: float | None,
) -> float | None:
    if station_pressure_hpa is None or temperature_c is None:
        return None
    exponent = math.exp(-altitude_m / ((temperature_c + 273.15) * 29.263))
    return station_pressure_hpa / exponent if exponent else None


def altimeter_hpa(station_pressure_hpa: float | None, altitude_m: float) -> float | None:
    if station_pressure_hpa is None or station_pressure_hpa <= 0.3:
        return None
    pressure_inhg = station_pressure_hpa * INHG_PER_HPA
    altitude_ft = altitude_m / METER_PER_FOOT
    altimeter_inhg = (pressure_inhg**0.1903 + 1.313e-5 * altitude_ft) ** 5.255
    return altimeter_inhg / INHG_PER_HPA


def cloudbase_m(
    temperature_c: float | None,
    humidity_pct: float | None,
    altitude_m: float,
) -> float | None:
    if temperature_c is None:
        return None
    dewpoint = dewpoint_c(temperature_c, humidity_pct)
    if dewpoint is None:
        return None
    return altitude_m + (temperature_c - dewpoint) * 1000.0 / 2.5 * METER_PER_FOOT


def rain_rate_mm_h_from_raw(
    raw: int | None,
    *,
    high_rate: bool,
    bucket_mm: float,
) -> float | None:
    """Convert the Davis 0x5 click interval to mm/h.

    ``raw == 0`` means no rain. ``None`` means no 0x5 frame was received.
    """
    if raw is None:
        return None
    if raw <= 0:
        return 0.0
    clicks_per_hour = (57600.0 if high_rate else 3600.0) / raw
    return clicks_per_hour * bucket_mm
