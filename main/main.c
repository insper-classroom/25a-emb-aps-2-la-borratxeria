#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "mpu6050.h"
#include "Fusion.h"

#include "hc06.h"

#define MPU_ADDRESS 0x68
#define I2C_SDA_GPIO 4
#define I2C_SCL_GPIO 5

#define CODE_TILT_LEFT   8
#define CODE_TILT_RIGHT  9

const int VRX = 26;
const int VRY = 27;

const uint START_BTN  = 15;
const uint BTN_HOME   = 10;
const uint BTN_A      = 12;
const uint BTN_B      = 11;
const uint BTN_1      = 14;
const uint BTN_2      = 13;

const uint CONECTION_LED = 28;
const uint START_LED     = 21;
const uint BUZZER        = 17;

typedef struct {
    int button;
    int value;
} btn_t;

static QueueHandle_t xQueue;

void oled1_btn_led_init(void) {
    gpio_init(CONECTION_LED); gpio_set_dir(CONECTION_LED, GPIO_OUT);
    gpio_init(START_LED);     gpio_set_dir(START_LED, GPIO_OUT);
    gpio_init(BUZZER);        gpio_set_dir(BUZZER, GPIO_OUT);

    const uint pins[] = { START_BTN, BTN_HOME, BTN_A, BTN_B, BTN_1, BTN_2 };
    for (size_t i = 0; i < sizeof(pins)/sizeof(pins[0]); i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
}

static void mpu6050_reset(void) {
    uint8_t buf[] = { 0x6B, 0x00 };
    i2c_write_blocking(i2c_default, MPU_ADDRESS, buf, 2, false);
}

static void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp) {
    uint8_t buffer[6], reg;

    reg = 0x3B;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 6, false);
    for (int i = 0; i < 3; i++)
        accel[i] = (buffer[2*i] << 8) | buffer[2*i + 1];

    reg = 0x43;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 6, false);
    for (int i = 0; i < 3; i++)
        gyro[i] = (buffer[2*i] << 8) | buffer[2*i + 1];

    reg = 0x41;
    i2c_write_blocking(i2c_default, MPU_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c_default, MPU_ADDRESS, buffer, 2, false);
    *temp = (buffer[0] << 8) | buffer[1];
}

void btn_callback(uint gpio, uint32_t events) {
    btn_t evt;
    evt.value = (events == GPIO_IRQ_EDGE_FALL) ? 1 : 0;
    if      (gpio == START_BTN) evt.button = 7;
    else if (gpio == BTN_HOME ) evt.button = 2;
    else if (gpio == BTN_A    ) evt.button = 3;
    else if (gpio == BTN_B    ) evt.button = 4;
    else if (gpio == BTN_1    ) evt.button = 5;
    else if (gpio == BTN_2    ) evt.button = 6;
    xQueueSendFromISR(xQueue, &evt, NULL);
}

void x_task(void *p) {
    btn_t evt;
    int samples[5] = {0}, idx = 0;
    while (1) {
        adc_select_input(0);
        samples[idx] = adc_read();
        idx = (idx + 1) % 5;
        int sum = 0;
        for (int i = 0; i < 5; i++) sum += samples[i];
        int mean = sum / 5;
        int delta = mean - 2048;
        int scaled = (delta * 255) / 2048;
        if (scaled > -30 && scaled < 30) scaled = 0;
        if (scaled != 0) {
            evt.button = 0;
            evt.value  = scaled;
            xQueueSend(xQueue, &evt, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void y_task(void *p) {
    btn_t evt;
    int samples[5] = {0}, idx = 0;
    while (1) {
        adc_select_input(1);
        samples[idx] = adc_read();
        idx = (idx + 1) % 5;
        int sum = 0;
        for (int i = 0; i < 5; i++) sum += samples[i];
        int mean = sum / 5;
        int delta = mean - 2048;
        int scaled = (delta * 255) / 2048;
        if (scaled > -30 && scaled < 30) scaled = 0;
        if (scaled != 0) {
            evt.button = 1;
            evt.value  = -scaled;
            xQueueSend(xQueue, &evt, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void uart_task(void *p) {
    btn_t evt;

    uart_init(HC06_UART_ID, HC06_BAUD_RATE);
    gpio_set_function(8, GPIO_FUNC_UART);
    gpio_set_function(9, GPIO_FUNC_UART);
    hc06_init("aps2_legal", "1234");
    
    while (1) {
        if (xQueueReceive(xQueue, &evt, portMAX_DELAY)) {
            uint8_t b   = (uint8_t)evt.button;
            int16_t v   = (int16_t)evt.value;
            uint8_t lsb = v & 0xFF;
            uint8_t msb = (v >> 8) & 0xFF;
            uart_putc(HC06_UART_ID, b);
            uart_putc(HC06_UART_ID, lsb);
            uart_putc(HC06_UART_ID, msb);
            uart_putc(HC06_UART_ID, 0xFF);
        }
    }
}

void mpu6050_task(void *p) {
    int16_t accel[3], gyro[3], temp;
    bool left_active = false, right_active = false;

    i2c_init(i2c_default, 400000);
    gpio_set_function(I2C_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_GPIO, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_GPIO);
    gpio_pull_up(I2C_SCL_GPIO);

    mpu6050_reset();
    vTaskDelay(pdMS_TO_TICKS(100));

    FusionAhrs ahrs;
    FusionAhrsInitialise(&ahrs);

    const float samplePeriod = 0.01f;
    FusionVector gyroscope, accelerometer;

    while (1) {
        mpu6050_read_raw(accel, gyro, &temp);

        gyroscope.axis.x = gyro[0] / 131.0f;
        gyroscope.axis.y = gyro[1] / 131.0f;
        gyroscope.axis.z = gyro[2] / 131.0f;

        accelerometer.axis.x = accel[0] / 16384.0f;
        accelerometer.axis.y = accel[1] / 16384.0f;
        accelerometer.axis.z = accel[2] / 16384.0f;

        FusionAhrsUpdateNoMagnetometer(&ahrs, gyroscope, accelerometer, samplePeriod);
        FusionEuler angles = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));
        float roll = FusionRadiansToDegrees(angles.angle.roll);

        btn_t evt;
        if (roll < -1000.0f) {
            evt.button = CODE_TILT_LEFT;
            evt.value = 1;
            xQueueSend(xQueue, &evt, 0);
            left_active = true;
            right_active = false;
        } else if (roll > 1000.0f) {
            evt.button = CODE_TILT_RIGHT;
            evt.value = 1;
            xQueueSend(xQueue, &evt, 0);
            right_active = true;
            left_active = false;
        } else if (roll >= -800.0f && roll <= 800.0f) {
            if (left_active) {
                evt.button = CODE_TILT_LEFT;
                evt.value = 0;
                xQueueSend(xQueue, &evt, 0);
                left_active = false;
            }
            if (right_active) {
                evt.button = CODE_TILT_RIGHT;
                evt.value = 0;
                xQueueSend(xQueue, &evt, 0);
                right_active = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main(void) {
    stdio_init_all();
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);
    oled1_btn_led_init();

    gpio_set_irq_enabled_with_callback(START_BTN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true, &btn_callback);
    gpio_set_irq_enabled(BTN_HOME, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_A,    GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_B,    GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_1,    GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_2,    GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);

    xQueue = xQueueCreate(32, sizeof(btn_t));
    xTaskCreate(uart_task,    "uart", 2048, NULL, 1, NULL);
    xTaskCreate(x_task,       "adcX", 2048, NULL, 1, NULL);
    xTaskCreate(y_task,       "adcY", 2048, NULL, 1, NULL);
    xTaskCreate(mpu6050_task, "gyro", 2048, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1) tight_loop_contents();
    return 0;
}