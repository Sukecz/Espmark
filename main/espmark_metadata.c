#include "espmark.h"

#include <inttypes.h>
#include <stdio.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

void espmark_print_metadata_json_prefix(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    printf("ESPMARK_RESULT_BEGIN\n");
    printf("{\n");
    printf("  \"schema_version\": \"%s\",\n", ESPMARK_SCHEMA_VERSION);
    printf("  \"firmware_version\": \"%s\",\n", ESPMARK_FIRMWARE_VERSION);
    printf("  \"run_label\": \"%s\",\n", CONFIG_ESPMARK_RUN_LABEL);
    printf("  \"board\": {\n");
    printf("    \"vendor\": \"%s\",\n", CONFIG_ESPMARK_BOARD_VENDOR);
    printf("    \"name\": \"%s\",\n", CONFIG_ESPMARK_BOARD_NAME);
    printf("    \"module\": \"%s\",\n", CONFIG_ESPMARK_MODULE_NAME);
    printf("    \"soc\": \"%s\",\n", CONFIG_IDF_TARGET);
    printf("    \"revision\": %u\n", chip.revision);
    printf("  },\n");
    printf("  \"build\": {\n");
    printf("    \"sdk\": \"esp-idf\",\n");
    printf("    \"sdk_version\": \"%s\",\n", esp_get_idf_version());
    printf("    \"target\": \"%s\"\n", CONFIG_IDF_TARGET);
    printf("  },\n");
    printf("  \"config\": {\n");
    printf("    \"cpu_freq_mhz\": %d,\n", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    printf("    \"free_rtos_hz\": %d,\n", CONFIG_FREERTOS_HZ);
    printf("    \"flash_size_bytes\": %" PRIu32 ",\n", flash_size);
    printf("    \"cpu_cores\": %d,\n", SOC_CPU_CORES_NUM);
    printf("    \"cpu_repeat\": %d,\n", CONFIG_ESPMARK_CPU_REPEAT);
    printf("    \"cpu_inner_iterations\": %d\n", CONFIG_ESPMARK_CPU_INNER_ITERATIONS);
    printf("  },\n");
    printf("  \"results\": [\n");
}
