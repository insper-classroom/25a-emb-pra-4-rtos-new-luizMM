/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const uint ECHO_PIN = 7;
const uint TRIG_PIN = 6;

volatile int time_end = 0;
volatile int time_start = 0;

QueueHandle_t xQueueDistance;
QueueHandle_t xQueueTime;
SemaphoreHandle_t xSemaphoreTrigger;

void pin_callback(uint gpio, uint32_t events){
    int time;
    if (events == 0x8) {
        time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time, NULL);
    } else if (events == 0x4) {
        time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time, NULL);
    }
}

void trigger_task(void *p){
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    while (true){
        xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(200));

        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);
        
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void echo_task(void *p){
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    int time_start = 0;
    int time_end = 0;

    while (true){
        if (xQueueReceive(xQueueTime, &time_start, pdMS_TO_TICKS(100))){
            if (xQueueReceive(xQueueTime, &time_end, pdMS_TO_TICKS(100))){
                if (time_end > time_start){
                    int dif = time_end - time_start;
                    float distance = (dif * 0.0343)/ 2;

                    if (distance > 400 || distance < 2){
                        distance = -1;
                    }
                    xQueueSend(xQueueDistance, &distance, 0);
                    xSemaphoreGive(xSemaphoreTrigger);
                }
            }
        }
    }
    
}

void oled_task(void *p){
    ssd1306_init();

    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    float distance = 0;
    char buffer[32];

    while (true){
        xSemaphoreTake(xSemaphoreTrigger, 100);

        if (xQueueReceive(xQueueDistance, &distance, pdMS_TO_TICKS(100))){
            gfx_clear_buffer(&disp);
            
            if (distance <= 400 && distance >= 2){
                snprintf(buffer, sizeof(buffer), "Distancia: %.2f", distance);
                gfx_draw_string(&disp, 0, 10, 1, buffer);
            } else{
                gfx_draw_string(&disp, 0, 10, 1, "Erro: out of range");
            }

            int bar_length = 0;
            if (distance > 112){
                bar_length = 112;
            } else{
                bar_length = (int)distance;
            }

            gfx_draw_line(&disp, 0, 27, bar_length, 27);
            
            gfx_show(&disp);
        } else{
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 10, 1,"Erro: Sem sinal");
            gfx_show(&disp);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
}

int main() {
    stdio_init_all();

    xQueueTime = xQueueCreate(32, sizeof(int));
    xQueueDistance = xQueueCreate(32, sizeof(float));
    xSemaphoreTrigger = xSemaphoreCreateBinary();

    xSemaphoreGive(xSemaphoreTrigger);

    xTaskCreate(trigger_task, "Trigger task", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo task", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
