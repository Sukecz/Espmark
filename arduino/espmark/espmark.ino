/*
  espmark Arduino starter firmware

  Open this folder in Arduino IDE, select your ESP32 board, upload, and open
  Serial Monitor at 115200 baud. Press Enter to run the benchmark.
*/

#include <Arduino.h>
#include <math.h>

#define ESPMARK_SCHEMA_VERSION "1.0.0-beta"
#define ESPMARK_VERSION "0.2.3"
#define ESPMARK_FIRMWARE_VERSION ESPMARK_VERSION "-arduino"
#define ESPMARK_BENCHMARK_PROFILE "espmark-core-stress-preview"
#define ESPMARK_TEST_SET_ID "espmark-core-stress-preview-1"
#define ESPMARK_MODE "full"
#define ESPMARK_CPU_REPEAT 12
#define ESPMARK_CPU_INNER_ITERATIONS 250000UL
#define ESPMARK_COMPUTE_INNER_ITERATIONS 7000UL
#define ESPMARK_MATRIX_INNER_ITERATIONS 4200UL
#define ESPMARK_SUSTAINED_INNER_ITERATIONS 320000UL
#define ESPMARK_FLASH_REPEAT 12
#define ESPMARK_PRACTICAL_REPEAT 12
#define ESPMARK_PRACTICAL_INNER_ITERATIONS 1200UL
#define ESPMARK_MEM_REPEAT 12
#define ESPMARK_MEM_INNER_ITERATIONS 1024UL
#define ESPMARK_MEM_MAX_BLOCK_BYTES 16384UL
#define ESPMARK_MEM_ALLOCATIONS 64
#define ESPMARK_MEM_ALLOCATION_BYTES 128
#define ESPMARK_MEM_ALLOC_ROUNDS 12
#define ESPMARK_MEM_FRAGMENT_ROUNDS 24
#define ESPMARK_FLASH_READ_PASSES 128UL
#define ESPMARK_SAMPLE_SETTLE_MS 20
#define ESPMARK_BOARD_VENDOR "Generic"

#if defined(ARDUINO_ARCH_ESP8266)
#define ESPMARK_BOARD_NAME "Generic ESP8266"
#define ESPMARK_BOARD_TARGET "esp8266-generic"
#elif CONFIG_IDF_TARGET_ESP32
#define ESPMARK_BOARD_NAME "Generic ESP32"
#define ESPMARK_BOARD_TARGET "esp32-generic"
#elif CONFIG_IDF_TARGET_ESP32C3
#define ESPMARK_BOARD_NAME "Generic ESP32-C3"
#define ESPMARK_BOARD_TARGET "esp32c3-generic"
#elif CONFIG_IDF_TARGET_ESP32C5
#define ESPMARK_BOARD_NAME "Generic ESP32-C5"
#define ESPMARK_BOARD_TARGET "esp32c5-generic"
#elif CONFIG_IDF_TARGET_ESP32C6
#define ESPMARK_BOARD_NAME "Generic ESP32-C6"
#define ESPMARK_BOARD_TARGET "esp32c6-generic"
#elif CONFIG_IDF_TARGET_ESP32S2
#define ESPMARK_BOARD_NAME "Generic ESP32-S2"
#define ESPMARK_BOARD_TARGET "esp32s2-generic"
#elif CONFIG_IDF_TARGET_ESP32S3
#define ESPMARK_BOARD_NAME "Generic ESP32-S3"
#define ESPMARK_BOARD_TARGET "esp32s3-generic"
#elif CONFIG_IDF_TARGET_ESP32P4
#define ESPMARK_BOARD_NAME "Generic ESP32-P4"
#define ESPMARK_BOARD_TARGET "esp32p4-generic"
#else
#define ESPMARK_BOARD_NAME "Generic ESP32-family"
#define ESPMARK_BOARD_TARGET "esp32-family-generic"
#endif

struct Metric {
  const char *id;
  const char *category;
  const char *unit;
  uint32_t samples;
  double mean;
  double median;
  double min;
  double max;
  double stdev;
  double p95;
  uint32_t workUnits;
  uint32_t checksum;
};

typedef uint32_t (*KernelFn)(uint32_t iterations, uint32_t seed);
typedef void (*MemoryFn)(uint8_t *dst, uint8_t *src, size_t length, uint32_t iterations, volatile uint32_t &guard);

static Metric gMetrics[20];
static uint32_t gMetricCount = 0;
static size_t gMemoryBlockBytes = 0;
static bool gHasResults = false;

static const char *chipModel() {
#if defined(ARDUINO_ARCH_ESP8266)
  return "ESP8266";
#else
  return ESP.getChipModel();
#endif
}

static uint32_t chipRevision() {
#if defined(ARDUINO_ARCH_ESP8266)
  return 0;
#else
  return ESP.getChipRevision();
#endif
}

static uint32_t cpuFrequencyMhz() {
#if defined(ARDUINO_ARCH_ESP8266)
  return ESP.getCpuFreqMHz();
#else
  return getCpuFrequencyMhz();
#endif
}

static uint32_t heapSizeBytes() {
#if defined(ARDUINO_ARCH_ESP8266)
  return ESP.getFreeHeap();
#else
  return ESP.getHeapSize();
#endif
}

static uint32_t freeHeapBytes() {
  return ESP.getFreeHeap();
}

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

static void settleBeforeSamples() {
  delay(ESPMARK_SAMPLE_SETTLE_MS);
  yield();
}

static Metric summarizeUs(const char *id, const char *category, uint64_t *samples, uint32_t count, uint32_t workUnits, uint32_t checksum) {
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
    category,
    "us",
    count,
    mean,
    (count % 2 == 0) ? ((double)sorted[count / 2 - 1] + (double)sorted[count / 2]) / 2.0 : (double)sorted[count / 2],
    (double)sorted[0],
    (double)sorted[count - 1],
    sqrt(variance),
    (double)sorted[p95Index],
    workUnits,
    checksum,
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

static uint32_t kernelFloat32Affine(uint32_t iterations, uint32_t seed) {
  volatile float x = (float)(seed & 0xffffU) / 97.0f;
  volatile float y = 1.125f;
  for (uint32_t i = 0; i < iterations; ++i) {
    x = (x * 1.0009765625f) + y;
    y = (y * 0.9990234375f) - 0.03125f;
    if (x > 4096.0f) {
      x *= 0.125f;
    }
  }
  return (uint32_t)(x * 1000.0f) ^ (uint32_t)(y * 1000.0f);
}

static uint32_t kernelMandelbrotQ16(uint32_t iterations, uint32_t seed) {
  volatile uint32_t checksum = seed;
  for (uint32_t sample = 0; sample < iterations; ++sample) {
    const int32_t cx = -131072 + (int32_t)((sample % 48U) * 5461U);
    const int32_t cy = -65536 + (int32_t)(((sample / 48U) % 32U) * 4096U);
    int32_t x = 0;
    int32_t y = 0;
    uint32_t escape = 0;
    for (uint32_t i = 0; i < 24; ++i) {
      const int32_t x2 = (int32_t)(((int64_t)x * x) >> 16);
      const int32_t y2 = (int32_t)(((int64_t)y * y) >> 16);
      if (x2 + y2 > (4L << 16)) {
        break;
      }
      const int32_t xy = (int32_t)(((int64_t)x * y) >> 15);
      x = x2 - y2 + cx;
      y = xy + cy;
      ++escape;
    }
    checksum = (checksum * 33U) ^ escape ^ (uint32_t)x ^ ((uint32_t)y << 1);
  }
  return checksum;
}

static uint32_t kernelMatrixI16(uint32_t iterations, uint32_t seed) {
  int16_t a[6][6];
  int16_t b[6][6];
  int32_t c[6][6];
  for (uint32_t row = 0; row < 6; ++row) {
    for (uint32_t col = 0; col < 6; ++col) {
      a[row][col] = (int16_t)(((row * 17U + col * 31U + seed) & 0x7fU) - 63);
      b[row][col] = (int16_t)(((row * 29U + col * 13U + (seed >> 3)) & 0x7fU) - 63);
      c[row][col] = 0;
    }
  }

  volatile uint32_t checksum = seed;
  for (uint32_t iter = 0; iter < iterations; ++iter) {
    for (uint32_t row = 0; row < 6; ++row) {
      for (uint32_t col = 0; col < 6; ++col) {
        int32_t sum = 0;
        for (uint32_t k = 0; k < 6; ++k) {
          sum += (int32_t)a[row][k] * (int32_t)b[k][col];
        }
        c[row][col] = sum + (int32_t)(iter & 7U);
        checksum ^= (uint32_t)c[row][col] + (row << 4) + col;
      }
    }
  }
  return checksum;
}

static uint32_t kernelSustainedMix(uint32_t iterations, uint32_t seed) {
  volatile uint32_t x = seed ^ 0xa5a5a5a5U;
  volatile uint32_t y = seed | 1U;
  for (uint32_t i = 1; i <= iterations; ++i) {
    x += 0x9e3779b9U;
    x ^= x << 7;
    x += y / ((i & 15U) + 1U);
    y = (y * 1664525U) + 1013904223U;
    if ((i & 0x3ffU) == 0) {
      yield();
    }
  }
  return x ^ y;
}

static Metric runKernel(const char *id, KernelFn kernel, uint32_t iterations = ESPMARK_CPU_INNER_ITERATIONS) {
  uint64_t samples[ESPMARK_CPU_REPEAT];
  volatile uint32_t guard = 0;

  for (uint32_t i = 0; i < 3; ++i) {
    guard ^= kernel(iterations, 0x12345678U + i);
  }

  settleBeforeSamples();
  for (uint32_t i = 0; i < ESPMARK_CPU_REPEAT; ++i) {
    const uint64_t start = micros();
    guard ^= kernel(iterations, 0x9e3779b9U + i);
    const uint64_t end = micros();
    samples[i] = end - start;
  }

  return summarizeUs(id, "cpu", samples, ESPMARK_CPU_REPEAT, iterations, guard);
}

static size_t memoryBlockBytes() {
  size_t blockBytes = freeHeapBytes() / 4;
  if (blockBytes > ESPMARK_MEM_MAX_BLOCK_BYTES) {
    blockBytes = ESPMARK_MEM_MAX_BLOCK_BYTES;
  }
  blockBytes &= ~(size_t)255U;
  if (blockBytes < 1024) {
    blockBytes = 1024;
  }
  return blockBytes;
}

static void memoryMemcpy(uint8_t *dst, uint8_t *src, size_t length, uint32_t iterations, volatile uint32_t &guard) {
  for (uint32_t i = 0; i < iterations; ++i) {
    memcpy(dst, src, length);
    guard ^= dst[(i * 131U) % length];
  }
}

static void memoryMemset(uint8_t *dst, uint8_t *, size_t length, uint32_t iterations, volatile uint32_t &guard) {
  for (uint32_t i = 0; i < iterations; ++i) {
    memset(dst, (int)(i & 0xffU), length);
    guard ^= dst[(i * 131U) % length];
  }
}

static void memoryStridedRead(uint8_t *, uint8_t *src, size_t length, uint32_t iterations, volatile uint32_t &guard) {
  for (uint32_t i = 0; i < iterations; ++i) {
    uint32_t sum = 0;
    for (size_t offset = (i & 31U); offset < length; offset += 32) {
      sum += src[offset];
    }
    guard ^= sum;
  }
}

static Metric runMemoryKernel(const char *id, MemoryFn kernel, uint8_t *dst, uint8_t *src, size_t length) {
  uint64_t samples[ESPMARK_MEM_REPEAT];
  volatile uint32_t guard = 0;

  kernel(dst, src, length, 3, guard);

  settleBeforeSamples();
  for (uint32_t i = 0; i < ESPMARK_MEM_REPEAT; ++i) {
    const uint64_t start = micros();
    kernel(dst, src, length, ESPMARK_MEM_INNER_ITERATIONS, guard);
    const uint64_t end = micros();
    samples[i] = end - start;
  }

  return summarizeUs(id, "memory", samples, ESPMARK_MEM_REPEAT, (uint32_t)(length * ESPMARK_MEM_INNER_ITERATIONS), guard);
}

static Metric runAllocationBenchmark() {
  uint64_t samples[ESPMARK_MEM_REPEAT];
  volatile uint32_t guard = 0;

  settleBeforeSamples();
  for (uint32_t sample = 0; sample < ESPMARK_MEM_REPEAT; ++sample) {
    void *blocks[ESPMARK_MEM_ALLOCATIONS];
    for (uint32_t i = 0; i < ESPMARK_MEM_ALLOCATIONS; ++i) {
      blocks[i] = nullptr;
    }

    const uint64_t start = micros();
    for (uint32_t round = 0; round < ESPMARK_MEM_ALLOC_ROUNDS; ++round) {
      for (uint32_t i = 0; i < ESPMARK_MEM_ALLOCATIONS; ++i) {
        blocks[i] = malloc(ESPMARK_MEM_ALLOCATION_BYTES);
        if (blocks[i]) {
          memset(blocks[i], (int)(i + round), ESPMARK_MEM_ALLOCATION_BYTES);
          guard ^= ((uint8_t *)blocks[i])[(i + round) % ESPMARK_MEM_ALLOCATION_BYTES];
        }
      }
      for (int32_t i = ESPMARK_MEM_ALLOCATIONS - 1; i >= 0; --i) {
        free(blocks[i]);
        blocks[i] = nullptr;
      }
    }
    const uint64_t end = micros();
    samples[sample] = end - start;
  }

  return summarizeUs(
    "memory.heap.malloc_free.128b",
    "memory",
    samples,
    ESPMARK_MEM_REPEAT,
    ESPMARK_MEM_ALLOCATIONS * ESPMARK_MEM_ALLOC_ROUNDS,
    guard
  );
}

static Metric runHeapFragmentationBenchmark() {
  uint64_t samples[ESPMARK_MEM_REPEAT];
  volatile uint32_t guard = 0;

  settleBeforeSamples();
  for (uint32_t sample = 0; sample < ESPMARK_MEM_REPEAT; ++sample) {
    void *blocks[32];
    for (uint32_t i = 0; i < 32; ++i) {
      blocks[i] = nullptr;
    }

    const uint64_t start = micros();
    for (uint32_t round = 0; round < ESPMARK_MEM_FRAGMENT_ROUNDS; ++round) {
      for (uint32_t i = 0; i < 32; ++i) {
        const size_t size = 24U + ((i * 37U + round * 11U) % 160U);
        blocks[i] = malloc(size);
        if (blocks[i]) {
          memset(blocks[i], (int)(i + round), size);
          guard ^= ((uint8_t *)blocks[i])[size / 2U];
        }
      }
      for (uint32_t i = 1; i < 32; i += 2) {
        free(blocks[i]);
        blocks[i] = nullptr;
      }
      for (uint32_t i = 1; i < 32; i += 2) {
        const size_t size = 40U + ((i * 19U + round * 13U) % 96U);
        blocks[i] = malloc(size);
        if (blocks[i]) {
          memset(blocks[i], (int)(round + 3U), size);
          guard ^= ((uint8_t *)blocks[i])[0];
        }
      }
      for (int32_t i = 31; i >= 0; --i) {
        free(blocks[i]);
        blocks[i] = nullptr;
      }
    }
    const uint64_t end = micros();
    samples[sample] = end - start;
  }

  return summarizeUs("memory.heap.fragmentation", "memory", samples, ESPMARK_MEM_REPEAT, 32U * ESPMARK_MEM_FRAGMENT_ROUNDS, guard);
}

#define ESPMARK_FLASH_PATTERN_256 \
  0x31, 0x7a, 0xc4, 0x10, 0x9d, 0x22, 0xf0, 0x61, 0x48, 0x83, 0xbd, 0x05, 0xd6, 0x19, 0xa2, 0x5f, \
  0x6c, 0x90, 0x1e, 0xe7, 0x42, 0xb8, 0x73, 0x0d, 0xfa, 0x2c, 0x55, 0x99, 0x04, 0xce, 0x37, 0x68, \
  0xab, 0x11, 0xdf, 0x24, 0x75, 0x8e, 0x03, 0xc9, 0x52, 0x6f, 0xb1, 0x0a, 0xe2, 0x3c, 0x87, 0x49, \
  0x95, 0x20, 0xda, 0x64, 0x18, 0xf3, 0x7d, 0x06, 0xbe, 0x41, 0x8a, 0x2f, 0xd1, 0x59, 0x70, 0xac, \
  0x4d, 0x86, 0x12, 0xef, 0x39, 0xa4, 0x7b, 0x01, 0xc8, 0x5e, 0x93, 0x2a, 0xf6, 0x40, 0xbd, 0x77, \
  0x08, 0xd3, 0x6a, 0x9f, 0x25, 0xec, 0x51, 0xb7, 0x0e, 0x84, 0x3d, 0xf1, 0x69, 0xa8, 0x14, 0xcb, \
  0x72, 0x0b, 0xd9, 0x46, 0x8f, 0x33, 0xfa, 0x5c, 0x91, 0x27, 0xbe, 0x60, 0x0a, 0xe5, 0x38, 0x94, \
  0x1f, 0xc2, 0x7d, 0x56, 0xab, 0x04, 0xee, 0x89, 0x35, 0xd0, 0x6b, 0x17, 0xa1, 0x4c, 0xf8, 0x23, \
  0x5a, 0xb6, 0x0d, 0xc7, 0x92, 0x3e, 0xe1, 0x68, 0x15, 0xaf, 0x44, 0xfb, 0x80, 0x29, 0xd5, 0x63, \
  0x0f, 0xba, 0x71, 0x2c, 0xe8, 0x96, 0x41, 0xdd, 0x1a, 0x54, 0xc0, 0x8b, 0x37, 0xf2, 0x6e, 0x03, \
  0xad, 0x49, 0x85, 0x20, 0xdc, 0x76, 0x11, 0xbf, 0x58, 0x02, 0xea, 0x9c, 0x34, 0xc9, 0x7f, 0x26, \
  0xf4, 0x6a, 0x18, 0xa6, 0x4e, 0xd1, 0x90, 0x0c, 0xb8, 0x52, 0xed, 0x39, 0x84, 0x21, 0xcb, 0x67, \
  0x13, 0xfe, 0x45, 0x9a, 0x2f, 0xd7, 0x70, 0x08, 0xbc, 0x5d, 0xe2, 0x36, 0x81, 0x19, 0xcf, 0x64, \
  0x0b, 0xa7, 0x4a, 0xf5, 0x93, 0x2d, 0xd8, 0x51, 0x06, 0xbe, 0x79, 0x24, 0xe0, 0x8c, 0x32, 0xfa, \
  0x5f, 0x10, 0xc4, 0x6b, 0x97, 0x28, 0xdd, 0x43, 0x0e, 0xb2, 0x75, 0xec, 0x39, 0x8a, 0x16, 0xc1, \
  0x6d, 0x04, 0xf9, 0x50, 0xab, 0x27, 0xde, 0x83, 0x3c, 0x95, 0x1a, 0xe7, 0x62, 0x0f, 0xb9, 0x44

static const uint8_t kFlashBlob[] PROGMEM = {
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
  ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256, ESPMARK_FLASH_PATTERN_256,
};

static uint32_t crc32Update(uint32_t crc, uint8_t value) {
  crc ^= value;
  for (uint32_t bit = 0; bit < 8; ++bit) {
    const uint32_t mask = 0U - (crc & 1U);
    crc = (crc >> 1) ^ (0xedb88320U & mask);
  }
  return crc;
}

static Metric runFlashReadBenchmark() {
  uint64_t samples[ESPMARK_FLASH_REPEAT];
  volatile uint32_t guard = 0;

  settleBeforeSamples();
  for (uint32_t sample = 0; sample < ESPMARK_FLASH_REPEAT; ++sample) {
    const uint64_t start = micros();
    for (uint32_t pass = 0; pass < ESPMARK_FLASH_READ_PASSES; ++pass) {
      for (size_t i = 0; i < sizeof(kFlashBlob); ++i) {
        guard = (guard * 33U) ^ pgm_read_byte(&kFlashBlob[i]);
      }
    }
    const uint64_t end = micros();
    samples[sample] = end - start;
  }

  return summarizeUs("flash.read.seq", "flash", samples, ESPMARK_FLASH_REPEAT, (uint32_t)(sizeof(kFlashBlob) * ESPMARK_FLASH_READ_PASSES), guard);
}

static Metric runCrc32Benchmark() {
  uint64_t samples[ESPMARK_PRACTICAL_REPEAT];
  uint8_t buffer[256];
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    buffer[i] = (uint8_t)(i * 29U + 7U);
  }
  volatile uint32_t guard = 0;

  settleBeforeSamples();
  for (uint32_t sample = 0; sample < ESPMARK_PRACTICAL_REPEAT; ++sample) {
    uint32_t crc = 0xffffffffU;
    const uint64_t start = micros();
    for (uint32_t iter = 0; iter < ESPMARK_PRACTICAL_INNER_ITERATIONS; ++iter) {
      for (size_t i = 0; i < sizeof(buffer); ++i) {
        crc = crc32Update(crc, buffer[(i + iter) & 0xffU]);
      }
    }
    const uint64_t end = micros();
    guard ^= crc;
    samples[sample] = end - start;
  }

  return summarizeUs("practical.crc32.sw", "practical_iot", samples, ESPMARK_PRACTICAL_REPEAT, sizeof(buffer) * ESPMARK_PRACTICAL_INNER_ITERATIONS, guard);
}

struct Sha256Ctx {
  uint8_t data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
};

static const uint32_t kSha256K[64] PROGMEM = {
  0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
  0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
  0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
  0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
  0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
  0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
  0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
  0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL, 0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL,
};

static uint32_t rotr32(uint32_t value, uint32_t bits) {
  return (value >> bits) | (value << (32U - bits));
}

static void sha256Transform(void *context, const uint8_t data[]) {
  Sha256Ctx *ctx = (Sha256Ctx *)context;
  uint32_t m[64];
  for (uint32_t i = 0, j = 0; i < 16; ++i, j += 4) {
    m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) | ((uint32_t)data[j + 2] << 8) | data[j + 3];
  }
  for (uint32_t i = 16; i < 64; ++i) {
    const uint32_t s0 = rotr32(m[i - 15], 7) ^ rotr32(m[i - 15], 18) ^ (m[i - 15] >> 3);
    const uint32_t s1 = rotr32(m[i - 2], 17) ^ rotr32(m[i - 2], 19) ^ (m[i - 2] >> 10);
    m[i] = m[i - 16] + s0 + m[i - 7] + s1;
  }

  uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
  uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
  for (uint32_t i = 0; i < 64; ++i) {
    const uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
    const uint32_t ch = (e & f) ^ ((~e) & g);
    const uint32_t temp1 = h + s1 + ch + pgm_read_dword(&kSha256K[i]) + m[i];
    const uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
  ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256Init(void *context) {
  Sha256Ctx *ctx = (Sha256Ctx *)context;
  ctx->datalen = 0;
  ctx->bitlen = 0;
  ctx->state[0] = 0x6a09e667UL; ctx->state[1] = 0xbb67ae85UL; ctx->state[2] = 0x3c6ef372UL; ctx->state[3] = 0xa54ff53aUL;
  ctx->state[4] = 0x510e527fUL; ctx->state[5] = 0x9b05688cUL; ctx->state[6] = 0x1f83d9abUL; ctx->state[7] = 0x5be0cd19UL;
}

static void sha256Update(void *context, const uint8_t *data, size_t len) {
  Sha256Ctx *ctx = (Sha256Ctx *)context;
  for (size_t i = 0; i < len; ++i) {
    ctx->data[ctx->datalen++] = data[i];
    if (ctx->datalen == 64) {
      sha256Transform(ctx, ctx->data);
      ctx->bitlen += 512;
      ctx->datalen = 0;
    }
  }
}

static void sha256Final(void *context, uint8_t hash[32]) {
  Sha256Ctx *ctx = (Sha256Ctx *)context;
  uint32_t i = ctx->datalen;
  ctx->data[i++] = 0x80;
  if (i > 56) {
    while (i < 64) ctx->data[i++] = 0;
    sha256Transform(ctx, ctx->data);
    i = 0;
  }
  while (i < 56) ctx->data[i++] = 0;
  ctx->bitlen += (uint64_t)ctx->datalen * 8ULL;
  for (uint32_t j = 0; j < 8; ++j) {
    ctx->data[63 - j] = (uint8_t)(ctx->bitlen >> (j * 8));
  }
  sha256Transform(ctx, ctx->data);
  for (i = 0; i < 4; ++i) {
    for (uint32_t j = 0; j < 8; ++j) {
      hash[i + (j * 4)] = (uint8_t)(ctx->state[j] >> (24 - i * 8));
    }
  }
}

static Metric runSha256Benchmark() {
  uint64_t samples[ESPMARK_PRACTICAL_REPEAT];
  uint8_t buffer[256];
  uint8_t digest[32];
  volatile uint32_t guard = 0;
  for (size_t i = 0; i < sizeof(buffer); ++i) {
    buffer[i] = (uint8_t)(i * 13U + 91U);
  }

  settleBeforeSamples();
  for (uint32_t sample = 0; sample < ESPMARK_PRACTICAL_REPEAT; ++sample) {
    const uint64_t start = micros();
    for (uint32_t iter = 0; iter < ESPMARK_PRACTICAL_INNER_ITERATIONS / 4U; ++iter) {
      Sha256Ctx ctx;
      sha256Init(&ctx);
      buffer[0] = (uint8_t)(sample + iter);
      sha256Update(&ctx, buffer, sizeof(buffer));
      sha256Final(&ctx, digest);
      guard ^= ((uint32_t)digest[0] << 24) | ((uint32_t)digest[7] << 16) | ((uint32_t)digest[19] << 8) | digest[31];
    }
    const uint64_t end = micros();
    samples[sample] = end - start;
  }

  return summarizeUs("practical.sha256.sw", "practical_iot", samples, ESPMARK_PRACTICAL_REPEAT, sizeof(buffer) * (ESPMARK_PRACTICAL_INNER_ITERATIONS / 4U), guard);
}

static Metric runStringFormatBenchmark() {
  uint64_t samples[ESPMARK_PRACTICAL_REPEAT];
  char line[96];
  volatile uint32_t guard = 0;

  settleBeforeSamples();
  for (uint32_t sample = 0; sample < ESPMARK_PRACTICAL_REPEAT; ++sample) {
    const uint64_t start = micros();
    for (uint32_t iter = 0; iter < ESPMARK_PRACTICAL_INNER_ITERATIONS * 12U; ++iter) {
      const int written = snprintf(
        line,
        sizeof(line),
        "chip=%s,seq=%lu,temp=%ld,heap=%lu",
        chipModel(),
        (unsigned long)iter,
        (long)((int32_t)(iter % 900U) - 200),
        (unsigned long)(freeHeapBytes() & 0xffffU)
      );
      guard ^= (uint32_t)written ^ (uint8_t)line[(iter + 7U) % (sizeof(line) - 1U)];
    }
    const uint64_t end = micros();
    samples[sample] = end - start;
  }

  return summarizeUs("practical.string.format", "practical_iot", samples, ESPMARK_PRACTICAL_REPEAT, ESPMARK_PRACTICAL_INNER_ITERATIONS * 12U, guard);
}

static Metric runJsonRoundtripBenchmark() {
  uint64_t samples[ESPMARK_PRACTICAL_REPEAT];
  char json[160];
  volatile uint32_t guard = 0;

  settleBeforeSamples();
  for (uint32_t sample = 0; sample < ESPMARK_PRACTICAL_REPEAT; ++sample) {
    const uint64_t start = micros();
    for (uint32_t iter = 0; iter < ESPMARK_PRACTICAL_INNER_ITERATIONS * 8U; ++iter) {
      const int written = snprintf(
        json,
        sizeof(json),
        "{\"seq\":%lu,\"chip\":\"%s\",\"mhz\":%lu,\"ok\":true,\"heap\":%lu}",
        (unsigned long)iter,
        chipModel(),
        (unsigned long)cpuFrequencyMhz(),
        (unsigned long)(freeHeapBytes() & 0xffffU)
      );
      uint32_t parsed = 0;
      for (int i = 0; i < written; ++i) {
        const char c = json[i];
        if (c >= '0' && c <= '9') {
          parsed = parsed * 10U + (uint32_t)(c - '0');
        } else {
          parsed ^= (uint8_t)c;
        }
      }
      guard ^= parsed ^ (uint32_t)written;
    }
    const uint64_t end = micros();
    samples[sample] = end - start;
  }

  return summarizeUs("practical.json.roundtrip", "practical_iot", samples, ESPMARK_PRACTICAL_REPEAT, ESPMARK_PRACTICAL_INNER_ITERATIONS * 8U, guard);
}

static void printJsonMetric(const Metric &metric, bool last) {
  Serial.print("    {");
  Serial.print("\"test_id\":\"");
  Serial.print(metric.id);
  Serial.print("\",\"category\":\"");
  Serial.print(metric.category);
  Serial.print("\"");
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
  Serial.print(",\"work_units\":");
  Serial.print(metric.workUnits);
  Serial.print(",\"checksum\":\"0x");
  char checksum[9];
  snprintf(checksum, sizeof(checksum), "%08lx", (unsigned long)metric.checksum);
  Serial.print(checksum);
  Serial.print("\"");
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
  Serial.print("  \"benchmark_profile\": \"");
  Serial.print(ESPMARK_BENCHMARK_PROFILE);
  Serial.println("\",");
  Serial.print("  \"test_set_id\": \"");
  Serial.print(ESPMARK_TEST_SET_ID);
  Serial.println("\",");
  Serial.print("  \"mode\": \"");
  Serial.print(ESPMARK_MODE);
  Serial.println("\",");
  Serial.println("  \"official_generic_firmware\": true,");
  Serial.println("  \"board\": {");
  Serial.print("    \"vendor\": \"");
  Serial.print(ESPMARK_BOARD_VENDOR);
  Serial.println("\",");
  Serial.print("    \"name\": \"");
  Serial.print(ESPMARK_BOARD_NAME);
  Serial.println("\",");
  Serial.print("    \"module\": \"");
  Serial.print(chipModel());
  Serial.println("\",");
  Serial.print("    \"soc\": \"");
  Serial.print(chipModel());
  Serial.println("\",");
  Serial.print("    \"revision\": ");
  Serial.println(chipRevision());
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
  Serial.print(cpuFrequencyMhz());
  Serial.println(",");
  Serial.print("    \"flash_size_bytes\": ");
  Serial.print(ESP.getFlashChipSize());
  Serial.println(",");
  Serial.print("    \"heap_size_bytes\": ");
  Serial.print(heapSizeBytes());
  Serial.println(",");
  Serial.print("    \"cpu_repeat\": ");
  Serial.print(ESPMARK_CPU_REPEAT);
  Serial.println(",");
  Serial.print("    \"cpu_inner_iterations\": ");
  Serial.print(ESPMARK_CPU_INNER_ITERATIONS);
  Serial.println(",");
  Serial.print("    \"memory_repeat\": ");
  Serial.print(ESPMARK_MEM_REPEAT);
  Serial.println(",");
  Serial.print("    \"memory_inner_iterations\": ");
  Serial.print(ESPMARK_MEM_INNER_ITERATIONS);
  Serial.println(",");
  Serial.print("    \"memory_block_bytes\": ");
  Serial.print(gMemoryBlockBytes);
  Serial.println(",");
  Serial.print("    \"memory_allocation_bytes\": ");
  Serial.print(ESPMARK_MEM_ALLOCATION_BYTES);
  Serial.println(",");
  Serial.print("    \"memory_allocations\": ");
  Serial.print(ESPMARK_MEM_ALLOCATIONS);
  Serial.println(",");
  Serial.print("    \"memory_alloc_rounds\": ");
  Serial.print(ESPMARK_MEM_ALLOC_ROUNDS);
  Serial.println(",");
  Serial.print("    \"memory_fragment_rounds\": ");
  Serial.print(ESPMARK_MEM_FRAGMENT_ROUNDS);
  Serial.println(",");
  Serial.print("    \"flash_read_passes\": ");
  Serial.print(ESPMARK_FLASH_READ_PASSES);
  Serial.println(",");
  Serial.print("    \"practical_inner_iterations\": ");
  Serial.println(ESPMARK_PRACTICAL_INNER_ITERATIONS);
  Serial.println("  },");
  Serial.println("  \"results\": [");
  for (uint32_t i = 0; i < gMetricCount; ++i) {
    printJsonMetric(gMetrics[i], i == gMetricCount - 1);
  }
  Serial.println("  ]");
  Serial.println("}");
  Serial.println("ESPMARK_RESULT_END");
}

static const char *shortName(const char *id) {
  if (strcmp(id, "cpu.integer.add_mul.u32") == 0) {
    return "basic math";
  }
  if (strcmp(id, "cpu.integer.div_mod.u32") == 0) {
    return "hard math";
  }
  if (strcmp(id, "cpu.integer.branch.u32") == 0) {
    return "decisions";
  }
  if (strcmp(id, "cpu.integer.crc_like.u32") == 0) {
    return "data crunch";
  }
  if (strcmp(id, "cpu.sustained.mix") == 0) {
    return "sustained";
  }
  if (strcmp(id, "cpu.float32.affine") == 0) {
    return "float32";
  }
  if (strcmp(id, "cpu.mandelbrot.q16") == 0) {
    return "mandelbrot";
  }
  if (strcmp(id, "cpu.matrix.i16") == 0) {
    return "matrix";
  }
  if (strcmp(id, "memory.ram.memcpy.seq") == 0) {
    return "RAM copy";
  }
  if (strcmp(id, "memory.ram.memset.seq") == 0) {
    return "RAM fill";
  }
  if (strcmp(id, "memory.ram.read.strided") == 0) {
    return "RAM read";
  }
  if (strcmp(id, "memory.heap.malloc_free.128b") == 0) {
    return "small alloc";
  }
  if (strcmp(id, "memory.heap.fragmentation") == 0) {
    return "heap frag";
  }
  if (strcmp(id, "flash.read.seq") == 0) {
    return "flash read";
  }
  if (strcmp(id, "practical.crc32.sw") == 0) {
    return "CRC32";
  }
  if (strcmp(id, "practical.json.roundtrip") == 0) {
    return "JSON";
  }
  if (strcmp(id, "practical.string.format") == 0) {
    return "strings";
  }
  if (strcmp(id, "practical.sha256.sw") == 0) {
    return "SHA-256";
  }
  return id;
}

static double referenceUs(const char *id) {
  if (strcmp(id, "flash.read.seq") == 0) return 128000.0;

  if (strcmp(id, "practical.crc32.sw") == 0) return 35000.0;
  if (strcmp(id, "practical.sha256.sw") == 0) return 70000.0;
  if (strcmp(id, "practical.json.roundtrip") == 0) return 50000.0;
  if (strcmp(id, "practical.string.format") == 0) return 25000.0;

  if (strcmp(id, "cpu.integer.add_mul.u32") == 0) return 10000.0;
  if (strcmp(id, "cpu.integer.div_mod.u32") == 0) return 30000.0;
  if (strcmp(id, "cpu.integer.branch.u32") == 0) return 12000.0;
  if (strcmp(id, "cpu.integer.crc_like.u32") == 0) return 45000.0;
  if (strcmp(id, "cpu.sustained.mix") == 0) return 35000.0;
  if (strcmp(id, "cpu.float32.affine") == 0) return 25000.0;
  if (strcmp(id, "cpu.mandelbrot.q16") == 0) return 40000.0;
  if (strcmp(id, "cpu.matrix.i16") == 0) return 30000.0;

  if (strcmp(id, "memory.ram.memcpy.seq") == 0) return 16000.0;
  if (strcmp(id, "memory.ram.memset.seq") == 0) return 16000.0;
  if (strcmp(id, "memory.ram.read.strided") == 0) return 16000.0;
  if (strcmp(id, "memory.heap.malloc_free.128b") == 0) return 12000.0;
  if (strcmp(id, "memory.heap.fragmentation") == 0) return 6000.0;

  return 0.0;
}

static double categoryScore(const char *category) {
  double logSum = 0.0;
  double weightSum = 0.0;
  for (uint32_t i = 0; i < gMetricCount; ++i) {
    if (strcmp(gMetrics[i].category, category) != 0 || gMetrics[i].median <= 0.0) {
      continue;
    }
    const double ref = referenceUs(gMetrics[i].id);
    if (ref <= 0.0) {
      continue;
    }
    double ratio = ref / gMetrics[i].median;
    if (ratio < 0.25) ratio = 0.25;
    if (ratio > 4.0) ratio = 4.0;
    logSum += log(ratio);
    weightSum += 1.0;
  }
  if (weightSum <= 0.0) {
    return 0.0;
  }
  return exp(logSum / weightSum) * 1000.0;
}

static void printMetricRow(const Metric &metric) {
  char line[96];
  snprintf(
    line,
    sizeof(line),
    "%-12s %9.1f %9.1f %9.1f %9.1f",
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
  Serial.print("espmark ");
  Serial.println(ESPMARK_VERSION);
  Serial.println("Community ESP32 benchmark");
  Serial.println();
  Serial.print("Board: ");
  Serial.println(ESPMARK_BOARD_NAME);
  Serial.print("Chip: ");
  Serial.print(chipModel());
  Serial.print(" rev ");
  Serial.println(chipRevision());
  Serial.print("CPU: ");
  Serial.print(cpuFrequencyMhz());
  Serial.println(" MHz");
  Serial.print("Flash: ");
  Serial.print(ESP.getFlashChipSize() / 1024);
  Serial.println(" KB");
  Serial.print("Heap: ");
  Serial.print(heapSizeBytes() / 1024);
  Serial.println(" KB");
  Serial.println();
  Serial.println("espmark runs a balanced set of CPU, memory, flash and practical IoT workloads.");
  Serial.println("The web page turns the raw measurements into comparable scores.");
  Serial.println("Higher scores are better. Raw timing details are printed for transparency.");
  Serial.println();
  Serial.println("Click Run benchmark in the web UI, or press Enter in a serial monitor.");
}

static void printMetricGroup(const char *title, const char *category) {
  Serial.println();
  Serial.println(title);
  Serial.print("Score: ");
  Serial.print(categoryScore(category), 1);
  Serial.println(" (higher is better)");
  Serial.println("Raw time unit: microseconds per test run. The score above is the comparison value.");
  Serial.println();
  Serial.println("test              average    median       best        p95");
  Serial.println("---------------------------------------------------------");
  for (uint32_t i = 0; i < gMetricCount; ++i) {
    if (strcmp(gMetrics[i].category, category) == 0) {
      printMetricRow(gMetrics[i]);
    }
  }
}

static void printHumanReport() {
  printMetricGroup("CPU benchmark results", "cpu");
  printMetricGroup("Memory benchmark results", "memory");
  printMetricGroup("Flash benchmark results", "flash");
  printMetricGroup("Practical IoT benchmark results", "practical_iot");
  Serial.println();
  Serial.println("What was tested:");
  Serial.println("- CPU: everyday integer math, branches, float32 and deterministic compute");
  Serial.println("- Memory: RAM copy/fill/read plus small heap allocation patterns");
  Serial.println("- Flash: read-only access to data stored in program flash");
  Serial.println("- Practical IoT: JSON, strings, CRC32 and SHA-256 style payload work");
  Serial.println("- Stability: repeated samples help the server spot noisy runs");
  Serial.println();
  Serial.println("Press 'j' then Enter to print JSON for sharing.");
  Serial.println("Click Run benchmark in the web UI, or press Enter to run again.");
}

static void runBenchmarks() {
  Serial.println();
  Serial.println("Running benchmark...");
  Serial.println();
  gMetricCount = 0;
  gMetrics[gMetricCount++] = runKernel("cpu.integer.add_mul.u32", kernelAddMul);
  Serial.println("add/mul done");
  gMetrics[gMetricCount++] = runKernel("cpu.integer.div_mod.u32", kernelDivMod);
  Serial.println("div/mod done");
  gMetrics[gMetricCount++] = runKernel("cpu.integer.branch.u32", kernelBranch);
  Serial.println("branch done");
  gMetrics[gMetricCount++] = runKernel("cpu.integer.crc_like.u32", kernelCrcLike);
  Serial.println("crc-like done");
  gMetrics[gMetricCount++] = runKernel("cpu.sustained.mix", kernelSustainedMix, ESPMARK_SUSTAINED_INNER_ITERATIONS);
  Serial.println("sustained mix done");
  gMetrics[gMetricCount++] = runKernel("cpu.float32.affine", kernelFloat32Affine);
  Serial.println("float32 done");
  gMetrics[gMetricCount++] = runKernel("cpu.mandelbrot.q16", kernelMandelbrotQ16, ESPMARK_COMPUTE_INNER_ITERATIONS);
  Serial.println("mandelbrot done");
  gMetrics[gMetricCount++] = runKernel("cpu.matrix.i16", kernelMatrixI16, ESPMARK_MATRIX_INNER_ITERATIONS);
  Serial.println("matrix done");

  gMemoryBlockBytes = memoryBlockBytes();
  uint8_t *src = (uint8_t *)malloc(gMemoryBlockBytes);
  uint8_t *dst = (uint8_t *)malloc(gMemoryBlockBytes);
  if (src && dst) {
    for (size_t i = 0; i < gMemoryBlockBytes; ++i) {
      src[i] = (uint8_t)(i * 17U + 3U);
      dst[i] = 0;
    }
    gMetrics[gMetricCount++] = runMemoryKernel("memory.ram.memcpy.seq", memoryMemcpy, dst, src, gMemoryBlockBytes);
    Serial.println("memcpy done");
    gMetrics[gMetricCount++] = runMemoryKernel("memory.ram.memset.seq", memoryMemset, dst, src, gMemoryBlockBytes);
    Serial.println("memset done");
    gMetrics[gMetricCount++] = runMemoryKernel("memory.ram.read.strided", memoryStridedRead, dst, src, gMemoryBlockBytes);
    Serial.println("strided read done");
  } else {
    Serial.println("memory buffer allocation skipped");
  }
  free(dst);
  free(src);

  gMetrics[gMetricCount++] = runAllocationBenchmark();
  Serial.println("malloc/free done");
  gMetrics[gMetricCount++] = runHeapFragmentationBenchmark();
  Serial.println("heap fragmentation done");
  gMetrics[gMetricCount++] = runFlashReadBenchmark();
  Serial.println("flash read done");
  gMetrics[gMetricCount++] = runCrc32Benchmark();
  Serial.println("crc32 done");
  gMetrics[gMetricCount++] = runSha256Benchmark();
  Serial.println("sha256 done");
  gMetrics[gMetricCount++] = runJsonRoundtripBenchmark();
  Serial.println("json done");
  gMetrics[gMetricCount++] = runStringFormatBenchmark();
  Serial.println("strings done");

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
    return;
  }

  runBenchmarks();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  printIntro();
}

void loop() {
  waitForCommand();
}
