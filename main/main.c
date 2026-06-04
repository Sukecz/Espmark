#include "espmark.h"

#include <stdio.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    espmark_print_metadata_json_prefix();
    espmark_run_cpu_benchmarks();
    printf("  ]\n");
    printf("}\n");
    printf("ESPMARK_RESULT_END\n");

    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

