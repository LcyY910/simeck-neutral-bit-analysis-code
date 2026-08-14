#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TOTAL_ROUNDS 11
#define LEVELS 7
#define PAIRS_PER_STRUCTURE 64

static const uint64_t plaintext_levels[LEVELS] = {
    1ull << 10, 1ull << 12, 1ull << 14, 1ull << 16, 1ull << 18,
    1ull << 20, 1ull << 21
};
static const uint16_t class_guesses[4] = {0x0000, 0x0080, 0x1000, 0x1080};
static const unsigned independent_positions[6] = {0, 5, 10, 11, 15, 26};

typedef struct {
    uint64_t trials;
    uint64_t top_included;
    uint64_t unique_top;
    uint64_t rank_le_2;
    uint64_t rank_le_3;
    uint64_t top_ties[5];
    double random_tiebreak_success;
    double sum_rank;
    double sum_correct_abs_corr;
    double sum_correct_signed_corr;
    double sum_best_wrong_abs_corr;
} level_stats_t;

static inline uint16_t rol16(uint16_t x, unsigned n) {
    return (uint16_t)((x << n) | (x >> (16 - n)));
}

static inline uint16_t f16(uint16_t x) {
    return (uint16_t)((x & rol16(x, 5)) ^ rol16(x, 1));
}

static inline void encrypt11(uint16_t *left, uint16_t *right,
                             const uint16_t round_keys[32]) {
    uint16_t l = *left, r = *right;
    for (int i = 0; i < TOTAL_ROUNDS; i++) {
        uint16_t old_l = l;
        l = (uint16_t)(r ^ f16(l) ^ round_keys[i]);
        r = old_l;
    }
    *left = l;
    *right = r;
}

static void expand_simeck32_64(const uint16_t master[4],
                               uint16_t round_keys[32]) {
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

static uint32_t subset_mask(unsigned subset) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 6; i++) {
        if ((subset >> i) & 1u) value |= 1u << independent_positions[i];
    }
    return value;
}

static inline int score_from_ciphertexts(uint16_t c1l, uint16_t c1r,
                                         uint16_t c2l, uint16_t c2r,
                                         uint16_t guess) {
    uint16_t r10_1 = (uint16_t)(c1l ^ f16(c1r) ^ guess);
    uint16_t r10_2 = (uint16_t)(c2l ^ f16(c2r) ^ guess);
    uint16_t r9_1 = (uint16_t)(c1r ^ f16(r10_1));
    uint16_t r9_2 = (uint16_t)(c2r ^ f16(r10_2));
    return (((r9_1 ^ r9_2) >> 12) & 1u) ? -1 : 1;
}

static int vector_test(void) {
    const uint16_t master[4] = {0x0100, 0x0908, 0x1110, 0x1918};
    uint16_t rk[32];
    uint16_t left = 0x6565, right = 0x6877;
    expand_simeck32_64(master, rk);
    for (int i = 0; i < 32; i++) {
        uint16_t old_l = left;
        left = (uint16_t)(right ^ f16(left) ^ rk[i]);
        right = old_l;
    }
    printf("VECTOR32 observed=%04x:%04x expected=770d:2c76 status=%s\n",
           left, right, (left == 0x770d && right == 0x2c76) ? "PASS" : "FAIL");
    return left == 0x770d && right == 0x2c76;
}

static void record_level(level_stats_t *stats, const int64_t scores[4],
                         int correct_class, uint64_t pair_count) {
    int64_t abs_scores[4];
    int64_t maximum = 0;
    for (int c = 0; c < 4; c++) {
        abs_scores[c] = scores[c] < 0 ? -scores[c] : scores[c];
        if (abs_scores[c] > maximum) maximum = abs_scores[c];
    }

    int top_ties = 0;
    int greater = 0;
    int64_t best_wrong = 0;
    for (int c = 0; c < 4; c++) {
        if (abs_scores[c] == maximum) top_ties++;
        if (abs_scores[c] > abs_scores[correct_class]) greater++;
        if (c != correct_class && abs_scores[c] > best_wrong) {
            best_wrong = abs_scores[c];
        }
    }

    int rank = greater + 1;
    int included = abs_scores[correct_class] == maximum;
    stats->trials++;
    stats->top_included += included;
    stats->unique_top += included && top_ties == 1;
    stats->rank_le_2 += rank <= 2;
    stats->rank_le_3 += rank <= 3;
    stats->top_ties[top_ties]++;
    stats->random_tiebreak_success += included ? 1.0 / top_ties : 0.0;
    stats->sum_rank += rank;
    stats->sum_correct_abs_corr += (double)abs_scores[correct_class] / pair_count;
    stats->sum_correct_signed_corr += (double)scores[correct_class] / pair_count;
    stats->sum_best_wrong_abs_corr += (double)best_wrong / pair_count;
}

static void run_trial(uint64_t base_seed, uint64_t trial_index,
                      level_stats_t stats[LEVELS]) {
    uint64_t rng = base_seed ^
                   ((trial_index + 1) * 0xd1342543de82ef95ull);
    uint16_t master[4] = {
        (uint16_t)splitmix64(&rng), (uint16_t)splitmix64(&rng),
        (uint16_t)splitmix64(&rng), (uint16_t)splitmix64(&rng)
    };
    uint16_t rk[32];
    expand_simeck32_64(master, rk);
    int correct_class = ((rk[10] >> 7) & 1u) | (((rk[10] >> 12) & 1u) << 1);
    int64_t scores[4] = {0, 0, 0, 0};
    uint64_t pair_count = 0;
    int next_level = 0;
    const uint64_t max_structures = plaintext_levels[LEVELS - 1] / 128;

    for (uint64_t structure = 0; structure < max_structures; structure++) {
        uint16_t base_l = (uint16_t)splitmix64(&rng);
        uint16_t base_r = (uint16_t)splitmix64(&rng);
        for (unsigned subset = 0; subset < PAIRS_PER_STRUCTURE; subset++) {
            uint32_t noise = subset_mask(subset);
            uint16_t p1l = (uint16_t)(base_l ^ (noise >> 16));
            uint16_t p1r = (uint16_t)(base_r ^ noise);
            uint16_t p2l = p1l;
            uint16_t p2r = (uint16_t)(p1r ^ 0x2000u);
            uint16_t c1l = p1l, c1r = p1r, c2l = p2l, c2r = p2r;
            encrypt11(&c1l, &c1r, rk);
            encrypt11(&c2l, &c2r, rk);
            for (int c = 0; c < 4; c++) {
                scores[c] += score_from_ciphertexts(c1l, c1r, c2l, c2r,
                                                    class_guesses[c]);
            }
            pair_count++;
        }

        while (next_level < LEVELS &&
               pair_count == plaintext_levels[next_level] / 2) {
            record_level(&stats[next_level], scores, correct_class, pair_count);
            next_level++;
        }
    }
}

int main(int argc, char **argv) {
    uint64_t trials = argc > 1 ? strtoull(argv[1], NULL, 0) : 10000;
    uint64_t seed = argc > 2 ? strtoull(argv[2], NULL, 0) : 0x4b43555256453031ull;
    int requested_threads = argc > 3 ? atoi(argv[3]) : 4;
    if (trials == 0 || requested_threads < 1) {
        fprintf(stderr, "usage: %s [trials] [seed] [threads]\n", argv[0]);
        return 2;
    }
    if (!vector_test()) return 1;

    omp_set_num_threads(requested_threads);
    int thread_count = omp_get_max_threads();
    level_stats_t *all_stats = calloc((size_t)thread_count * LEVELS,
                                      sizeof(*all_stats));
    if (!all_stats) return 2;

    printf("CONFIG trials=%llu seed=0x%016llx threads=%d levels=2^10,2^12,2^14,2^16,2^18,2^20,2^21 structure_plaintexts=128 classes=4 effective_key_bits=2\n",
           (unsigned long long)trials, (unsigned long long)seed, thread_count);
    fflush(stdout);
    double started = omp_get_wtime();

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        level_stats_t *local = all_stats + (size_t)tid * LEVELS;
        #pragma omp for schedule(static)
        for (uint64_t trial = 0; trial < trials; trial++) {
            run_trial(seed, trial, local);
        }
    }

    level_stats_t totals[LEVELS] = {{0}};
    for (int tid = 0; tid < thread_count; tid++) {
        for (int level = 0; level < LEVELS; level++) {
            level_stats_t *dst = &totals[level];
            const level_stats_t *src = &all_stats[(size_t)tid * LEVELS + level];
            dst->trials += src->trials;
            dst->top_included += src->top_included;
            dst->unique_top += src->unique_top;
            dst->rank_le_2 += src->rank_le_2;
            dst->rank_le_3 += src->rank_le_3;
            for (int tie = 1; tie <= 4; tie++) dst->top_ties[tie] += src->top_ties[tie];
            dst->random_tiebreak_success += src->random_tiebreak_success;
            dst->sum_rank += src->sum_rank;
            dst->sum_correct_abs_corr += src->sum_correct_abs_corr;
            dst->sum_correct_signed_corr += src->sum_correct_signed_corr;
            dst->sum_best_wrong_abs_corr += src->sum_best_wrong_abs_corr;
        }
    }

    printf("RESULT_HEADER plaintexts,pairs,trials,top_included,unique_top,rank_le_2,rank_le_3,random_tiebreak_success,mean_rank,mean_correct_abs_corr,mean_correct_signed_corr,mean_best_wrong_abs_corr,ties1,ties2,ties3,ties4\n");
    for (int level = 0; level < LEVELS; level++) {
        const level_stats_t *s = &totals[level];
        double n = (double)s->trials;
        printf("RESULT %llu,%llu,%llu,%llu,%llu,%llu,%llu,%.10f,%.10f,%.10e,% .10e,%.10e,%llu,%llu,%llu,%llu\n",
               (unsigned long long)plaintext_levels[level],
               (unsigned long long)(plaintext_levels[level] / 2),
               (unsigned long long)s->trials,
               (unsigned long long)s->top_included,
               (unsigned long long)s->unique_top,
               (unsigned long long)s->rank_le_2,
               (unsigned long long)s->rank_le_3,
               s->random_tiebreak_success / n,
               s->sum_rank / n,
               s->sum_correct_abs_corr / n,
               s->sum_correct_signed_corr / n,
               s->sum_best_wrong_abs_corr / n,
               (unsigned long long)s->top_ties[1],
               (unsigned long long)s->top_ties[2],
               (unsigned long long)s->top_ties[3],
               (unsigned long long)s->top_ties[4]);
    }
    printf("ELAPSED seconds=%.3f\n", omp_get_wtime() - started);
    free(all_stats);
    return 0;
}
