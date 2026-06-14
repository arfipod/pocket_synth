#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

void app_main() 
{
    for (;;)
    {
        ESP_LOGI("GENERAL", "This is a test");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
