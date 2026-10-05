#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "../src/fd_bb_slbb.h"

struct InstanceMeta {
    char name[32];
    int n;
    int m;
    uint32_t seed;
    uint16_t ub;
    uint16_t lb;
};

static bool read_taillard(const std::string &path, uint16_t p[MAX_N][MAX_M], InstanceMeta &meta) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) lines.push_back(line);

    bool got_meta = false, got_size = false;
    for (const std::string &raw : lines) {
        std::string s = raw;
        if (!s.empty() && s[0] == '#') {
            unsigned seed = 0, ub = 0, lb = 0;
            int n = 0, m = 0;
            if (std::sscanf(s.c_str(), "# %d %d %u %u %u", &n, &m, &seed, &ub, &lb) == 5) {
                meta.n=n; meta.m=m; meta.seed=seed; meta.ub=(uint16_t)ub; meta.lb=(uint16_t)lb; got_meta=true;
            }
        }
    }
    std::vector<int> nums;
    bool size_line=false;
    for (const std::string &raw : lines) {
        std::string s=raw;
        if (!s.empty() && s[0]=='#') continue;
        std::istringstream iss(s);
        int a,b;
        if (!size_line && (iss>>a>>b)) {
            if (a>0 && a<=MAX_N && b>0 && b<=MAX_M) {
                if (a==meta.n && b==meta.m) { size_line=true; continue; }
            }
        }
        if (size_line) {
            int v;
            while (iss>>v) nums.push_back(v);
        }
    }
    if (!got_meta || !size_line || (int)nums.size() < meta.n*meta.m) return false;
    int idx=0;
    // Taillard/PISCO files are machine-major: m rows x n jobs.
    for (int k=0;k<meta.m;++k)
        for (int j=0;j<meta.n;++j)
            p[j][k]=(uint16_t)nums[idx++];
    return true;
}

static void zero_matrix(uint16_t p[MAX_N][MAX_M]) {
    for (int j=0;j<MAX_N;++j) for (int k=0;k<MAX_M;++k) p[j][k]=0;
}

static void write_header(FILE *f) {
    std::fprintf(f,
      "instance,n,m,initial_ub,locked_max_parents_popped,cnt_parents_popped,cnt_cand_evaluated,cnt_lb_evaluated,cnt_direction_probes,cnt_pruned_pre_commit,cnt_states_committed,peak_stack_depth,max_c_fwd,max_c_bwd,max_lb1,max_observed_time_val,max_sum_lb1_dir,cnt_bram_state_wr_words,total_cycles_model,complete_flag,overflow_flag\n");
}

static int run_one(const std::string &path, FILE *csv) {
    uint16_t p[MAX_N][MAX_M]; zero_matrix(p);
    InstanceMeta meta{};
    { std::string::size_type pos = path.find_last_of("/\\"); std::string fn = (pos == std::string::npos) ? path : path.substr(pos + 1); std::string::size_type dot = fn.find_last_of("."); if (dot != std::string::npos) fn = fn.substr(0, dot); std::snprintf(meta.name, sizeof(meta.name), "%s", fn.c_str()); }
    if (!read_taillard(path,p,meta)) {
        std::fprintf(stderr,"[ERROR] Cannot parse %s\n", path.c_str());
        return 2;
    }
    Telemetry t{};
    fd_bb_slbb(p, meta.n, meta.m, meta.ub, 50, &t);
    std::fprintf(csv,
      "%s,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%u,%u\n",
      meta.name, meta.n, meta.m, meta.ub, 50,
      t.cnt_parents_popped, t.cnt_cand_evaluated, t.cnt_lb_evaluated,
      t.cnt_direction_probes, t.cnt_pruned_pre_commit, t.cnt_states_committed,
      t.peak_stack_depth, t.max_c_fwd, t.max_c_bwd, t.max_lb1,
      t.max_observed_time_val, t.max_sum_lb1_dir,
      (unsigned long long)t.cnt_bram_state_wr_words,
      (unsigned long long)t.total_cycles_model,
      t.complete_flag, t.overflow_flag);
    return 0;
}

int main(int argc, char **argv) {
    const char *env_root = std::getenv("FD_BB_DATA_ROOT");
    const char *env_out  = std::getenv("FD_BB_CSV_OUT");
    const char *env_all  = std::getenv("FD_BB_RUN_ALL");
    std::string root = (argc > 1)
    ? argv[1]
    : (env_root ? env_root
                : "data/taillard");
    std::string out  = (argc>2) ? argv[2] : (env_out ? env_out : "results/rtl/host_csim.csv");
    bool all = true;
    std::string smoke = root + "/ta001.txt";
    FILE *csv = std::fopen(out.c_str(), "w");
    if (!csv) { std::perror(out.c_str()); return 3; }
    write_header(csv);
    if (!all) {
        int rc=run_one(smoke,csv);
        std::fclose(csv);
        return rc;
    }
    for (int i=1;i<=120;++i) {
        char name[32]; std::sprintf(name,"ta%03d.txt",i);
        std::string path=root+"/"+name;
        int rc=run_one(path,csv);
        if (rc) { std::fclose(csv); return rc; }
        std::fprintf(stderr,"[DONE] %s\n",name);
    }
    std::fclose(csv);
    return 0;
}
