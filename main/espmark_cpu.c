#include "espmark.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "esp_timer.h"
#include "sdkconfig.h"

typedef uint32_t (*kernel_fn_t)(uint32_t iterations, uint32_t seed);

static void sort_u64(uint64_t *values, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        uint64_t key = values[i];
        size_t j = i;
        while (j > 0 && values[j - 1] > key) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = key;
    }
}

static espmark_metric_t summarize_us(const char *id, uint64_t *samples, uint32_t count)
{
    uint64_t sorted[CONFIG_ESPMARK_CPU_REPEAT];
    memcpy(sorted, samples, sizeof(uint64_t) * count);
    sort_u64(sorted, count);

    double sum = 0.0;
    for (uint32_t i = 0; i < count; ++i) {
        sum += (double)samples[i];
    }

    const double mean = sum / (double)count;
    double variance = 0.0;
    for (uint32_t i = 0; i < count; ++i) {
        const double diff = (double)samples[i] - mean;
        variance += diff * diff;
    }
    variance /= (double)count;

    const uint32_t p95_index = (count * 95 + 99) / 100 - 1;

    espmark_metric_t metric = {
        .id = id,
        .unit = "us",
        .samples = count,
        .mean = mean,
        .median = (count % 2 == 0)
            ? ((double)sorted[count / 2 - 1] + (double)sorted[count / 2]) / 2.0
            : (double)sorted[count / 2],
        .min = (double)sorted[0],
        .max = (double)sorted[count - 1],
        .stdev = sqrt(variance),
        .p95 = (double)sorted[p95_index],
    };
    return metric;
}

static uint32_t kernel_add_mul(uint32_t iterations, uint32_t seed)
{
    volatile uint32_t x = seed | 1U;
    for (uint32_t i = 0; i < iterations; ++i) {
        x += 0x9e3779b9U;
        x *= 1664525U;
        x ^= x >> 13;
    }
    return x;
}

static uint32_t kernel_div_mod(uint32_t iterations, uint32_t seed)
{
    volatile uint32_t x = seed | 0x10001U;
    for (uint32_t i = 1; i <= iterations; ++i) {
        x += (x / ((i & 31U) + 1U));
        x ^= (x % ((i & 15U) + 3U)) << 7;
    }
    return x;
}

static uint32_t kernel_branch(uint32_t iterations, uint32_t seed)
{
    volatile uint32_t x = seed;
    for (uint32_t i = 0; i < iterations; ++i) {
        if (((x ^ i) & 7U) < 3U) {
            x = (x << 3) ^ (x >> 2) ^ i;
        } else {
            x = (x >> 1) + (i * 33U);
        }
    }
    return x;
}

static uint32_t kernel_crc_like(uint32_t iterations, uint32_t seed)
{
    volatile uint32_t crc = seed ^ 0xffffffffU;
    for (uint32_t i = 0; i < iterations; ++i) {
        crc ^= i;
        for (uint32_t bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    return crc;
}

static espmark_metric_t run_kernel(const char *id, kernel_fn_t kernel)
{
    uint64_t samples[CONFIG_ESPMARK_CPU_REPEAT];
    volatile uint32_t guard = 0;

    for (uint32_t i = 0; i < 3; ++i) {
        guard ^= kernel(CONFIG_ESPMARK_CPU_INNER_ITERATIONS, 0x12345678U + i);
    }

    for (uint32_t i = 0; i < CONFIG_ESPMARK_CPU_REPEAT; ++i) {
        const int64_t start = esp_timer_get_time();
        guard ^= kernel(CONFIG_ESPMARK_CPU_INNER_ITERATIONS, 0x9e3779b9U + i);
        const int64_t end = esp_timer_get_time();
        samples[i] = (uint64_t)(end - start);
    }

    return summarize_us(id, samples, CONFIG_ESPMARK_CPU_REPEAT);
}

static void print_metric(const espmark_metric_t *metric, bool last)
{
    printf("    {");
    printf("\"test_id\":\"%s\",", metric->id);
    printf("\"category\":\"cpu\",");
    printf("\"unit\":\"%s\",", metric->unit);
    printf("\"samples\":%" PRIu32 ",", metric->samples);
    printf("\"mean\":%.3f,", metric->mean);
    printf("\"median\":%.3f,", metric->median);
    printf("\"stdev\":%.3f,", metric->stdev);
    printf("\"p95\":%.3f,", metric->p95);
    printf("\"min\":%.3f,", metric->min);
    printf("\"max\":%.3f", metric->max);
    printf("}%s\n", last ? "" : ",");
}

void espmark_run_cpu_benchmarks(void)
{
    const espmark_metric_t add_mul = run_kernel("cpu.integer.add_mul.u32", kernel_add_mul);
    const espmark_metric_t div_mod = run_kernel("cpu.integer.div_mod.u32", kernel_div_mod);
    const espmark_metric_t branch = run_kernel("cpu.integer.branch.u32", kernel_branch);
    const espmark_metric_t crc = run_kernel("cpu.integer.crc_like.u32", kernel_crc_like);

    print_metric(&add_mul, false);
    print_metric(&div_mod, false);
    print_metric(&branch, false);
    print_metric(&crc, true);
}
