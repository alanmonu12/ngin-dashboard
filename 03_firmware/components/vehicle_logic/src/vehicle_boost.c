#include "vehicle_boost.h"

#include <math.h>

typedef struct {
    float per_bar;
    int decimals; /* resolución de SYS-02/SYS-17: 0.01 bar, 0.1 psi, 1 kPa */
    const char *name;
} unit_info_t;

static const unit_info_t UNITS[VEHICLE_UNIT_COUNT] = {
    [VEHICLE_UNIT_BAR] = {1.0f, 2, "bar"},
    [VEHICLE_UNIT_PSI] = {14.5038f, 1, "psi"},
    [VEHICLE_UNIT_KPA] = {VEHICLE_KPA_PER_BAR, 0, "kPa"},
};

static const unit_info_t *unit_info(vehicle_unit_t unit)
{
    return &UNITS[unit < VEHICLE_UNIT_COUNT ? unit : VEHICLE_UNIT_BAR];
}

float vehicle_boost_bar(float map_kpa, float baro_kpa)
{
    return fmaxf((map_kpa - baro_kpa) / VEHICLE_KPA_PER_BAR, 0.0f);
}

float vehicle_pressure_from_bar(float bar, vehicle_unit_t unit)
{
    return bar * unit_info(unit)->per_bar;
}

int vehicle_pressure_decimals(vehicle_unit_t unit)
{
    return unit_info(unit)->decimals;
}

const char *vehicle_unit_name(vehicle_unit_t unit)
{
    return unit_info(unit)->name;
}
