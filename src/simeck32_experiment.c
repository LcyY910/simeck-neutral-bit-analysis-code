#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define MASK16 0xffffu
#define TOTAL_ROUNDS 11
#ifndef STRUCTURES
#define STRUCTURES 32
#endif
#define PAIRS_PER_STRUCTURE 64
#ifndef TRIALS
#define TRIALS 5
#endif

typedef struct {
    uint16_t c1l, c1r, c2l, c2r;
} pair_t;

static inline uint16_t rol16(uint16_t x, unsigned n) {
    return (uint16_t)((x << n) | (x >> (16 - n)));
}

static inline uint16_t f16(uint16_t x) {
    return (uint16_t)((x & rol16(x, 5)) ^ rol16(x, 1));
}

static inline void round16(uint16_t *l, uint16_t *r, uint16_t key) {
    uint16_t old_l = *l;
    *l = (uint16_t)(*r ^ f16(*l) ^ key);
    *r = old_l;
}

static void encrypt16(uint16_t *l, uint16_t *r, const uint16_t *round_keys, int rounds) {
    for (int i = 0; i < rounds; i++) {
        round16(l, r, round_keys[i]);
    }
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

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static uint32_t subset_mask(unsigned subset, const unsigned *positions, unsigned count) {
    uint32_t value = 0;
    for (unsigned i = 0; i < count; i++) {
        if ((subset >> i) & 1u) value |= 1u << positions[i];
    }
    return value;
}

static void three_round_diff(uint16_t pl, uint16_t pr, const uint16_t keys[3],
                             uint16_t *dl, uint16_t *dr) {
    uint16_t al = pl, ar = pr;
    uint16_t bl = pl, br = (uint16_t)(pr ^ 0x2000u);
    encrypt16(&al, &ar, keys, 3);
    encrypt16(&bl, &br, keys, 3);
    *dl = (uint16_t)(al ^ bl);
    *dr = (uint16_t)(ar ^ br);
}

static int test_official_vector(void) {
    const uint16_t master[4] = {0x0100, 0x0908, 0x1110, 0x1918};
    uint16_t rk[32];
    uint16_t l = 0x6565, r = 0x6877;
    expand_simeck32_64(master, rk);
    encrypt16(&l, &r, rk, 32);
    int pass = (l == 0x770d && r == 0x2c76);
    printf("VECTOR l=%04x r=%04x expected=770d:2c76 status=%s\n",
           l, r, pass ? "PASS" : "FAIL");
    return pass;
}

static int test_known_counterexample(void) {
    const uint16_t keys[3] = {0x2080, 0x6388, 0x0000};
    const uint16_t pl = 0x218e, pr = 0x0086;
    const uint32_t noise = 0x106030c2u;
    uint16_t dl0, dr0, dl1, dr1;
    three_round_diff(pl, pr, keys, &dl0, &dr0);
    three_round_diff((uint16_t)(pl ^ (noise >> 16)),
                     (uint16_t)(pr ^ noise), keys, &dl1, &dr1);
    int differs = (dl0 != dl1 || dr0 != dr1);
    printf("NB16_COUNTEREXAMPLE base=%04x:%04x flipped=%04x:%04x status=%s\n",
           dl0, dr0, dl1, dr1, differs ? "CONFIRMED" : "NOT_REPRODUCED");
    return differs;
}

static int stress_reduced_mask(void) {
    const unsigned positions[7] = {0, 5, 10, 11, 13, 15, 26};
    uint64_t rng = 0x20260805c0decafeull;
    const int trials = 20000;
    uint64_t checked = 0;

    for (int t = 0; t < trials; t++) {
        uint16_t pl = (uint16_t)splitmix64(&rng);
        uint16_t pr = (uint16_t)splitmix64(&rng);
        uint16_t keys[3] = {
            (uint16_t)splitmix64(&rng),
            (uint16_t)splitmix64(&rng),
            (uint16_t)splitmix64(&rng)
        };
        uint16_t base_l, base_r;
        three_round_diff(pl, pr, keys, &base_l, &base_r);

        for (unsigned s = 0; s < 128; s++) {
            uint32_t noise = subset_mask(s, positions, 7);
            uint16_t dl, dr;
            three_round_diff((uint16_t)(pl ^ (noise >> 16)),
                             (uint16_t)(pr ^ noise), keys, &dl, &dr);
            checked++;
            if (dl != base_l || dr != base_r) {
                printf("NB7_STRESS status=FAIL trial=%d noise=%08x\n", t, noise);
                return 0;
            }
        }
    }
    printf("NB7_STRESS mask=0400ac21 checked=%llu status=PASS\n",
           (unsigned long long)checked);
    return 1;
}

static void build_pairs(pair_t *pairs, uint64_t *rng, const uint16_t rk[32]) {
    const unsigned independent_positions[6] = {0, 5, 10, 11, 15, 26};
    int index = 0;
    for (int s = 0; s < STRUCTURES; s++) {
        uint16_t base_l = (uint16_t)splitmix64(rng);
        uint16_t base_r = (uint16_t)splitmix64(rng);
        for (unsigned n = 0; n < PAIRS_PER_STRUCTURE; n++) {
            uint32_t noise = subset_mask(n, independent_positions, 6);
            uint16_t p1l = (uint16_t)(base_l ^ (noise >> 16));
            uint16_t p1r = (uint16_t)(base_r ^ noise);
            uint16_t p2l = p1l;
            uint16_t p2r = (uint16_t)(p1r ^ 0x2000u);
            uint16_t c1l = p1l, c1r = p1r;
            uint16_t c2l = p2l, c2r = p2r;
            encrypt16(&c1l, &c1r, rk, TOTAL_ROUNDS);
            encrypt16(&c2l, &c2r, rk, TOTAL_ROUNDS);
            pairs[index++] = (pair_t){c1l, c1r, c2l, c2r};
        }
    }
}

static inline int pair_score(const pair_t *p, uint16_t guess) {
    uint16_t l10_1 = p->c1r;
    uint16_t r10_1 = (uint16_t)(p->c1l ^ f16(p->c1r) ^ guess);
    uint16_t l10_2 = p->c2r;
    uint16_t r10_2 = (uint16_t)(p->c2l ^ f16(p->c2r) ^ guess);

    uint16_t r9_1 = (uint16_t)(l10_1 ^ f16(r10_1));
    uint16_t r9_2 = (uint16_t)(l10_2 ^ f16(r10_2));
    unsigned parity = ((r9_1 ^ r9_2) >> 12) & 1u;
    return parity ? -1 : 1;
}

static void run_key_recovery(void) {
    const int pair_count = STRUCTURES * PAIRS_PER_STRUCTURE;
    pair_t *pairs = malloc((size_t)pair_count * sizeof(*pairs));
    int *scores = malloc(65536u * sizeof(*scores));
    if (!pairs || !scores) {
        fprintf(stderr, "allocation failed\n");
        exit(2);
    }

    uint64_t rng = 0x1571271571abcdefull;
    int top1_success = 0;
    double start = omp_get_wtime();
    printf("KEY_RECOVERY config trials=%d structures=%d pairs=%d plaintexts=%d threads=%d\n",
           TRIALS, STRUCTURES, pair_count, STRUCTURES * 128, omp_get_max_threads());

    for (int trial = 0; trial < TRIALS; trial++) {
        uint16_t master[4] = {
            (uint16_t)splitmix64(&rng), (uint16_t)splitmix64(&rng),
            (uint16_t)splitmix64(&rng), (uint16_t)splitmix64(&rng)
        };
        uint16_t rk[32];
        expand_simeck32_64(master, rk);
        build_pairs(pairs, &rng, rk);

        #pragma omp parallel for schedule(static)
        for (int guess = 0; guess < 65536; guess++) {
            int score = 0;
            for (int i = 0; i < pair_count; i++) {
                score += pair_score(&pairs[i], (uint16_t)guess);
            }
            scores[guess] = score;
        }

        uint16_t real = rk[10];
        int real_abs = scores[real] < 0 ? -scores[real] : scores[real];
        int max_abs = 0;
        int greater = 0, tied = 0;
        uint16_t first_best = 0;
        for (int guess = 0; guess < 65536; guess++) {
            int value = scores[guess] < 0 ? -scores[guess] : scores[guess];
            if (value > max_abs) {
                max_abs = value;
                first_best = (uint16_t)guess;
            }
        }
        for (int guess = 0; guess < 65536; guess++) {
            int value = scores[guess] < 0 ? -scores[guess] : scores[guess];
            if (value > real_abs) greater++;
            if (value == max_abs) tied++;
        }
        int rank = greater + 1;
        int success = (real_abs == max_abs);
        top1_success += success;
        double corr = (double)real_abs / pair_count;
        double bias = corr / 2.0;
        printf("KEY_TRIAL id=%d master=%04x:%04x:%04x:%04x real=%04x rank=%d "
               "top_ties=%d first_best=%04x score=%d corr=%.8f bias=%.8f status=%s\n",
               trial, master[0], master[1], master[2], master[3], real, rank,
               tied, first_best, scores[real], corr, bias, success ? "TOP" : "MISS");
        fflush(stdout);
    }

    printf("KEY_SUMMARY top1=%d/%d elapsed=%.3f_seconds\n",
           top1_success, TRIALS, omp_get_wtime() - start);
    free(scores);
    free(pairs);
}

int main(void) {
    omp_set_num_threads(4);
    int vector_ok = test_official_vector();
    int counterexample_ok = test_known_counterexample();
    int reduced_ok = stress_reduced_mask();
    if (!vector_ok || !counterexample_ok || !reduced_ok) {
        printf("PRECHECK status=FAIL\n");
        return 1;
    }
    printf("PRECHECK status=PASS\n");
    run_key_recovery();
    return 0;
}
