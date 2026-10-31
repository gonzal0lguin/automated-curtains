#include "sun.h"

#include <math.h>

namespace sun {

static constexpr double D2R = M_PI / 180.0;
static constexpr double R2D = 180.0 / M_PI;

static double norm(double v, double max) {
    while (v < 0) v += max;
    while (v >= max) v -= max;
    return v;
}

// "Almanac for Computers" sunrise algorithm. Returns UTC hours (0..24).
static bool utcEvent(int year, int month, int day, double lat, double lng, bool rise, double& utHours) {
    const double zenith = 90.833;  // official: refraction + solar disc radius
    double N1 = floor(275.0 * month / 9.0);
    double N2 = floor((month + 9) / 12.0);
    double N3 = 1 + floor((year - 4 * floor(year / 4.0) + 2) / 3.0);
    double N = N1 - (N2 * N3) + day - 30;

    double lngHour = lng / 15.0;
    double t = N + ((rise ? 6.0 : 18.0) - lngHour) / 24.0;
    double M = 0.9856 * t - 3.289;
    double L = norm(M + 1.916 * sin(M * D2R) + 0.020 * sin(2 * M * D2R) + 282.634, 360);
    double RA = norm(R2D * atan(0.91764 * tan(L * D2R)), 360);
    RA += floor(L / 90) * 90 - floor(RA / 90) * 90;
    RA /= 15.0;

    double sinDec = 0.39782 * sin(L * D2R);
    double cosDec = cos(asin(sinDec));
    double cosH = (cos(zenith * D2R) - sinDec * sin(lat * D2R)) / (cosDec * cos(lat * D2R));
    if (cosH > 1 || cosH < -1) return false;

    double H = rise ? 360 - R2D * acos(cosH) : R2D * acos(cosH);
    H /= 15.0;
    double T = H + RA - 0.06571 * t - 6.622;
    utHours = norm(T - lngHour, 24);
    return true;
}

static bool localMinutes(const struct tm& day, double lat, double lng, bool rise, int& out) {
    double ut;
    if (!utcEvent(day.tm_year + 1900, day.tm_mon + 1, day.tm_mday, lat, lng, rise, ut)) return false;

    // UTC midnight of that calendar date (timegm equivalent, TZ independent)
    int y = day.tm_year + 1900, m = day.tm_mon + 1, d = day.tm_mday;
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    time_t midnightUtc = (time_t)((era * 146097 + doe - 719468) * 86400L);

    time_t ev = midnightUtc + (time_t)(ut * 3600.0);
    struct tm lt;
    localtime_r(&ev, &lt);
    // the UTC hour may have wrapped onto the neighbouring local day
    if (lt.tm_yday != day.tm_yday || lt.tm_year != day.tm_year) {
        bool after = (lt.tm_year > day.tm_year) || (lt.tm_year == day.tm_year && lt.tm_yday > day.tm_yday);
        ev += after ? -86400 : 86400;
        localtime_r(&ev, &lt);
    }
    out = lt.tm_hour * 60 + lt.tm_min;
    return true;
}

bool times(time_t when, float latitude, float longitude, int& sunriseMin, int& sunsetMin) {
    struct tm day;
    localtime_r(&when, &day);
    return localMinutes(day, latitude, longitude, true, sunriseMin) &&
           localMinutes(day, latitude, longitude, false, sunsetMin);
}

}  // namespace sun
