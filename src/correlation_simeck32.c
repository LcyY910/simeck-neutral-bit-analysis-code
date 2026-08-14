#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef KEY_COUNT
#define KEY_COUNT 64
#endif
#ifndef SAMPLE_LOG2
#define SAMPLE_LOG2 18
#endif
#ifndef TEST_ROUNDS
#define TEST_ROUNDS 8
#endif
#ifndef TEST_DP_L
#define TEST_DP_L 0x0000u
#endif
#ifndef TEST_DP_R
#define TEST_DP_R 0x2000u
#endif
#ifndef TEST_LC_L
#define TEST_LC_L 0x0000u
#endif
#ifndef TEST_LC_R
#define TEST_LC_R 0x1000u
#endif
#ifndef BASE_SEED
#define BASE_SEED 0x32335f504f534954ull
#endif

typedef struct {
    double corr;
    double abs_corr;
    double unbiased_sq;
    double control_corr;
    double control_abs_corr;
    double control_unbiased_sq;
} key_result_t;

static inline uint16_t rol16(uint16_t x, unsigned n) {
    return (uint16_t)((x << n) | (x >> (16 - n)));
}

static inline uint16_t f16(uint16_t x) {
    return (uint16_t)((x & rol16(x, 5)) ^ rol16(x, 1));
}

static inline void round16(uint16_t *left, uint16_t *right, uint16_t key) {
    uint16_t old_left = *left;
    *left = (uint16_t)(*right ^ f16(*left) ^ key);
    *right = old_left;
}

static void expand_simeck32_64(const uint16_t master[4], uint16_t round_keys[32]) {
    uint16_t keys[4] = {master[0], master[1], master[2], master[3]};
    uint32_t sequence = 0x9a42bb1fu;
    for (int i = 0; i < 32; i++) {
        round_keys[i] = keys[0];
        uint16_t constant = (uint16_t)(0xfffcu | (sequence & 1u));
        sequence >>= 1;
        uint16_t old_k1 = keys[1];
        keys[1] = (uint16_t)(keys[0] ^ f16(keys[1]) ^ constant);
        keys[0] = old_k1;
        uint16_t generated = keys[1];
        keys[1] = keys[2];
        keys[2] = keys[3];
        keys[3] = generated;
    }
}

static inline void encrypt16(uint16_t *left, uint16_t *right,
                             const uint16_t *round_keys, int rounds) {
    for (int i = 0; i < rounds; i++) round16(left, right, round_keys[i]);
}

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static inline unsigned parity32(uint16_t left, uint16_t right,
                                uint16_t mask_left, uint16_t mask_right) {
    return (__builtin_parity((unsigned)(left & mask_left)) ^
            __builtin_parity((unsigned)(right & mask_right))) & 1u;
}

static void summarize(const char *label, const key_result_t *results, int control) {
    double mean_corr = 0.0, mean_abs = 0.0, mean_sq = 0.0;
    for (int i = 0; i < KEY_COUNT; i++) {
        mean_corr += control ? results[i].control_corr : results[i].corr;
        mean_abs += control ? results[i].control_abs_corr : results[i].abs_corr;
        mean_sq += control ? results[i].control_unbiased_sq : results[i].unbiased_sq;
    }
    mean_corr /= KEY_COUNT;
    mean_abs /= KEY_COUNT;
    mean_sq /= KEY_COUNT;

    double variance = 0.0;
    for (int i = 0; i < KEY_COUNT; i++) {
        double value = control ? results[i].control_unbiased_sq : results[i].unbiased_sq;
        double delta = value - mean_sq;
        variance += delta * delta;
    }
    variance /= KEY_COUNT - 1;
    double se = sqrt(variance / KEY_COUNT);
    double rms = mean_sq > 0.0 ? sqrt(mean_sq) : 0.0;
    printf("%s mean_signed=% .10e mean_abs=%.10e abs_exp=%.6f "
           "mean_unbiased_sq=% .10e sq_se=%.10e z=%.4f rms=%.10e rms_exp=%.6f\n",
           label, mean_corr, mean_abs, -log2(mean_abs), mean_sq, se,
           se > 0.0 ? mean_sq / se : 0.0, rms,
           rms > 0.0 ? -log2(rms) : INFINITY);
}

int main(void) {
    const uint64_t samples = 1ull << SAMPLE_LOG2;
    key_result_t *results = calloc(KEY_COUNT, sizeof(*results));
    if (!results) return 2;

    const uint16_t vector_master[4] = {0x0100, 0x0908, 0x1110, 0x1918};
    uint16_t vector_rk[32];
    uint16_t vector_l = 0x6565, vector_r = 0x6877;
    expand_simeck32_64(vector_master, vector_rk);
    encrypt16(&vector_l, &vector_r, vector_rk, 32);
    printf("VECTOR32 l=%04x r=%04x expected=770d:2c76 status=%s\n",
           vector_l, vector_r,
           (vector_l == 0x770d && vector_r == 0x2c76) ? "PASS" : "FAIL");

    const uint16_t control_l = TEST_LC_L ^ 1u;
    const uint16_t control_r = TEST_LC_R ^ 1u;
    omp_set_num_threads(4);
    printf("CONFIG rounds=%d keys=%d samples_per_key=2^%d total_samples=2^%.6f "
           "dp=%04x:%04x lc=%04x:%04x control=%04x:%04x seed=0x%016llx threads=%d\n",
           TEST_ROUNDS, KEY_COUNT, SAMPLE_LOG2,
           log2((double)KEY_COUNT * samples), TEST_DP_L, TEST_DP_R,
           TEST_LC_L, TEST_LC_R, control_l, control_r,
           (unsigned long long)BASE_SEED, omp_get_max_threads());
    double started = omp_get_wtime();

    #pragma omp parallel for schedule(static)
    for (int key_index = 0; key_index < KEY_COUNT; key_index++) {
        uint64_t rng = BASE_SEED ^
                       ((uint64_t)key_index * 0x9e3779b97f4a7c15ull);
        uint16_t master[4] = {
            (uint16_t)splitmix64(&rng), (uint16_t)splitmix64(&rng),
            (uint16_t)splitmix64(&rng), (uint16_t)splitmix64(&rng)
        };
        uint16_t round_keys[32];
        expand_simeck32_64(master, round_keys);
        int64_t sum = 0, control_sum = 0;

        for (uint64_t i = 0; i < samples; i++) {
            uint16_t p1l = (uint16_t)splitmix64(&rng);
            uint16_t p1r = (uint16_t)splitmix64(&rng);
            uint16_t c1l = p1l, c1r = p1r;
            uint16_t c2l = p1l ^ TEST_DP_L, c2r = p1r ^ TEST_DP_R;
            encrypt16(&c1l, &c1r, round_keys, TEST_ROUNDS);
            encrypt16(&c2l, &c2r, round_keys, TEST_ROUNDS);
            unsigned bit = parity32(c1l ^ c2l, c1r ^ c2r,
                                    TEST_LC_L, TEST_LC_R);
            unsigned ctl = parity32(c1l ^ c2l, c1r ^ c2r,
                                    control_l, control_r);
            sum += bit ? -1 : 1;
            control_sum += ctl ? -1 : 1;
        }

        double n = (double)samples;
        results[key_index].corr = (double)sum / n;
        results[key_index].abs_corr = fabs(results[key_index].corr);
        results[key_index].unbiased_sq =
            (((double)sum * sum) - n) / (n * (n - 1.0));
        results[key_index].control_corr = (double)control_sum / n;
        results[key_index].control_abs_corr = fabs(results[key_index].control_corr);
        results[key_index].control_unbiased_sq =
            (((double)control_sum * control_sum) - n) / (n * (n - 1.0));
    }

    summarize("TARGET", results, 0);
    summarize("CONTROL", results, 1);
    printf("NULL_ABS_EXPECTATION value=%.10e exponent=%.6f\n",
           sqrt(2.0 / (M_PI * (double)samples)),
           -log2(sqrt(2.0 / (M_PI * (double)samples))));
    printf("ELAPSED seconds=%.3f\n", omp_get_wtime() - started);
    free(results);
    return 0;
}
