#include "fd_bb_conventional.h"

#include <assert.h>
#include <stdint.h>


static inline bool job_used(const NodeState &s, int j) {
    return ((s.job_mask[j >> 6] >> (j & 63)) & 1ULL) != 0ULL;
}


static inline void set_job(NodeState &s, int j) {
    s.job_mask[j >> 6] |= (1ULL << (j & 63));
}


static inline int count_free_jobs(const NodeState &s, int n) {
    int c = 0;

    for (int j = 0; j < n; ++j) {
        if (!job_used(s, j)) {
            ++c;
        }
    }

    return c;
}


/*
 * Convert a 32-bit intermediate value to the locked 16-bit representation.
 *
 * The PFSP timing/LB datapath is intentionally kept at uint16_t.
 * During C-simulation, an observed overflow is treated as an assertion
 * failure so that an unsafe bit-width cannot silently propagate into
 * the profiling or hardware baseline.
 *
 * During synthesis, the assertion is excluded.
 */
static inline uint16_t sat16(uint32_t x, Telemetry &t) {

    if (x > 65535U) {

        t.overflow_flag = 1;

#ifndef __SYNTHESIS__
        assert(x <= 65535U);
#endif

        return 65535U;
    }

    return (uint16_t)x;
}


/*
 * Forward child LB1 evaluation on an already materialized child state.
 *
 * child.c_fwd[k] already contains p[job][k].
 *
 * The remaining unscheduled workload excluding candidate job j is:
 *
 *     R_p[k] - p[j][k]
 *
 * Therefore the machine-wise LB1 candidate value is:
 *
 *     C_fwd_child[k]
 *     + (R_p[k] - p[j][k])
 *     + C_bwd_parent[k]
 *
 * The maximum over all machines is used as LB1.
 */
static uint16_t eval_lb1_forward(
    const NodeState &child,
    const uint16_t p[MAX_N][MAX_M],
    const uint32_t rem[MAX_M],
    int m,
    int job,
    Telemetry &t) {

    uint32_t lb = 0;

    for (int k = 0; k < m; ++k) {

        uint32_t rem_excl =
            (rem[k] >= p[job][k])
                ? (rem[k] - p[job][k])
                : 0U;

        uint32_t v =
            (uint32_t)child.c_fwd[k]
            + rem_excl
            + (uint32_t)child.c_bwd[k];

        if (v > lb) {
            lb = v;
        }
    }

    if (lb > t.max_lb1) {
        t.max_lb1 = sat16(lb, t);
    }

    if (lb > t.max_observed_time_val) {
        t.max_observed_time_val = sat16(lb, t);
    }

    return sat16(lb, t);
}


/*
 * Backward child LB1 evaluation on an already materialized child state.
 *
 * child.c_bwd[k] already contains p[job][k].
 *
 * The corresponding machine-wise LB1 value is:
 *
 *     C_fwd_parent[k]
 *     + (R_p[k] - p[j][k])
 *     + C_bwd_child[k]
 *
 * Again, the maximum over all machines defines LB1.
 */
static uint16_t eval_lb1_backward(
    const NodeState &child,
    const uint16_t p[MAX_N][MAX_M],
    const uint32_t rem[MAX_M],
    int m,
    int job,
    Telemetry &t) {

    uint32_t lb = 0;

    for (int k = 0; k < m; ++k) {

        uint32_t rem_excl =
            (rem[k] >= p[job][k])
                ? (rem[k] - p[job][k])
                : 0U;

        uint32_t v =
            (uint32_t)child.c_fwd[k]
            + rem_excl
            + (uint32_t)child.c_bwd[k];

        if (v > lb) {
            lb = v;
        }
    }

    if (lb > t.max_lb1) {
        t.max_lb1 = sat16(lb, t);
    }

    if (lb > t.max_observed_time_val) {
        t.max_observed_time_val = sat16(lb, t);
    }

    return sat16(lb, t);
}


/*
 * Materialize a forward child state.
 *
 * Conventional baseline behavior:
 * the complete child state is constructed before the pruning
 * decision is committed.
 */
static void make_child_forward(
    NodeState &child,
    const NodeState &parent,
    const uint16_t p[MAX_N][MAX_M],
    int m,
    int job,
    Telemetry &t) {

    child = parent;

    set_job(child, job);

    child.depth = parent.depth + 1;

    /*
     * Child token:
     * bit [8:0]  : job index
     * bit [9]     : direction = 0 (Forward)
     */
    child.token =
        (uint16_t)((job & 0x1FF) | (0u << 9));

    uint32_t prev = 0;

    for (int k = 0; k < m; ++k) {

        uint32_t v =
            child.c_fwd[k] > prev
                ? child.c_fwd[k]
                : prev;

        prev = v + p[job][k];

        child.c_fwd[k] = sat16(prev, t);

        if (prev > t.max_c_fwd) {
            t.max_c_fwd = sat16(prev, t);
        }

        if (prev > t.max_observed_time_val) {
            t.max_observed_time_val = sat16(prev, t);
        }
    }
}


/*
 * Materialize a backward child state.
 *
 * Conventional baseline behavior:
 * the complete child state is constructed before the pruning
 * decision is committed.
 */
static void make_child_backward(
    NodeState &child,
    const NodeState &parent,
    const uint16_t p[MAX_N][MAX_M],
    int m,
    int job,
    Telemetry &t) {

    child = parent;

    set_job(child, job);

    child.depth = parent.depth + 1;

    /*
     * Child token:
     * bit [8:0]  : job index
     * bit [9]     : direction = 1 (Backward)
     */
    child.token =
        (uint16_t)((job & 0x1FF) | (1u << 9));

    uint32_t next = 0;

    for (int k = m - 1; k >= 0; --k) {

        uint32_t v =
            child.c_bwd[k] > next
                ? child.c_bwd[k]
                : next;

        next = v + p[job][k];

        child.c_bwd[k] = sat16(next, t);

        if (next > t.max_c_bwd) {
            t.max_c_bwd = sat16(next, t);
        }

        if (next > t.max_observed_time_val) {
            t.max_observed_time_val = sat16(next, t);
        }
    }
}


/*
 * Conventional FPGA baseline
 *
 * Search semantics:
 *
 *   - same LB1 formulation
 *   - Forward / Backward direction probing
 *   - MinBranch direction selection
 *   - deterministic tie breaking
 *   - LB1 < UB survivor criterion
 *   - descending-LB survivor ordering
 *   - descending-job-index tie breaking
 *   - LIFO DFS stack
 *
 * Architectural characteristic of this baseline:
 *
 *   Every generated child is first materialized as a full
 *   152-byte NodeState before the pruning result is committed.
 *
 * This is intentionally different from the proposed FD-BB
 * architecture, where child-specific information is represented
 * by a compact transient delta and full state is committed only
 * for survivors.
 */
void fd_bb_conventional(
    const uint16_t p[MAX_N][MAX_M],
    int n,
    int m,
    uint16_t initial_ub,
    uint32_t max_parents_popped,
    Telemetry *out) {

    Telemetry t = {};

    uint16_t ub = initial_ub;


    /*
     * Main LIFO search stack.
     *
     * static storage is used so that Vitis HLS can infer the
     * structure as hardware memory rather than a software
     * function-frame object.
     */
    static NodeState stack[MAX_STACK];

#pragma HLS BIND_STORAGE variable=stack type=ram_1p impl=bram


    /*
     * Conventional eager candidate buffer.
     *
     * Each generated candidate is materialized as a complete
     * NodeState before the pruning decision is committed.
     *
     * NodeState = 152 B = 19 x 64-bit words.
     *
     * The corresponding state-write counter below is a LOGICAL
     * state-traffic model. Actual physical BRAM mapping and
     * write behavior are verified later with Vitis HLS/Vivado.
     */
    static NodeState cand_buf[MAX_N];

#pragma HLS BIND_STORAGE variable=cand_buf type=ram_1p impl=bram


    int sp = 0;


    /*
     * Root node.
     */
    NodeState root = {};

    root.depth = 0;
    root.lb = 0;
    root.parent_ptr = 0;
    root.token = 0;

    stack[sp++] = root;

    t.peak_stack_depth = 1;


    /*
     * Main deterministic LIFO DFS loop.
     */
    while (
        sp > 0 &&
        t.cnt_parents_popped < max_parents_popped) {

        /*
         * Parent slot before pop.
         *
         * ParentPtr is an active-stack/frontier slot index,
         * not a global B&B node identifier.
         */
        uint16_t parent_slot =
            (uint16_t)(sp - 1);

        NodeState parent =
            stack[--sp];

        t.cnt_parents_popped++;


        /*
         * Determine the number of unscheduled jobs.
         */
        int free_jobs =
            count_free_jobs(parent, n);

        if (free_jobs <= 0) {
            continue;
        }


        /*
         * Direction-specific LB arrays.
         *
         * Only MAX_N entries are required because at most N jobs
         * can remain unscheduled.
         */
        uint16_t f_lb[MAX_N];
        uint16_t b_lb[MAX_N];


        /*
         * 32-bit accumulators are intentionally used for
         * MinBranch tie-breaking.
         *
         * For N <= 500 and LB1 <= 65,535:
         *
         *     500 x 65,535 < 2^25
         *
         * therefore uint32_t is sufficient.
         */
        uint32_t sum_f = 0;
        uint32_t sum_b = 0;


        int f_surv = 0;
        int b_surv = 0;


        /*
         * Parent residual workload vector.
         *
         * R_p[k] contains the total processing workload of all
         * jobs that are not yet scheduled on machine k.
         *
         * It is computed once per parent and reused by all
         * candidate LB1 evaluations.
         */
        uint32_t rem[MAX_M];

        for (int k = 0; k < m; ++k) {

            uint32_t r = 0;

            for (int j = 0; j < n; ++j) {

                if (!job_used(parent, j)) {
                    r += p[j][k];
                }
            }

            rem[k] = r;
        }


        /*
         * ------------------------------------------------------
         * Direction probe: FORWARD
         * ------------------------------------------------------
         *
         * Jobs are enumerated in deterministic increasing index
         * order.
         */
        for (int j = 0; j < n; ++j) {

            if (job_used(parent, j)) {
                continue;
            }


            NodeState child_f;


            /*
             * Full child materialization.
             */
            make_child_forward(
                child_f,
                parent,
                p,
                m,
                j,
                t);


            /*
             * Evaluate LB1 for the fully materialized child.
             */
            uint16_t lb =
                eval_lb1_forward(
                    child_f,
                    p,
                    rem,
                    m,
                    j,
                    t);


            t.cnt_lb_evaluated++;

            f_lb[j] = lb;


            /*
             * Primary survivor criterion:
             *
             *     LB1 < UB
             */
            if (lb < ub) {
                ++f_surv;
            }

            sum_f += lb;
        }


        /*
         * ------------------------------------------------------
         * Direction probe: BACKWARD
         * ------------------------------------------------------
         */
        for (int j = 0; j < n; ++j) {

            if (job_used(parent, j)) {
                continue;
            }


            NodeState child_b;


            /*
             * Full child materialization.
             */
            make_child_backward(
                child_b,
                parent,
                p,
                m,
                j,
                t);


            /*
             * Evaluate LB1 for the fully materialized child.
             */
            uint16_t lb =
                eval_lb1_backward(
                    child_b,
                    p,
                    rem,
                    m,
                    j,
                    t);


            t.cnt_lb_evaluated++;

            b_lb[j] = lb;


            /*
             * Primary survivor criterion:
             *
             *     LB1 < UB
             */
            if (lb < ub) {
                ++b_surv;
            }

            sum_b += lb;
        }


        /*
         * Two directions were evaluated.
         */
        t.cnt_direction_probes += 2;


        /*
         * ------------------------------------------------------
         * MinBranch direction selection
         * ------------------------------------------------------
         *
         * Primary rule:
         *     choose direction with fewer survivors.
         *
         * Tie 1:
         *     choose direction with larger sum of LB1 values.
         *
         * Tie 2:
         *     deterministic Forward fallback.
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

            /*
             * Deterministic final fallback:
             * Forward.
             */
            use_fwd = true;
        }


        uint32_t chosen_sum =
            use_fwd ? sum_f : sum_b;


        if (chosen_sum > t.max_sum_lb1_dir) {
            t.max_sum_lb1_dir =
                chosen_sum;
        }


        /*
         * ------------------------------------------------------
         * Conventional Eager Materialization Pass
         * ------------------------------------------------------
         *
         * Every unscheduled job in the selected direction is
         * materialized as a complete 152-byte NodeState BEFORE
         * the pruning decision is committed.
         *
         * This is the defining architectural behavior of the
         * conventional baseline.
         */
        int jobs_buf[MAX_N];

        int count = 0;


        for (int j = 0; j < n; ++j) {

            if (job_used(parent, j)) {
                continue;
            }


            NodeState child;


            if (use_fwd) {

                make_child_forward(
                    child,
                    parent,
                    p,
                    m,
                    j,
                    t);

            } else {

                make_child_backward(
                    child,
                    parent,
                    p,
                    m,
                    j,
                    t);
            }


            /*
             * Reuse the LB1 result already obtained during
             * the direction probe.
             */
            child.lb =
                use_fwd
                    ? f_lb[j]
                    : b_lb[j];


            child.parent_ptr =
                parent_slot;


            /*
             * --------------------------------------------------
             * FULL STATE WRITE
             * --------------------------------------------------
             *
             * Conventional baseline:
             *
             *     generated child
             *          ↓
             *     full state materialization
             *          ↓
             *     state-memory write
             *          ↓
             *     pruning decision
             *
             * The counter represents the logical 152-B state
             * write model:
             *
             *     152 B = 19 x 64-bit words
             *
             * Actual physical BRAM implementation is measured
             * later through HLS/Vivado reports.
             */
            cand_buf[j] = child;

            t.cnt_cand_evaluated++;

            t.cnt_bram_state_wr_words +=
                FULL_WORDS;


            /*
             * --------------------------------------------------
             * PRUNE OR KEEP
             * --------------------------------------------------
             */
            if (child.lb >= ub) {

                /*
                 * Candidate is transient/pruned.
                 *
                 * It was nevertheless already materialized and
                 * written into cand_buf.
                 */
                t.cnt_pruned_pre_commit++;

            } else {

                /*
                 * Candidate survives and will later be committed
                 * to the DFS stack.
                 */
                jobs_buf[count++] = j;
            }
        }


        /*
         * ------------------------------------------------------
         * Deterministic survivor ordering
         * ------------------------------------------------------
         *
         * Descending LB1.
         *
         * For equal LB1:
         * descending job index.
         *
         * Since the stack is LIFO, this makes the next popped
         * survivor deterministic.
         */
        for (int a = 0; a < count; ++a) {

            for (int b = a + 1; b < count; ++b) {

                int ja = jobs_buf[a];
                int jb = jobs_buf[b];


                uint16_t la =
                    cand_buf[ja].lb;

                uint16_t lb =
                    cand_buf[jb].lb;


                if (
                    la < lb ||
                    (la == lb && ja < jb)) {

                    int tmp =
                        jobs_buf[a];

                    jobs_buf[a] =
                        jobs_buf[b];

                    jobs_buf[b] =
                        tmp;
                }
            }
        }


        /*
         * ------------------------------------------------------
         * Commit surviving full states to LIFO stack
         * ------------------------------------------------------
         */
        for (int idx = 0; idx < count; ++idx) {

            int j =
                jobs_buf[idx];


            if (sp < MAX_STACK) {

                stack[sp++] =
                    cand_buf[j];


                t.cnt_states_committed++;


                if ((uint32_t)sp >
                    t.peak_stack_depth) {

                    t.peak_stack_depth =
                        (uint32_t)sp;
                }

            } else {

                /*
                 * Active-stack capacity exceeded.
                 */
                t.overflow_flag = 1;
            }
        }
    }


    /*
     * Search completion status.
     *
     * If the stack is empty, the selected search prefix has been
     * completely exhausted.
     *
     * If max_parents_popped was reached while the stack still
     * contains states, the run is intentionally incomplete.
     */
    t.complete_flag =
        (sp == 0) ? 1 : 0;


    /*
     * ----------------------------------------------------------
     * Analytical/software cycle model
     * ----------------------------------------------------------
     *
     * IMPORTANT:
     *
     * This is NOT the final FPGA cycle measurement.
     *
     * Actual FPGA performance is obtained from Vitis HLS/Vivado
     * scheduling, implementation and hardware telemetry.
     *
     * This value is retained only as an analytical model for
     * software-side profiling.
     */
    t.total_cycles_model =
        (uint64_t)t.cnt_parents_popped
        *
        (uint64_t)(2 * n * m + n)
        +
        (uint64_t)t.cnt_cand_evaluated
        *
        (uint64_t)(m + 1);


    /*
     * Return telemetry.
     */
    *out = t;
}