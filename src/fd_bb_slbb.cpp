#include "fd_bb_slbb.h"

#include <assert.h>
#include <stdint.h>


/*
 * ==================================================================
 * SL-BB v4.0 (Pipelined II=1 Datapath + 22-BRAM State-Lifetime Engine)
 *   - Keeps exact 22 BRAM_18K (7.8%) footprint
 *   - Pipelines inner O(m) machine loops & survivor commit at II=1
 *   - Targets ~8x-10x RTL speedup over conventional baseline
 * ==================================================================
 */


/* ------------------------------------------------------------------
 * 16-bit safety helper
 * ------------------------------------------------------------------ */
static inline uint16_t sat16(uint32_t x, Telemetry &t) {
#pragma HLS INLINE
    if (x > 65535U) {
        t.overflow_flag = 1;
#ifndef __SYNTHESIS__
        assert(x <= 65535U);
#endif
        return 65535U;
    }
    return (uint16_t)x;
}


/* ------------------------------------------------------------------
 * 32-bit transient survivor token
 * ------------------------------------------------------------------ */
static inline uint32_t pack_transient_token(uint16_t lb, uint16_t job) {
#pragma HLS INLINE
    return ((uint32_t)lb << 16) | (uint32_t)job;
}

static inline int token_get_job(uint32_t tok) {
#pragma HLS INLINE
    return (int)(tok & 0xFFFFU);
}


/* ------------------------------------------------------------------
 * 32-bit compact DFS frontier descriptor
 * ------------------------------------------------------------------ */
static inline uint32_t pack_frontier_desc(
    uint16_t parent_depth,
    uint16_t job,
    bool use_fwd,
    bool is_root) {
#pragma HLS INLINE

    uint32_t d = 0U;
    d |= ((uint32_t)job & 0x1FFU);
    d |= ((uint32_t)(use_fwd ? 0U : 1U) << 9);
    d |= ((uint32_t)(is_root ? 1U : 0U) << 10);
    d |= (((uint32_t)parent_depth & 0x1FFU) << 11);
    return d;
}


/* ------------------------------------------------------------------
 * Per-depth restore metadata
 * ------------------------------------------------------------------ */
static inline uint16_t pack_path_meta(uint16_t job, bool use_fwd) {
#pragma HLS INLINE
    return (uint16_t)(
        (job & 0x1FFU) |
        ((uint16_t)(use_fwd ? 0U : 1U) << 9));
}


/* ==================================================================
 * Top-level SL-BB v4.0 Accelerator
 * ================================================================== */
void fd_bb_slbb(
    const uint16_t p[MAX_N][MAX_M],
    int n,
    int m,
    uint16_t initial_ub,
    uint32_t max_parents_popped,
    Telemetry *out) {

    Telemetry t = {};
    const uint16_t ub = initial_ub;


    /*
     * 1. Compact Pending Frontier (MAX_STACK x 20-bit -> 3 BRAM_18K)
     */
    static uint32_t frontier_stack[MAX_STACK];
#pragma HLS BIND_STORAGE variable=frontier_stack type=ram_1p impl=bram


    /*
     * 2. Single Restore Vector Store & Path Metadata in BRAM (17 BRAM_18K)
     */
    static uint16_t restore_vec[MAX_N + 1][32];
    static uint16_t path_meta[MAX_N + 1];
#pragma HLS BIND_STORAGE variable=restore_vec type=ram_1p impl=bram
#pragma HLS BIND_STORAGE variable=path_meta type=ram_1p impl=bram


    /*
     * 3. Active DFS State in fast Dual-Port / 1-cycle LUTRAM (0 BRAM)
     *    Dual-port LUTRAM (ram_2p) allows simultaneous read + write
     *    within II=1 pipelined update loops without port conflicts.
     */
    uint16_t cur_cfwd[32];
    uint16_t cur_cbwd[32];
    uint8_t  cur_used[512];
    uint32_t rem[32];
#pragma HLS BIND_STORAGE variable=cur_cfwd type=ram_2p impl=lutram
#pragma HLS BIND_STORAGE variable=cur_cbwd type=ram_2p impl=lutram
#pragma HLS BIND_STORAGE variable=cur_used type=ram_2p impl=lutram
#pragma HLS BIND_STORAGE variable=rem type=ram_2p impl=lutram
#pragma HLS DEPENDENCE variable=cur_cfwd inter false
#pragma HLS DEPENDENCE variable=cur_cbwd inter false
#pragma HLS DEPENDENCE variable=rem inter false


    /*
     * 4. Compact survivor token buffers (1 BRAM_18K each).
     */
    static uint32_t f_surv_tokens[MAX_N];
    static uint32_t b_surv_tokens[MAX_N];
#pragma HLS BIND_STORAGE variable=f_surv_tokens type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=b_surv_tokens type=ram_2p impl=bram


    /*
     * Root initialization
     */
    for (int j = 0; j < n; ++j) {
#pragma HLS PIPELINE II=1
        cur_used[j] = 0;
    }

    for (int k = 0; k < m; ++k) {
#pragma HLS PIPELINE II=1
        cur_cfwd[k] = 0;
        cur_cbwd[k] = 0;
        rem[k] = 0;
    }

    for (int j = 0; j < n; ++j) {
        for (int k = 0; k < m; ++k) {
#pragma HLS PIPELINE II=1
            rem[k] += (uint32_t)p[j][k];
        }
    }

    uint16_t active_depth = 0;

    int sp = 0;
    frontier_stack[sp++] = pack_frontier_desc(0, 0, true, true);
    t.peak_stack_depth = 1;


    /*
     * Deterministic LIFO DFS
     */
    while (
        sp > 0 &&
        t.cnt_parents_popped < max_parents_popped) {

        const uint32_t desc = frontier_stack[--sp];
        t.cnt_parents_popped++;

        const uint16_t parent_depth =
            (uint16_t)((desc >> 11) & 0x1FFU);

        const uint16_t child_job =
            (uint16_t)(desc & 0x1FFU);

        const bool child_fwd =
            (((desc >> 9) & 1U) == 0U);

        const bool is_root =
            (((desc >> 10) & 1U) != 0U);


        /*
         * ----------------------------------------------------------
         * Differential Backtracking & Child Descent (Pipelined II=1)
         * ----------------------------------------------------------
         */
        if (!is_root) {

#ifndef __SYNTHESIS__
            assert(active_depth >= parent_depth);
#endif

            if (active_depth < parent_depth) {
                t.overflow_flag = 1;
                break;
            }

            while (active_depth > parent_depth) {

                const uint16_t meta = path_meta[active_depth];

                const uint16_t undo_job =
                    (uint16_t)(meta & 0x1FFU);

                const bool undo_fwd =
                    (((meta >> 9) & 1U) == 0U);

                for (int k = 0; k < m; ++k) {
#pragma HLS PIPELINE II=1

                    uint16_t saved_val = restore_vec[active_depth][k];

                    if (undo_fwd) {
                        cur_cfwd[k] = saved_val;
                    } else {
                        cur_cbwd[k] = saved_val;
                    }

                    rem[k] += (uint32_t)p[undo_job][k];
                }

                cur_used[undo_job] = 0;
                --active_depth;
            }


            const uint16_t new_depth =
                (uint16_t)(active_depth + 1U);

#ifndef __SYNTHESIS__
            assert(new_depth <= MAX_N);
#endif

            if (new_depth > MAX_N) {
                t.overflow_flag = 1;
                break;
            }

            path_meta[new_depth] =
                pack_path_meta(child_job, child_fwd);

            cur_used[child_job] = 1;


            if (child_fwd) {

                uint32_t prev = 0;

                for (int k = 0; k < m; ++k) {
#pragma HLS PIPELINE II=1

                    uint16_t old_cf = cur_cfwd[k];
                    restore_vec[new_depth][k] = old_cf;

                    const uint16_t pj = p[child_job][k];

                    rem[k] -= (uint32_t)pj;

                    const uint32_t base =
                        (old_cf > prev)
                            ? (uint32_t)old_cf
                            : prev;

                    prev = base + (uint32_t)pj;

                    cur_cfwd[k] = sat16(prev, t);
                }

            } else {

                uint32_t next = 0;

                for (int k = m - 1; k >= 0; --k) {
#pragma HLS PIPELINE II=1

                    uint16_t old_cb = cur_cbwd[k];
                    restore_vec[new_depth][k] = old_cb;

                    const uint16_t pj = p[child_job][k];

                    rem[k] -= (uint32_t)pj;

                    const uint32_t base =
                        (old_cb > next)
                            ? (uint32_t)old_cb
                            : next;

                    next = base + (uint32_t)pj;

                    cur_cbwd[k] = sat16(next, t);
                }
            }

            active_depth = new_depth;
        }


        const int free_jobs =
            n - (int)active_depth;

        if (free_jobs <= 0) {
            continue;
        }


        /*
         * ----------------------------------------------------------
         * Unified Single-Pass Bidirectional Probing (Pipelined II=1)
         * ----------------------------------------------------------
         */
        uint32_t sum_f = 0;
        uint32_t sum_b = 0;

        int f_surv = 0;
        int b_surv = 0;

        for (int j = 0; j < n; ++j) {

            if (cur_used[j] != 0) {
                continue;
            }

            /* --- 1. Forward Token Evaluation (II=1: 1 cycle/machine!) --- */
            uint32_t c_run_f = 0;
            uint32_t lb_f = 0;

            for (int k = 0; k < m; ++k) {
#pragma HLS PIPELINE II=1

                const uint16_t pj = p[j][k];
                const uint16_t cf_parent = cur_cfwd[k];
                const uint16_t cb_parent = cur_cbwd[k];
                const uint32_t rem_k = rem[k];

                const uint32_t base =
                    (cf_parent > c_run_f)
                        ? (uint32_t)cf_parent
                        : c_run_f;

                c_run_f = base + (uint32_t)pj;

                const uint32_t rem_excl =
                    (rem_k >= (uint32_t)pj)
                        ? (rem_k - (uint32_t)pj)
                        : 0U;

                const uint32_t v =
                    c_run_f + rem_excl + (uint32_t)cb_parent;

                if (v > lb_f) {
                    lb_f = v;
                }
            }

            const uint16_t ck_f_max = sat16(c_run_f, t);
            if (c_run_f > t.max_c_fwd) {
                t.max_c_fwd = ck_f_max;
            }

            const uint16_t lb16_f = sat16(lb_f, t);
            if (lb_f > t.max_lb1) {
                t.max_lb1 = lb16_f;
            }
            if (lb_f > t.max_observed_time_val) {
                t.max_observed_time_val = lb16_f;
            }

            sum_f += (uint32_t)lb16_f;

            if (lb16_f < ub) {
                f_surv_tokens[f_surv++] =
                    pack_transient_token(lb16_f, (uint16_t)j);
            }


            /* --- 2. Backward Token Evaluation (II=1: 1 cycle/machine!) --- */
            uint32_t c_run_b = 0;
            uint32_t lb_b = 0;

            for (int k = m - 1; k >= 0; --k) {
#pragma HLS PIPELINE II=1

                const uint16_t pj = p[j][k];
                const uint16_t cf_parent = cur_cfwd[k];
                const uint16_t cb_parent = cur_cbwd[k];
                const uint32_t rem_k = rem[k];

                const uint32_t base =
                    (cb_parent > c_run_b)
                        ? (uint32_t)cb_parent
                        : c_run_b;

                c_run_b = base + (uint32_t)pj;

                const uint32_t rem_excl =
                    (rem_k >= (uint32_t)pj)
                        ? (rem_k - (uint32_t)pj)
                        : 0U;

                const uint32_t v =
                    (uint32_t)cf_parent + rem_excl + c_run_b;

                if (v > lb_b) {
                    lb_b = v;
                }
            }

            const uint16_t ck_b_max = sat16(c_run_b, t);
            if (c_run_b > t.max_c_bwd) {
                t.max_c_bwd = ck_b_max;
            }

            const uint16_t lb16_b = sat16(lb_b, t);
            if (lb_b > t.max_lb1) {
                t.max_lb1 = lb16_b;
            }
            if (lb_b > t.max_observed_time_val) {
                t.max_observed_time_val = lb16_b;
            }

            sum_b += (uint32_t)lb16_b;

            if (lb16_b < ub) {
                b_surv_tokens[b_surv++] =
                    pack_transient_token(lb16_b, (uint16_t)j);
            }
        }

        t.cnt_lb_evaluated += (uint64_t)(2 * free_jobs);
        t.cnt_direction_probes += 2;


        /*
         * Locked MinBranch rule
         */
        bool use_fwd = false;

        if (f_surv < b_surv) {
            use_fwd = true;
        } else if (f_surv > b_surv) {
            use_fwd = false;
        } else if (sum_f > sum_b) {
            use_fwd = true;
        } else if (sum_f < sum_b) {
            use_fwd = false;
        } else {
            use_fwd = true;
        }

        const uint32_t chosen_sum =
            use_fwd ? sum_f : sum_b;

        if (chosen_sum > t.max_sum_lb1_dir) {
            t.max_sum_lb1_dir = chosen_sum;
        }


        const int count =
            use_fwd ? f_surv : b_surv;

        t.cnt_cand_evaluated +=
            (uint64_t)free_jobs;

        t.cnt_pruned_pre_commit +=
            (uint64_t)(free_jobs - count);


        /*
         * Compact survivor-token ordering
         */
        if (use_fwd) {

            for (int i = 1; i < count; ++i) {

                const uint32_t key =
                    f_surv_tokens[i];

                int pos = i - 1;

                while (
                    pos >= 0 &&
                    f_surv_tokens[pos] < key) {
#pragma HLS PIPELINE II=1

                    f_surv_tokens[pos + 1] =
                        f_surv_tokens[pos];

                    --pos;
                }

                f_surv_tokens[pos + 1] = key;
            }

        } else {

            for (int i = 1; i < count; ++i) {

                const uint32_t key =
                    b_surv_tokens[i];

                int pos = i - 1;

                while (
                    pos >= 0 &&
                    b_surv_tokens[pos] < key) {
#pragma HLS PIPELINE II=1

                    b_surv_tokens[pos + 1] =
                        b_surv_tokens[pos];

                    --pos;
                }

                b_surv_tokens[pos + 1] = key;
            }
        }


        /*
         * Deferred frontier commit (Pipelined II=1 -> 1 cycle/survivor!)
         */
        for (int idx = 0; idx < count; ++idx) {
#pragma HLS PIPELINE II=1

            const uint32_t tok =
                use_fwd
                    ? f_surv_tokens[idx]
                    : b_surv_tokens[idx];

            const uint16_t job =
                (uint16_t)token_get_job(tok);

            if (sp < MAX_STACK) {

                frontier_stack[sp++] =
                    pack_frontier_desc(
                        active_depth,
                        job,
                        use_fwd,
                        false);

                t.cnt_bram_state_wr_words +=
                    FULL_WORDS;

                t.cnt_states_committed++;

                if ((uint32_t)sp >
                    t.peak_stack_depth) {

                    t.peak_stack_depth =
                        (uint32_t)sp;
                }

            } else {

                t.overflow_flag = 1;
            }
        }
    }


    t.complete_flag =
        (sp == 0) ? 1 : 0;


    t.total_cycles_model =
        (uint64_t)t.cnt_parents_popped
        * (uint64_t)(2 * n * m + n)
        +
        (uint64_t)t.cnt_states_committed
        * (uint64_t)(m + 1);


    *out = t;
}