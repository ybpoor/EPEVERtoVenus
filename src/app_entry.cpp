#ifdef ARDUINO
#include <Arduino.h>

extern "C" {
    void app_main_c(void);
}

void setup(void) {
    app_main_c();
}

void loop(void) {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
#endif
