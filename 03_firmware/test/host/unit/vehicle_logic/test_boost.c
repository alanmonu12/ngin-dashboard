#include "unity_fixture.h"

#include "vehicle_boost.h"

TEST_GROUP(boost);
TEST_SETUP(boost) {}
TEST_TEAR_DOWN(boost) {}

/* Verifica: SW-101 */
TEST(boost, is_map_minus_baro)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.00f, vehicle_boost_bar(178.0f, 78.0f));
}

/* Verifica: SW-102 */
TEST(boost, vacuum_shows_zero)
{
    /* SYS-21: sin escala de vacío */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, vehicle_boost_bar(40.0f, 78.0f));
}

/* Verifica: SW-103 */
TEST(boost, unit_conversion)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.5038f, vehicle_pressure_from_bar(1.0f, VEHICLE_UNIT_PSI));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, vehicle_pressure_from_bar(1.0f, VEHICLE_UNIT_KPA));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, vehicle_pressure_from_bar(1.0f, VEHICLE_UNIT_BAR));
}

/* Verifica: SW-104 */
TEST(boost, resolution_per_unit)
{
    /* SYS-02 y SYS-17: 0.01 bar, 0.1 psi, 1 kPa */
    TEST_ASSERT_EQUAL_INT(2, vehicle_pressure_decimals(VEHICLE_UNIT_BAR));
    TEST_ASSERT_EQUAL_INT(1, vehicle_pressure_decimals(VEHICLE_UNIT_PSI));
    TEST_ASSERT_EQUAL_INT(0, vehicle_pressure_decimals(VEHICLE_UNIT_KPA));
}

TEST_GROUP_RUNNER(boost)
{
    RUN_TEST_CASE(boost, is_map_minus_baro);
    RUN_TEST_CASE(boost, vacuum_shows_zero);
    RUN_TEST_CASE(boost, unit_conversion);
    RUN_TEST_CASE(boost, resolution_per_unit);
}
