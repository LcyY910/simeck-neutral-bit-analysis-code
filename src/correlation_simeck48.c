#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MASK24 0xffffffu
#ifndef KEY_COUNT
#define KEY_COUNT 32
#endif
#ifndef SAMPLE_LOG2
#define SAMPLE_LOG2 24
#endif
#ifndef TEST_ROUNDS
#define TEST_ROUNDS 17
#endif
#ifndef TEST_DP_L
#define TEST_DP_L 0x000100u
#endif
#ifndef TEST_DP_R
#define TEST_DP_R 0x000200u
#endif
#ifndef TEST_LC_L
#define TEST_LC_L 0x000200u
#endif
#ifndef TEST_LC_R
#define TEST_LC_R 0x000710u
#endif
#ifndef BASE_SEED
#define BASE_SEED 0x48c0decafef00d00ull
#endif

typedef struct {
    double corr;
    double abs_corr;
    double unbiased_sq;
    double control_corr;
    double control_abs_corr;
    double control_unbiased_sq;
} key_result_t;

static inline uint32_t rol24(uint32_t x, unsigned n) {
    return ((x << n) | (x >> (24 - n))) & MASK24;
}

static inline uint32_t f24(uint32_t x) {
    return ((x & rol24(x, 5)) ^ rol24(x, 1)) & MASK24;
}

static inline void round24(uint32_t *l, uint32_t *r, uint32_t key) {
    uint32_t old_l = *l;
    *l = (*r ^ f24(*l) ^ key) & MASK24;
    *r = old_l;
}

static void expand_simeck48_96(const uint32_t master[4], uint32_t round_keys[36]) {
    uint32_t keys[4] = {master[0], master[1], master[2], master[3]};
    uint32_t sequence = 0x9a42bb1fu;
    for (int i = 0; i < 36; i++) {
        round_keys[i] = keys[0];
        uint32_t constant = 0xfffffcu | (sequence & 1u);
        sequence >>= 1;

        uint32_t old_k1 = keys[1];
        keys[1] = (keys[0] ^ f24(keys[1]) ^ constant) & MASK24;
        keys[0] = old_k1;

        uint32_t generated = keys[1];
        keys[1] = keys[2];
        keys[2] = keys[3];
        keys[3] = generated;
    }
}

static inline void encrypt24(uint32_t *l, uint32_t *r,
                             const uint32_t *round_keys, int rounds) {
    for (int i = 0; i < rounds; i++) round24(l, r, round_keys[i]);
}

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static inline unsigned parity48(uint32_t l, uint32_t r,
                                uint32_t mask_l, uint32_t mask_r) {
    return (__builtin_parity(l & mask_l) ^ __builtin_parity(r & mask_r)) & 1u;
}

static void summarize(const char *label, const key_result_t *results, int control) {
    double mean_corr = 0.0, mean_abs = 0.0, mean_sq = 0.0;
    for (int i = 0; i < KEY_COUNT; i++) {
        if (control) {
            mean_corr += results[i].control_corr;
            mean_abs += results[i].control_abs_corr;
            mean_sq += results[i].control_unbiased_sq;
        } else {
            mean_corr += results[i].corr;
            mean_abs += results[i].abs_corr;
            mean_sq += results[i].unbiased_sq;
        }
    }
    mean_corr /= KEY_COUNT;
    mean_abs /= KEY_COUNT;
    mean_sq /= KEY_COUNT;

    double variance = 0.0;
    for (int i = 0; i < KEY_COUNT; i++) {
        double value = control ? results[i].control_unbiased_sq : results[i].unbiased_sq;
        double d = value - mean_sq;
        variance += d * d;
    }
    variance /= (KEY_COUNT - 1);
    double se = sqrt(variance / KEY_COUNT);
    double rms = mean_sq > 0.0 ? sqrt(mean_sq) : 0.0;
    double abs_exp = mean_abs > 0.0 ? -log2(mean_abs) : INFINITY;
    double rms_exp = rms > 0.0 ? -log2(rms) : INFINITY;
    double z = se > 0.0 ? mean_sq / se : 0.0;

    printf("%s mean_signed=% .10e mean_abs=%.10e abs_exp=%.6f "
           "mean_unbiased_sq=% .10e sq_se=%.10e z=%.4f rms=%.10e rms_exp=%.6f\n",
           label, mean_corr, mean_abs, abs_exp, mean_sq, se, z, rms, rms_exp);
}

int main(void) {
    const int rounds = TEST_ROUNDS;
    const uint32_t dp_l = TEST_DP_L, dp_r = TEST_DP_R;
    const uint32_t lc_l = TEST_LC_L, lc_r = TEST_LC_R;
    const uint32_t ctl_l = TEST_LC_L ^ 1u, ctl_r = TEST_LC_R ^ 1u;
    const uint64_t samples = 1ull << SAMPLE_LOG2;
    key_result_t *results = calloc(KEY_COUNT, sizeof(*results));
    if (!results) return 2;

    uint32_t test_master[4] = {0x020100, 0x0a0908, 0x121110, 0x1a1918};
    uint32_t test_rk[36];
    uint32_t test_l = 0x726963, test_r = 0x20646e;
    expand_simeck48_96(test_master, test_rk);
    encrypt24(&test_l, &test_r, test_rk, 36);
    printf("VECTOR48 l=%06x r=%06x expected=f3cf25:e33b36 status=%s\n",
           test_l, test_r,
           (test_l == 0xf3cf25 && test_r == 0xe33b36) ? "PASS" : "FAIL");

    omp_set_num_threads(4);
    double start = omp_get_wtime();
    printf("CONFIG rounds=%d keys=%d samples_per_key=2^%d total_samples=2^%.6f "
           "dp=%06x:%06x lc=%06x:%06x control=%06x:%06x seed=0x%016llx threads=%d\n",
           rounds, KEY_COUNT, SAMPLE_LOG2, log2((double)KEY_COUNT * samples),
           dp_l, dp_r, lc_l, lc_r, ctl_l, ctl_r,
           (unsigned long long)BASE_SEED, omp_get_max_threads());

    #pragma omp parallel for schedule(static)
    for (int key_index = 0; key_index < KEY_COUNT; key_index++) {
        uint64_t rng = BASE_SEED ^
                       ((uint64_t)key_index * 0x9e3779b97f4a7c15ull);
        uint32_t master[4] = {
            (uint32_t)splitmix64(&rng) & MASK24,
            (uint32_t)splitmix64(&rng) & MASK24,
            (uint32_t)splitmix64(&rng) & MASK24,
            (uint32_t)splitmix64(&rng) & MASK24
        };
        uint32_t rk[36];
        expand_simeck48_96(master, rk);
        int64_t sum = 0, control_sum = 0;

        for (uint64_t i = 0; i < samples; i++) {
            uint32_t p1l = (uint32_t)splitmix64(&rng) & MASK24;
            uint32_t p1r = (uint32_t)splitmix64(&rng) & MASK24;
            uint32_t p2l = p1l ^ dp_l;
            uint32_t p2r = p1r ^ dp_r;
            uint32_t c1l = p1l, c1r = p1r, c2l = p2l, c2r = p2r;
            encrypt24(&c1l, &c1r, rk, rounds);
            encrypt24(&c2l, &c2r, rk, rounds);

            unsigned bit = parity48(c1l ^ c2l, c1r ^ c2r, lc_l, lc_r);
            unsigned ctl = parity48(c1l ^ c2l, c1r ^ c2r, ctl_l, ctl_r);
            sum += bit ? -1 : 1;
            control_sum += ctl ? -1 : 1;
        }

        double n = (double)samples;
        double corr = (double)sum / n;
        double control_corr = (double)control_sum / n;
        results[key_index].corr = corr;
        results[key_index].abs_corr = fabs(corr);
        results[key_index].unbiased_sq =
            (((double)sum * (double)sum) - n) / (n * (n - 1.0));
        results[key_index].control_corr = control_corr;
        results[key_index].control_abs_corr = fabs(control_corr);
        results[key_index].control_unbiased_sq =
            (((double)control_sum * (double)control_sum) - n) / (n * (n - 1.0));

        #pragma omp critical
        {
            printf("KEY id=%d signed=% .10e abs=%.10e unbiased_sq=% .10e "
                   "control_signed=% .10e control_abs=%.10e control_unbiased_sq=% .10e\n",
                   key_index, results[key_index].corr, results[key_index].abs_corr,
                   results[key_index].unbiased_sq, results[key_index].control_corr,
                   results[key_index].control_abs_corr,
                   results[key_index].control_unbiased_sq);
            fflush(stdout);
        }
    }

    summarize("TARGET", results, 0);
    summarize("CONTROL", results, 1);
    double null_abs = sqrt(2.0 / (M_PI * (double)samples));
    printf("NULL_ABS_EXPECTATION value=%.10e exponent=%.6f\n",
           null_abs, -log2(null_abs));
    printf("ELAPSED seconds=%.3f\n", omp_get_wtime() - start);
    free(results);
    return 0;
}
