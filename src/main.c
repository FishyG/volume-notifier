#include <getopt.h>

#include "pipewire/pipewire.h"
#include "spa/utils/type-info.h"

#include "../include/app.h"
#include "../include/volume_listener.h"

int main(int argc, char* argv[]) {
    state_t state;

    pw_init(&argc, &argv);
    spa_zero(state);

    SPA_TYPE_ROOT;

    struct option options[] = {
        {"uwu", no_argument, &state.uwu, 1},
        {"kawaii", no_argument, &state.kawaii, 1},
        {NULL, 0, NULL, 0}
    };

    while (getopt_long(argc, argv, "", options, NULL) != -1);

    start_listener(&state);

    return 0;
}
