/*
 * Integración: el modelo del simulador alimenta a vehicle_logic igual que
 * sim_source en la tarjeta (mismos ritmos y cuantización), sin FreeRTOS.
 */
#include <math.h>

#include "unity_fixture.h"

#include "sim_model.h"
#include "vehicle.h"

#define TICK_MS SIM_MODEL_TICK_MS

/* RPM_Cambio debajo del punto de cambio del modelo (6200 rpm): debe avisar antes de cada cambio. */
#define TEST_SHIFT_RPM     6000.0f
#define TEST_BAR_START_RPM 2500.0f

static vehicle_t v;
static sim_model_t m;

TEST_GROUP(sim_vehicle);

TEST_SETUP(sim_vehicle)
{
    /* Ajustes por defecto del firmware: si cambian, esta prueba los cubre. */
    vehicle_cfg_t cfg;
    vehicle_cfg_set_defaults(&cfg);
    cfg.bar_start_rpm = TEST_BAR_START_RPM;
    for (int g = 0; g < VEHICLE_GEAR_COUNT; g++) {
        cfg.gear.ratio[g] = SIM_MODEL_RATIO[g];
        cfg.shift_rpm[g] = TEST_SHIFT_RPM;
    }
    vehicle_init(&v, &cfg);
    sim_model_init(&m);
}

TEST_TEAR_DOWN(sim_vehicle) {}

/* Un paso del modelo convertido en muestra con la misma función que usa sim_source.c. */
static void feed(uint32_t t_ms, uint32_t tick)
{
    sim_model_step(&m, TICK_MS / 1000.0f);
    vehicle_sample_t s = sim_model_sample(&m, t_ms, tick);
    vehicle_on_sample(&v, &s);
}

TEST(sim_vehicle, full_pull_reports_gears_in_order_and_0_100_close_to_truth)
{
    int last_gear = VEHICLE_GEAR_NONE, max_gear = 0, gear_drops = 0, alarm_edges = 0;
    bool last_alarm = false;
    float last_speed = 0.0f;
    float launch_ms = -1.0f, truth_ms = -1.0f;
    uint32_t t = 0;

    for (uint32_t tick = 0; tick < 60000 / TICK_MS && v.out.z2h_state != VEHICLE_Z2H_DONE; tick++) {
        t = tick * TICK_MS;
        feed(t, tick);

        /* Verdad del modelo: de la última muestra a 0 al cruce de 100, interpolado. */
        if (launch_ms < 0.0f && m.speed_kmh > 0.0f) {
            launch_ms = (float)(t - TICK_MS);
        }
        if (truth_ms < 0.0f && last_speed < 100.0f && m.speed_kmh >= 100.0f) {
            float frac = (100.0f - last_speed) / (m.speed_kmh - last_speed);
            truth_ms = (float)(t - TICK_MS) + frac * TICK_MS - launch_ms;
        }
        last_speed = m.speed_kmh;

        int g = v.out.gear;
        if (g != VEHICLE_GEAR_NONE) {
            if (g < max_gear) {
                gear_drops++;
            }
            if (g > max_gear) {
                max_gear = g;
            }
            last_gear = g;
        }
        if (v.out.shift_alarm && !last_alarm) {
            alarm_edges++;
        }
        last_alarm = v.out.shift_alarm;
    }

    TEST_ASSERT_EQUAL(VEHICLE_Z2H_DONE, v.out.z2h_state);
    TEST_ASSERT_TRUE(truth_ms > 0.0f);
    TEST_ASSERT_INT_WITHIN(200, (int)lroundf(truth_ms), (int)v.out.z2h_ms); /* SYS-08 */

    /* Hasta 100 km/h el modelo pasa por 1.ª, 2.ª y 3.ª, sin regresar de marcha. */
    TEST_ASSERT_EQUAL_INT(3, max_gear);
    TEST_ASSERT_EQUAL_INT(3, last_gear);
    TEST_ASSERT_EQUAL_INT(0, gear_drops);
    /* El modelo cambia a 6200 rpm y la RPM_Cambio es 6000: un aviso antes de cada cambio. */
    TEST_ASSERT_EQUAL_INT(2, alarm_edges);
    TEST_ASSERT_TRUE(v.out.boost_peak_bar > 0.5f);
}

TEST_GROUP_RUNNER(sim_vehicle)
{
    RUN_TEST_CASE(sim_vehicle, full_pull_reports_gears_in_order_and_0_100_close_to_truth);
}
