#include <math.h>

#include "unity_fixture.h"

#include "vehicle_z2h.h"

static const vehicle_z2h_cfg_t cfg = {
    .arm_ms = 1000,
    .timeout_ms = 30000,
    .target_kmh = 100.0f,
};

static vehicle_z2h_t z;

TEST_GROUP(z2h);

TEST_SETUP(z2h)
{
    vehicle_z2h_reset(&z);
}

TEST_TEAR_DOWN(z2h) {}

/* Velocidad como la entrega el PID 0x0D: entera, truncada. */
static float obd_speed(float exact_kmh)
{
    return exact_kmh <= 0.0f ? 0.0f : floorf(exact_kmh);
}

/*
 * Simula una aceleración constante que arranca en launch_ms y llega a
 * 100 km/h en run_ms, muestreada cada period_ms con un desfase phase_ms.
 */
static vehicle_z2h_state_t run_launch(uint32_t launch_ms, uint32_t run_ms, uint32_t period_ms,
                                      uint32_t phase_ms)
{
    float accel = 100.0f / (float)run_ms;
    vehicle_z2h_state_t st = VEHICLE_Z2H_IDLE;
    for (uint32_t t = phase_ms; t < launch_ms + run_ms + 2000; t += period_ms) {
        float exact = t > launch_ms ? accel * (float)(t - launch_ms) : 0.0f;
        st = vehicle_z2h_update(&z, &cfg, t, obd_speed(exact));
        if (st == VEHICLE_Z2H_DONE) {
            break;
        }
    }
    return st;
}

/* Verifica: SW-401 */
TEST(z2h, needs_to_be_stopped_to_arm)
{
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_IDLE, vehicle_z2h_update(&z, &cfg, 0, 0.0f));
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_IDLE, vehicle_z2h_update(&z, &cfg, 500, 0.0f));
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_ARMED, vehicle_z2h_update(&z, &cfg, 1000, 0.0f));
}

/* Verifica: SW-401 */
TEST(z2h, rolling_start_does_not_arm)
{
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_IDLE, vehicle_z2h_update(&z, &cfg, 0, 20.0f));
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_IDLE, vehicle_z2h_update(&z, &cfg, 2000, 100.0f));
}

/* Verifica: SW-403, SW-404, SW-405 */
TEST(z2h, measures_within_0_2_s_at_10_hz)
{
    /* SYS-08: ±0.2 s con muestreo de 10 Hz y resolución de 1 km/h, con varios desfases */
    for (uint32_t phase = 0; phase < 100; phase += 10) {
        vehicle_z2h_reset(&z);
        TEST_ASSERT_EQUAL(VEHICLE_Z2H_DONE, run_launch(3000, 6500, 100, phase));
        TEST_ASSERT_INT_WITHIN(200, 6500, z.result_ms);
    }
}

/* Verifica: SW-402, SW-406 */
TEST(z2h, stopping_aborts)
{
    vehicle_z2h_update(&z, &cfg, 0, 0.0f);
    vehicle_z2h_update(&z, &cfg, 1000, 0.0f);
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_RUNNING, vehicle_z2h_update(&z, &cfg, 1100, 2.0f));
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_RUNNING, vehicle_z2h_update(&z, &cfg, 1200, 5.0f));
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_ABORTED, vehicle_z2h_update(&z, &cfg, 1300, 0.0f));
}

/* Verifica: SW-401 */
TEST(z2h, rearms_after_a_run)
{
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_DONE, run_launch(1500, 7000, 100, 0));
    uint32_t t = 20000;
    vehicle_z2h_update(&z, &cfg, t, 0.0f);
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_ARMED, vehicle_z2h_update(&z, &cfg, t + 1000, 0.0f));
}

/* Verifica: SW-408 */
TEST(z2h, measures_across_uint32_wrap_and_long_uptime)
{
    /* ~49.7 días encendido: el contador de ms da la vuelta a mitad de la medición. */
    const uint32_t base = UINT32_MAX - 3000u;
    const uint32_t launch_after_ms = 1500, run_ms = 6500, period_ms = 100;
    const float accel = 100.0f / (float)run_ms;
    vehicle_z2h_state_t st = VEHICLE_Z2H_IDLE;
    for (uint32_t i = 0; i * period_ms < launch_after_ms + run_ms + 2000 && st != VEHICLE_Z2H_DONE; i++) {
        uint32_t since = i * period_ms;
        float exact = since > launch_after_ms ? accel * (float)(since - launch_after_ms) : 0.0f;
        st = vehicle_z2h_update(&z, &cfg, base + since, obd_speed(exact));
    }
    TEST_ASSERT_EQUAL(VEHICLE_Z2H_DONE, st);
    TEST_ASSERT_INT_WITHIN(200, run_ms, z.result_ms);
}

TEST_GROUP_RUNNER(z2h)
{
    RUN_TEST_CASE(z2h, measures_across_uint32_wrap_and_long_uptime);
    RUN_TEST_CASE(z2h, needs_to_be_stopped_to_arm);
    RUN_TEST_CASE(z2h, rolling_start_does_not_arm);
    RUN_TEST_CASE(z2h, measures_within_0_2_s_at_10_hz);
    RUN_TEST_CASE(z2h, stopping_aborts);
    RUN_TEST_CASE(z2h, rearms_after_a_run);
}
