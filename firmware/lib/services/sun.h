#pragma once
#include <time.h>

namespace sun {

// Sunrise/sunset for the local calendar day containing `when`, as minutes
// after local midnight (uses the TZ configured with setenv/tzset).
// Returns false in polar day/night.
bool times(time_t when, float latitude, float longitude, int& sunriseMin, int& sunsetMin);

}  // namespace sun
