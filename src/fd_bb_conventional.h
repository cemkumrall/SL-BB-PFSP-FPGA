#pragma once
// Repository compatibility header for the Conventional B&B baseline.
// The baseline uses the same locked constants, NodeState, and Telemetry
// definitions as the SL-BB implementations; only the top-level function name differs.
#include "../../fd_bb_slbb.h"

void fd_bb_conventional(
    const uint16_t p[MAX_N][MAX_M],
    int n,
    int m,
    uint16_t initial_ub,
    uint32_t max_parents_popped,
    Telemetry *out);
