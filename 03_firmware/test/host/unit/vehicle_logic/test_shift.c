#include "unity_fixture.h"

#include "vehicle_shift.h"

static const vehicle_shift_cfg_t cfg = {
    .latency_ms = 100.0f,
    .rate_alpha = 1.0f, /* sin suavizado: la tasa es exacta en las pruebas */
};

static vehicle_shift_t s;

TEST_GROUP(shift);

TEST_SETUP(shift)
{
    vehicle_shift_reset(&s);
}

TEST_TEAR_DOWN(shift) {}

TEST(shift, bar_is_proportional)
{
    /* SYS-18: barra de RPM_Inicio_Barra a RPM_Cambio */
    vehicle_shift_out_t o = vehicle_shift_update(&s, &cfg, 0, 4250.0f, 2500.0f, 6000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, o.bar_fraction);
    o = vehicle_shift_update(&s, &cfg, 1000, 2000.0f, 2500.0f, 6000.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, o.bar_fraction);
    o = vehicle_shift_update(&s, &cfg, 2000, 7000.0f, 2500.0f, 6000.0f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, o.bar_fraction);
}

TEST(shift, alarm_at_shift_rpm_when_steady)
{
    vehicle_shift_out_t o = vehicle_shift_update(&s, &cfg, 0, 5990.0f, 2500.0f, 6000.0f);
    TEST_ASSERT_FALSE(o.alarm);
    o = vehicle_shift_update(&s, &cfg, 1000, 5990.0f, 2500.0f, 6000.0f);
    TEST_ASSERT_FALSE(o.alarm);
    o = vehicle_shift_update(&s, &cfg, 2000, 6000.0f, 2500.0f, 6000.0f);
    TEST_ASSERT_TRUE(o.alarm);
}

TEST(shift, alarm_leads_when_rpm_rises_fast)
{
    /* 3000 rpm/s × 0.1 s = 300 rpm de adelanto: a 5750 ya avisa */
    vehicle_shift_update(&s, &cfg, 0, 5600.0f, 2500.0f, 6000.0f);
    vehicle_shift_out_t o = vehicle_shift_update(&s, &cfg, 50, 5750.0f, 2500.0f, 6000.0f);
    TEST_ASSERT_TRUE(o.alarm);
}

TEST(shift, no_lead_when_rpm_falls)
{
    vehicle_shift_update(&s, &cfg, 0, 6500.0f, 2500.0f, 6000.0f);
    vehicle_shift_out_t o = vehicle_shift_update(&s, &cfg, 50, 5900.0f, 2500.0f, 6000.0f);
    TEST_ASSERT_FALSE(o.alarm);
}

TEST_GROUP_RUNNER(shift)
{
    RUN_TEST_CASE(shift, bar_is_proportional);
    RUN_TEST_CASE(shift, alarm_at_shift_rpm_when_steady);
    RUN_TEST_CASE(shift, alarm_leads_when_rpm_rises_fast);
    RUN_TEST_CASE(shift, no_lead_when_rpm_falls);
}
