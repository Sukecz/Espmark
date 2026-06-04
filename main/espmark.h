#pragma once

#include <stdint.h>

#define ESPMARK_SCHEMA_VERSION "0.1.0"
#define ESPMARK_FIRMWARE_VERSION "0.1.0"

typedef struct {
    const char *id;
    const char *unit;
    uint32_t samples;
    double mean;
    double median;
    double min;
    double max;
    double stdev;
    double p95;
} espmark_metric_t;

void espmark_print_metadata_json_prefix(void);
void espmark_run_cpu_benchmarks(void);

