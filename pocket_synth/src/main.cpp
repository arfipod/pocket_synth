#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "main_params.h"

#include "carputer_pinmap.h"
#include "IWave.h"
#include "SineWave.h"

extern "C" void app_main(void)
{
    IWave* wave = new SineWave(440.0f, 0.0f); // A4 note
    
    for (;;)
    {
        float sample = wave->nextSample();
        ESP_LOGI("GENERAL", "This is a test");
        (void)sample; // Nothing to do yet, but in the future it will be sent to I2S
        vTaskDelay(pdMS_TO_TICKS(STEP_TIME * 1000)); // Delay for the duration of one sample
    }
}
