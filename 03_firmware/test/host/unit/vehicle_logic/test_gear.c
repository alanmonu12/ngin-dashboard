#include "unity_fixture.h"

#include "vehicle_gear.h"

static const vehicle_gear_cfg_t cfg = {
    .ratio = {120.0f, 70.0f, 48.0f, 37.0f, 30.0f, 25.0f},
    .tolerance = 0.05f,
    .hysteresis = 0.02f,
    .min_speed_kmh = 10.0f,
};

TEST_GROUP(gear);
TEST_SETUP(gear) {}
TEST_TEAR_DOWN(gear) {}

/* Verifica: SW-201 */
TEST(gear, detects_each_gear)
{
    for (int g = 1; g <= VEHICLE_GEAR_COUNT; g++) {
        float speed = 50.0f;
        TEST_ASSERT_EQUAL_INT(g,
                              vehicle_gear_detect(&cfg, cfg.ratio[g - 1] * speed, speed, VEHICLE_GEAR_NONE));
    }
}

/* Verifica: SW-202 */
TEST(gear, below_min_speed_is_none)
{
    /* SYS-14: debajo de 10 km/h no se muestra marcha */
    TEST_ASSERT_EQUAL_INT(VEHICLE_GEAR_NONE, vehicle_gear_detect(&cfg, 1200.0f, 9.0f, VEHICLE_GEAR_NONE));
}

/* Verifica: SW-203 */
TEST(gear, out_of_tolerance_is_none)
{
    /* SYS-15: clutch pisado, la razón no coincide con ninguna marcha */
    TEST_ASSERT_EQUAL_INT(VEHICLE_GEAR_NONE, vehicle_gear_detect(&cfg, 900.0f, 60.0f, VEHICLE_GEAR_NONE));
}

/* Verifica: SW-204 */
TEST(gear, hysteresis_keeps_previous_gear)
{
    /* 6 % arriba de 3.ª: fuera de la tolerancia, dentro de tolerancia + histéresis */
    float speed = 50.0f, rpm = 48.0f * 1.06f * speed;
    TEST_ASSERT_EQUAL_INT(VEHICLE_GEAR_NONE, vehicle_gear_detect(&cfg, rpm, speed, VEHICLE_GEAR_NONE));
    TEST_ASSERT_EQUAL_INT(3, vehicle_gear_detect(&cfg, rpm, speed, 3));
}

/* Verifica: SW-205 */
TEST(gear, uncalibrated_gear_is_skipped)
{
    vehicle_gear_cfg_t c = cfg;
    c.ratio[2] = 0.0f;
    TEST_ASSERT_EQUAL_INT(VEHICLE_GEAR_NONE,
                          vehicle_gear_detect(&c, 48.0f * 50.0f, 50.0f, VEHICLE_GEAR_NONE));
}

/* Verifica: SW-207 */
TEST(gear, new_gear_needs_tolerance_not_hysteresis)
{
    /* La histéresis solo conserva la marcha mostrada (3.ª): la 4.ª entra con 5 %, no con 7 %. */
    float speed = 50.0f;
    TEST_ASSERT_EQUAL_INT(4, vehicle_gear_detect(&cfg, 37.0f * 1.04f * speed, speed, 3));
    TEST_ASSERT_EQUAL_INT(VEHICLE_GEAR_NONE, vehicle_gear_detect(&cfg, 37.0f * 1.06f * speed, speed, 3));
}

TEST_GROUP_RUNNER(gear)
{
    RUN_TEST_CASE(gear, detects_each_gear);
    RUN_TEST_CASE(gear, below_min_speed_is_none);
    RUN_TEST_CASE(gear, out_of_tolerance_is_none);
    RUN_TEST_CASE(gear, hysteresis_keeps_previous_gear);
    RUN_TEST_CASE(gear, uncalibrated_gear_is_skipped);
    RUN_TEST_CASE(gear, new_gear_needs_tolerance_not_hysteresis);
}
