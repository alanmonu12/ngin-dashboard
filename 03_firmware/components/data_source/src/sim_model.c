#include "sim_model.h"

#include <math.h>

#include "vehicle_boost.h"

/* Valores ilustrativos, no medidos en el Forte GT. */
const float SIM_MODEL_RATIO[VEHICLE_GEAR_COUNT] = {120.0f, 70.0f, 48.0f, 37.0f, 30.0f, 25.0f};
static const float ACCEL[VEHICLE_GEAR_COUNT] = {11.5f, 8.0f, 5.4f, 4.0f, 2.9f, 2.2f}; /* km/h por s */

#define IDLE_RPM           800.0f
#define IDLE_S             4.0f
#define LAUNCH_RPM         3800.0f
#define SHIFT_RPM          6200.0f
#define SHIFT_S            0.35f
#define TOP_KMH            170.0f
#define DECEL_KMH_S        7.0f
#define BARO_KPA           78.0f /* CDMX */
#define VACUUM_KPA         32.0f
#define MAX_BOOST_BAR      1.2f
#define MAP_TAU_S          0.3f
#define SPOOL_START_RPM    1800.0f /* el turbo empieza a dar boost */
#define SPOOL_SPAN_RPM     1600.0f /* rango hasta el boost máximo */
#define SHIFT_COAST_KMH_S  1.0f    /* pérdida de velocidad con el clutch pisado */
#define SHIFT_RPM_TAU_S    (SHIFT_S / 3.0f)
#define SHIFT_MAP_DROP_KPA 20.0f   /* MAP al soltar el acelerador en el cambio, bajo el barómetro */
#define DOWNSHIFT_RPM      1500.0f /* al frenar, baja de marcha debajo de estas RPM */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void set_phase(sim_model_t *m, sim_phase_t p)
{
    m->phase = p;
    m->phase_s = 0.0f;
}

static float boost_target_kpa(const sim_model_t *m)
{
    float spool = clampf((m->rpm - SPOOL_START_RPM) / SPOOL_SPAN_RPM, 0.0f, 1.0f);
    return m->baro_kpa + MAX_BOOST_BAR * spool * VEHICLE_KPA_PER_BAR;
}

void sim_model_init(sim_model_t *m)
{
    *m = (sim_model_t){
        .phase = SIM_IDLE,
        .gear = 1,
        .rpm = IDLE_RPM,
        .map_kpa = VACUUM_KPA,
        .baro_kpa = BARO_KPA,
    };
}

void sim_model_step(sim_model_t *m, float dt_s)
{
    float map_target = VACUUM_KPA;
    m->phase_s += dt_s;

    switch (m->phase) {
    case SIM_IDLE:
        m->speed_kmh = 0.0f;
        m->rpm = IDLE_RPM;
        if (m->phase_s >= IDLE_S) {
            m->gear = 1;
            set_phase(m, SIM_LAUNCH);
        }
        break;

    case SIM_LAUNCH:
        m->speed_kmh += ACCEL[0] * dt_s;
        m->rpm = LAUNCH_RPM;
        map_target = boost_target_kpa(m);
        if (SIM_MODEL_RATIO[0] * m->speed_kmh >= LAUNCH_RPM) {
            set_phase(m, SIM_PULL);
        }
        break;

    case SIM_PULL:
        m->speed_kmh += ACCEL[m->gear - 1] * dt_s;
        m->rpm = SIM_MODEL_RATIO[m->gear - 1] * m->speed_kmh;
        map_target = boost_target_kpa(m);
        if (m->speed_kmh >= TOP_KMH) {
            set_phase(m, SIM_DECEL);
        } else if (m->rpm >= SHIFT_RPM && m->gear < VEHICLE_GEAR_COUNT) {
            set_phase(m, SIM_SHIFT);
        }
        break;

    case SIM_SHIFT: {
        float next = SIM_MODEL_RATIO[m->gear] * m->speed_kmh;
        m->speed_kmh -= SHIFT_COAST_KMH_S * dt_s;
        m->rpm += (next - m->rpm) * clampf(dt_s / SHIFT_RPM_TAU_S, 0.0f, 1.0f);
        map_target = m->baro_kpa - SHIFT_MAP_DROP_KPA;
        if (m->phase_s >= SHIFT_S) {
            m->gear++;
            set_phase(m, SIM_PULL);
        }
        break;
    }

    case SIM_DECEL:
        m->speed_kmh -= DECEL_KMH_S * dt_s;
        if (m->speed_kmh <= 0.0f) {
            m->speed_kmh = 0.0f;
            set_phase(m, SIM_IDLE);
            break;
        }
        while (m->gear > 1 && SIM_MODEL_RATIO[m->gear - 1] * m->speed_kmh < DOWNSHIFT_RPM) {
            m->gear--;
        }
        m->rpm = SIM_MODEL_RATIO[m->gear - 1] * m->speed_kmh;
        if (m->rpm < IDLE_RPM) {
            m->rpm = IDLE_RPM; /* clutch pisado al final del frenado */
        }
        break;
    }

    m->map_kpa += (map_target - m->map_kpa) * clampf(dt_s / MAP_TAU_S, 0.0f, 1.0f);
}

vehicle_sample_t sim_model_sample(const sim_model_t *m, uint32_t t_ms, uint32_t tick)
{
    vehicle_sample_t s = {
        .t_ms = t_ms,
        .fields = VEHICLE_HAS_RPM | VEHICLE_HAS_SPEED,
        .rpm = floorf(m->rpm * 4.0f) / 4.0f,
        .speed_kmh = floorf(m->speed_kmh),
    };
    if (tick % SIM_MODEL_MAP_EVERY_TICKS == 0) {
        s.fields |= VEHICLE_HAS_MAP;
        s.map_kpa = roundf(m->map_kpa);
    }
    if (tick % SIM_MODEL_BARO_EVERY_TICKS == 0) {
        s.fields |= VEHICLE_HAS_BARO;
        s.baro_kpa = m->baro_kpa;
    }
    return s;
}
