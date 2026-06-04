/*
  espmark Arduino starter firmware

  Open this folder in Arduino IDE, select your ESP32 board, upload, and open
  Serial Monitor at 115200 baud. Press Enter to start the benchmark.
*/

#include <Arduino.h>
#include <math.h>

#define ESPMARK_SCHEMA_VERSION "0.1.0"
#define ESPMARK_FIRMWARE_VERSION "0.1.0-arduino"
#define ESPMARK_CPU_REPEAT 20
#define ESPMARK_CPU_INNER_ITERATIONS 25000UL
#define ESPMARK_BOARD_VENDOR "Seeed Studio"
#define ESPMARK_BOARD_NAME "XIAO ESP32C6"
#define ESPMARK_BOARD_TARGET "XIAO_ESP32C6"

struct Metric {
  const char *id;
  const char *unit;
  uint32_t samples;
  double mean;
  double median;
  double min;
  double max;
  double stdev;
  double p95;
};

static Metric gMetrics[4];
static bool gHasResults = false;

typedef uint32_t (*KernelFn)(uint32_t iterations, uint32_t seed);

static void sortU64(uint64_t *values, size_t count) {
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

static Metric summarizeUs(const char *id, uint64_t *samples, uint32_t count) {
  uint64_t sorted[ESPMARK_CPU_REPEAT];
  memcpy(sorted, samples, sizeof(uint64_t) * count);
  sortU64(sorted, count);

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

  const uint32_t p95Index = (count * 95 + 99) / 100 - 1;

  Metric metric = {
    id,
    "us",
    count,
    mean,
    (count % 2 == 0) ? ((double)sorted[count / 2 - 1] + (double)sorted[count / 2]) / 2.0 : (double)sorted[count / 2],
    (double)sorted[0],
    (double)sorted[count - 1],
    sqrt(variance),
    (double)sorted[p95Index],
  };
  return metric;
}

static uint32_t kernelAddMul(uint32_t iterations, uint32_t seed) {
  volatile uint32_t x = seed | 1U;
  for (uint32_t i = 0; i < iterations; ++i) {
    x += 0x9e3779b9U;
    x *= 1664525U;
    x ^= x >> 13;
  }
  return x;
}

static uint32_t kernelDivMod(uint32_t iterations, uint32_t seed) {
  volatile uint32_t x = seed | 0x10001U;
  for (uint32_t i = 1; i <= iterations; ++i) {
    x += (x / ((i & 31U) + 1U));
    x ^= (x % ((i & 15U) + 3U)) << 7;
  }
  return x;
}

static uint32_t kernelBranch(uint32_t iterations, uint32_t seed) {
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

static uint32_t kernelCrcLike(uint32_t iterations, uint32_t seed) {
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

static Metric runKernel(const char *id, KernelFn kernel) {
  uint64_t samples[ESPMARK_CPU_REPEAT];
  volatile uint32_t guard = 0;

  for (uint32_t i = 0; i < 3; ++i) {
    guard ^= kernel(ESPMARK_CPU_INNER_ITERATIONS, 0x12345678U + i);
  }

  for (uint32_t i = 0; i < ESPMARK_CPU_REPEAT; ++i) {
    const uint64_t start = micros();
    guard ^= kernel(ESPMARK_CPU_INNER_ITERATIONS, 0x9e3779b9U + i);
    const uint64_t end = micros();
    samples[i] = end - start;
  }

  return summarizeUs(id, samples, ESPMARK_CPU_REPEAT);
}

static void printJsonMetric(const Metric &metric, bool last) {
  Serial.print("    {");
  Serial.print("\"test_id\":\"");
  Serial.print(metric.id);
  Serial.print("\",\"category\":\"cpu\"");
  Serial.print(",\"unit\":\"");
  Serial.print(metric.unit);
  Serial.print("\",\"samples\":");
  Serial.print(metric.samples);
  Serial.print(",\"mean\":");
  Serial.print(metric.mean, 3);
  Serial.print(",\"median\":");
  Serial.print(metric.median, 3);
  Serial.print(",\"stdev\":");
  Serial.print(metric.stdev, 3);
  Serial.print(",\"p95\":");
  Serial.print(metric.p95, 3);
  Serial.print(",\"min\":");
  Serial.print(metric.min, 3);
  Serial.print(",\"max\":");
  Serial.print(metric.max, 3);
  Serial.print("}");
  Serial.println(last ? "" : ",");
}

static void printJsonResult() {
  Serial.println("ESPMARK_RESULT_BEGIN");
  Serial.println("{");
  Serial.print("  \"schema_version\": \"");
  Serial.print(ESPMARK_SCHEMA_VERSION);
  Serial.println("\",");
  Serial.print("  \"firmware_version\": \"");
  Serial.print(ESPMARK_FIRMWARE_VERSION);
  Serial.println("\",");
  Serial.println("  \"board\": {");
  Serial.print("    \"vendor\": \"");
  Serial.print(ESPMARK_BOARD_VENDOR);
  Serial.println("\",");
  Serial.print("    \"name\": \"");
  Serial.print(ESPMARK_BOARD_NAME);
  Serial.println("\",");
  Serial.print("    \"module\": \"");
  Serial.print(ESP.getChipModel());
  Serial.println("\",");
  Serial.print("    \"soc\": \"");
  Serial.print(ESP.getChipModel());
  Serial.println("\",");
  Serial.print("    \"revision\": ");
  Serial.println(ESP.getChipRevision());
  Serial.println("  },");
  Serial.println("  \"build\": {");
  Serial.println("    \"sdk\": \"arduino-esp32\",");
  Serial.print("    \"sdk_version\": \"");
  Serial.print(ESP.getSdkVersion());
  Serial.println("\",");
  Serial.print("    \"target\": \"");
  Serial.print(ESPMARK_BOARD_TARGET);
  Serial.println("\"");
  Serial.println("  },");
  Serial.println("  \"config\": {");
  Serial.print("    \"cpu_freq_mhz\": ");
  Serial.print(getCpuFrequencyMhz());
  Serial.println(",");
  Serial.print("    \"flash_size_bytes\": ");
  Serial.print(ESP.getFlashChipSize());
  Serial.println(",");
  Serial.print("    \"heap_size_bytes\": ");
  Serial.print(ESP.getHeapSize());
  Serial.println(",");
  Serial.print("    \"cpu_repeat\": ");
  Serial.print(ESPMARK_CPU_REPEAT);
  Serial.println(",");
  Serial.print("    \"cpu_inner_iterations\": ");
  Serial.println(ESPMARK_CPU_INNER_ITERATIONS);
  Serial.println("  },");
  Serial.println("  \"results\": [");
  printJsonMetric(gMetrics[0], false);
  printJsonMetric(gMetrics[1], false);
  printJsonMetric(gMetrics[2], false);
  printJsonMetric(gMetrics[3], true);
  Serial.println("  ]");
  Serial.println("}");
  Serial.println("ESPMARK_RESULT_END");
}

static const char *shortName(const char *id) {
  if (strcmp(id, "cpu.integer.add_mul.u32") == 0) {
    return "add/mul";
  }
  if (strcmp(id, "cpu.integer.div_mod.u32") == 0) {
    return "div/mod";
  }
  if (strcmp(id, "cpu.integer.branch.u32") == 0) {
    return "branch";
  }
  if (strcmp(id, "cpu.integer.crc_like.u32") == 0) {
    return "crc-like";
  }
  return id;
}

static void printMetricRow(const Metric &metric) {
  char line[96];
  snprintf(
    line,
    sizeof(line),
    "%-10s %10.3f %10.3f %10.3f %10.3f",
    shortName(metric.id),
    metric.mean,
    metric.median,
    metric.min,
    metric.p95
  );
  Serial.println(line);
}

static void printIntro() {
  Serial.println();
  Serial.println("espmark 0.1.0");
  Serial.println("Community ESP32 benchmark");
  Serial.println();
  Serial.print("Board: ");
  Serial.println(ESPMARK_BOARD_NAME);
  Serial.print("Chip: ");
  Serial.print(ESP.getChipModel());
  Serial.print(" rev ");
  Serial.println(ESP.getChipRevision());
  Serial.print("CPU: ");
  Serial.print(getCpuFrequencyMhz());
  Serial.println(" MHz");
  Serial.print("Flash: ");
  Serial.print(ESP.getFlashChipSize() / 1024);
  Serial.println(" KB");
  Serial.print("Heap: ");
  Serial.print(ESP.getHeapSize() / 1024);
  Serial.println(" KB");
  Serial.println();
  Serial.println("This benchmark runs four CPU integer tests.");
  Serial.println("Lower time is better. Results are printed in microseconds.");
  Serial.println();
  Serial.println("Press Enter to start.");
}

static void printHumanReport() {
  Serial.println();
  Serial.println("CPU benchmark results");
  Serial.println("Unit: microseconds per benchmark run. Lower is better.");
  Serial.println();
  Serial.println("test             mean     median        min        p95");
  Serial.println("------------------------------------------------------");
  for (uint32_t i = 0; i < 4; ++i) {
    printMetricRow(gMetrics[i]);
  }
  Serial.println();
  Serial.println("Tests:");
  Serial.println("- add/mul: 32-bit integer addition, multiplication and xor mix");
  Serial.println("- div/mod: 32-bit integer division and modulo");
  Serial.println("- branch: branch-heavy integer workload");
  Serial.println("- crc-like: bit-by-bit CRC-style integer workload");
  Serial.println();
  Serial.println("Press 'j' then Enter to print JSON for sharing.");
  Serial.println("Press Enter to run the benchmark again.");
}

static void runCpuBenchmarks() {
  Serial.println();
  Serial.println("Running CPU benchmark...");
  Serial.println();
  gMetrics[0] = runKernel("cpu.integer.add_mul.u32", kernelAddMul);
  Serial.println("add/mul done");
  gMetrics[1] = runKernel("cpu.integer.div_mod.u32", kernelDivMod);
  Serial.println("div/mod done");
  gMetrics[2] = runKernel("cpu.integer.branch.u32", kernelBranch);
  Serial.println("branch done");
  gMetrics[3] = runKernel("cpu.integer.crc_like.u32", kernelCrcLike);
  Serial.println("crc-like done");
  gHasResults = true;
  printHumanReport();
}

static void waitForCommand() {
  while (!Serial.available()) {
    delay(20);
  }

  bool wantsJson = false;
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == 'j' || c == 'J') {
      wantsJson = true;
    }
    delay(2);
  }

  if (wantsJson && gHasResults) {
    printJsonResult();
    Serial.println();
    Serial.println("Press Enter to run the benchmark again.");
    return;
  }

  runCpuBenchmarks();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  printIntro();
}

void loop() {
  waitForCommand();
}
