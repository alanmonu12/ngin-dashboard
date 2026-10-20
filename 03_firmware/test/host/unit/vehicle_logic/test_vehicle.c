#include "unity_fixture.h"

#include "vehicle.h"

static vehicle_t v;

static const vehicle_cfg_t cfg = {
    .gear =
        {
            .ratio = {120.0f, 70.0f, 48.0f, 37.0f, 30.0f, 25.0f},
            .tolerance = 0.05f,
            .hysteresis = 0.02f,
            .min_speed_kmh = 10.0f,
        },
    .shift = {.latency_ms = 0.0f, .rate_alpha = 0.5f},
    .z2h = {.arm_ms = 1000, .timeout_ms = 30000, .target_kmh = 100.0f},
    .shift_rpm = {5500.0f, 6000.0f, 6200.0f, 6200.0f, 6200.0f, 6200.0f},
    .bar_start_rpm = 2500.0f,
};

TEST_GROUP(vehicle);

TEST_SETUP(vehicle)
{
    vehicle_init(&v, &cfg);
}

TEST_TEAR_DOWN(vehicle) {}

TEST(vehicle, boost_uses_last_baro_and_tracks_peak)
{
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 0, .fields = VEHICLE_HAS_BARO, .baro_kpa = 78.0f});
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 100, .fields = VEHICLE_HAS_MAP, .map_kpa = 178.0f});
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 200, .fields = VEHICLE_HAS_MAP, .map_kpa = 128.0f});
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.50f, v.out.boost_bar);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.00f, v.out.boost_peak_bar);
}

TEST(vehicle, shift_rpm_follows_the_gear)
{
    /* 1.ª cambia a 5500, 2.ª a 6000: 5800 rpm alarma en 1.ª pero no en 2.ª */
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 0,
                                              .fields = VEHICLE_HAS_RPM | VEHICLE_HAS_SPEED,
                                              .rpm = 5800.0f,
                                              .speed_kmh = 5800.0f / 120.0f});
    TEST_ASSERT_EQUAL_INT(1, v.out.gear);
    TEST_ASSERT_TRUE(v.out.shift_alarm);

    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 1000,
                                              .fields = VEHICLE_HAS_RPM | VEHICLE_HAS_SPEED,
                                              .rpm = 5800.0f,
                                              .speed_kmh = 5800.0f / 70.0f});
    TEST_ASSERT_EQUAL_INT(2, v.out.gear);
    TEST_ASSERT_FALSE(v.out.shift_alarm);
}

TEST(vehicle, fields_seen_accumulates)
{
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 0, .fields = VEHICLE_HAS_RPM, .rpm = 900.0f});
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 10, .fields = VEHICLE_HAS_MAP, .map_kpa = 30.0f});
    TEST_ASSERT_EQUAL_UINT32(VEHICLE_HAS_RPM | VEHICLE_HAS_MAP, v.fields_seen);
}

TEST(vehicle, baro_before_any_map_shows_no_boost)
{
    /* Sin MAP todavía, el valor inicial de map_kpa no debe dar un boost ni un pico falsos. */
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 0, .fields = VEHICLE_HAS_BARO, .baro_kpa = 78.0f});
    TEST_ASSERT_EQUAL_FLOAT(0.0f, v.out.boost_bar);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, v.out.boost_peak_bar);
}

TEST(vehicle, gear_from_separate_rpm_and_speed_samples)
{
    /* OBD entrega 0x0C y 0x0D en respuestas separadas. */
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 0, .fields = VEHICLE_HAS_RPM, .rpm = 3500.0f});
    TEST_ASSERT_EQUAL_INT(VEHICLE_GEAR_NONE, v.out.gear);
    vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = 50, .fields = VEHICLE_HAS_SPEED, .speed_kmh = 50.0f});
    TEST_ASSERT_EQUAL_INT(2, v.out.gear); /* 3500 / 50 = 70: razón de 2.ª */
}

TEST(vehicle, last_z2h_result_survives_rearm)
{
    /* Detenido, armado, 0 a 100 en 5 s y otra vez detenido más de lo que tarda en volver a armar. */
    uint32_t t = 0;
    for (; t <= 1500; t += 100) {
        vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = t, .fields = VEHICLE_HAS_SPEED, .speed_kmh = 0.0f});
    }
    for (float kmh = 2.0f; kmh <= 100.0f; kmh += 2.0f) {
        t += 100;
        vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = t, .fields = VEHICLE_HAS_SPEED, .speed_kmh = kmh});
    }
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_DONE, v.out.z2h_state);
    uint32_t result = v.out.z2h_ms;
    for (uint32_t i = 0; i < 20; i++) {
        t += 100;
        vehicle_on_sample(&v, &(vehicle_sample_t){.t_ms = t, .fields = VEHICLE_HAS_SPEED, .speed_kmh = 0.0f});
    }
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_ARMED, v.out.z2h_state);
    TEST_ASSERT_EQUAL_UINT32(result, v.out.z2h_ms);
}

TEST_GROUP_RUNNER(vehicle)
{
    RUN_TEST_CASE(vehicle, boost_uses_last_baro_and_tracks_peak);
    RUN_TEST_CASE(vehicle, shift_rpm_follows_the_gear);
    RUN_TEST_CASE(vehicle, fields_seen_accumulates);
    RUN_TEST_CASE(vehicle, baro_before_any_map_shows_no_boost);
    RUN_TEST_CASE(vehicle, gear_from_separate_rpm_and_speed_samples);
    RUN_TEST_CASE(vehicle, last_z2h_result_survives_rearm);
}
