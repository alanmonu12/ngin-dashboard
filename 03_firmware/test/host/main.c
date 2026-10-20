#include "unity_fixture.h"

static void run_all(void)
{
    /* unit/ */
    RUN_TEST_GROUP(boost);
    RUN_TEST_GROUP(gear);
    RUN_TEST_GROUP(shift);
    RUN_TEST_GROUP(z2h);
    RUN_TEST_GROUP(vehicle);

    /* integration/ */
    RUN_TEST_GROUP(sim_vehicle);
}

int main(int argc, const char *argv[])
{
    return UnityMain(argc, argv, run_all);
}
