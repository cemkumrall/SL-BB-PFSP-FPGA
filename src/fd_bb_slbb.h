#pragma once
#include <stdint.h>

static const int MAX_N = 500;
static const int MAX_M = 20;
static const int MAX_STACK = 2048;
static const int MASK_WORDS = 8; // 512 bits
static const int FULL_WORDS = 19; // 152 B / 64-bit logical words

struct NodeState {
    uint16_t c_fwd[MAX_M];
    uint16_t c_bwd[MAX_M];
    uint64_t job_mask[MASK_WORDS];
    uint16_t depth;
    uint16_t lb;
    uint16_t parent_ptr;
    uint16_t token;
};
static_assert(sizeof(NodeState) == 152, "SL-BB full persistent state must remain 152 bytes");

struct Telemetry {
    uint32_t cnt_parents_popped;
    uint32_t cnt_cand_evaluated;
    uint32_t cnt_lb_evaluated;
    uint32_t cnt_direction_probes;
    uint32_t cnt_pruned_pre_commit;
    uint32_t cnt_states_committed;
    uint32_t peak_stack_depth;
    uint16_t max_c_fwd;
    uint16_t max_c_bwd;
    uint16_t max_lb1;
    uint16_t max_observed_time_val;
    uint32_t max_sum_lb1_dir;
    uint64_t cnt_bram_state_wr_words;
    uint64_t total_cycles_model;
    uint8_t complete_flag;
    uint8_t overflow_flag;
};

void fd_bb_slbb(
    const uint16_t p[MAX_N][MAX_M],
    int n,
    int m,
    uint16_t initial_ub,
    uint32_t max_parents_popped,
    Telemetry *out);
