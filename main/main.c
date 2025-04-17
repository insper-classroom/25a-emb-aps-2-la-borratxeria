#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include "ssd1306.h"
#include "gfx.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/adc.h"

// DEFININDO ENTRADAS

const int VRX = 26;
const int VRY = 27; 

const uint START_BTN = 15; // Botão para ligar o controle 
const uint BTN_HOME = 10; // Equivalente ao botão Home do Wii 
const uint BTN_A = 12; // Equivalente ao botão A do Wii -> Selecionar mapas, funções / Pausar o jogo 
const uint BTN_B = 11; // Equivalente ao botão B do Wii -> Soltar poderzinho 
const uint BTN_1 = 14; // Equivalente ao botão 1 do Wii -> Freiar, Ré, Drift
const uint BTN_2 = 13; // Equivalente ao botão 2 do Wii -> Acelerar

const uint CONECTION_LED = 28; // Led que indica que o controle está conectado com o computador
const uint START_LED = 21; // Led que indica que o controle está ligado 

const uint BUZZER = 17; // Buzzer que toca música ao se conectar com o computador

// CRIANDO STRUCT 
typedef struct {
    int button;
    int value;
} btn_t;

// CRIANDO VARIÁVEIS
double sleep_time;
int num_ciclos;

// CRIANDO FILA
QueueHandle_t xQueue;


// INICIALIZANDO LEDS E BOTÕES
void oled1_btn_led_init(void) {
    gpio_init(CONECTION_LED);
    gpio_set_dir(CONECTION_LED, GPIO_OUT);

    gpio_init(START_LED);
    gpio_set_dir(START_LED, GPIO_OUT);

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);

    gpio_init(START_BTN);
    gpio_set_dir(START_BTN, GPIO_IN);
    gpio_pull_up(START_BTN);

    gpio_init(BTN_HOME);
    gpio_set_dir(BTN_HOME, GPIO_IN);
    gpio_pull_up(BTN_HOME);

    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);

    gpio_init(BTN_B);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_B);

    gpio_init(BTN_1);
    gpio_set_dir(BTN_1, GPIO_IN);
    gpio_pull_up(BTN_1);

    gpio_init(BTN_2);
    gpio_set_dir(BTN_2, GPIO_IN);
    gpio_pull_up(BTN_2);
}


// FUNÇÕES

// Função para calcular os parãmetros usados para tocar os sons
void calcula_frequencia(int freq, int time, double *sleep, int *ciclos) {
    double periodo = (1.0 / freq) * 1e6; // Período em microssegundos
    *sleep = periodo / 2.0; // Meio período
    *ciclos = (time * 1000) / periodo; // Número total de ciclos
}

// Função que toca os sons
void toca_som(int freq, int time) {
    calcula_frequencia(freq, time, &sleep_time, &num_ciclos);
    for (int i = 0; i < num_ciclos; i++) {
        gpio_put(BUZZER, 1);
        sleep_us(sleep_time);
        gpio_put(BUZZER, 0);
        sleep_us(sleep_time);
    }
}

// Função que faz tocar a música do mário
void toca_musica_inicial() {
    // Frequências aproximadas (em Hz) para algumas notas usadas no tema do Mario
    // E5 ~ 659 Hz, C5 ~ 523 Hz, G5 ~ 784 Hz, G4 ~ 392 Hz
    // Usamos '0' para representar pausa (sem som).

    int notas[] = {
        659, 659, 659,   0,  // E5, E5, E5, pausa
        523, 659, 784,   0,  // C5, E5, G5, pausa
        392,  0                // G4, pausa
    };

    // Durações em milissegundos (aprox.) para cada nota/pausa
    int duracoes[] = {
        150, 150, 150, 150,
        150, 150, 150, 150,
        300, 150
    };

    int tamanho = sizeof(notas) / sizeof(notas[0]);

    for (int i = 0; i < tamanho; i++) {
        if (notas[i] == 0) {
            // Se a nota for 0, significa uma pausa
            sleep_ms(duracoes[i]);
        } else {
            // Toca o som com a frequência e duração especificada
            toca_som(notas[i], duracoes[i]);
        }
        // Pequeno intervalo entre as notas para evitar transição brusca
        sleep_ms(50);
    }
}


// Callbacks
void btn_callback(uint gpio, uint32_t events) {
    btn_t button_info; 
    if (events == 0x4) { // fall edge
        button_info.value = 1;

        if (gpio == START_BTN){
            button_info.button = 7;
        }
        if (gpio == BTN_HOME){
            button_info.button = 2;
        }
        if (gpio == BTN_A){
            button_info.button = 3;
        }
        if (gpio == BTN_B){
            button_info.button = 4;
        }
        if (gpio == BTN_1){
            button_info.button = 5;
        }
        if (gpio == BTN_2){
            button_info.button = 6;
        }
        xQueueSendFromISR(xQueue, &button_info, 0); // Mandando para fila o endereço do struct 
    }
    if (events == 0x8){
        button_info.value = 0;

        if (gpio == START_BTN){
            button_info.button = 7;
        }
        if (gpio == BTN_HOME){
            button_info.button = 2;
        }
        if (gpio == BTN_A){
            button_info.button = 3;
        }
        if (gpio == BTN_B){
            button_info.button = 4;
        }
        if (gpio == BTN_1){
            button_info.button = 5;
        }
        if (gpio == BTN_2){
            button_info.button = 6;
        }
        xQueueSendFromISR(xQueue, &button_info, 0); // Mandando para fila o endereço do struct 

    }
}

// TASKS

void uart_task(void *p) {
    btn_t recebido;
    while (1) {
        if (xQueueReceive(xQueue, &recebido, 5)) {
            uint8_t button = (uint8_t)recebido.button;
            int16_t value = (int16_t)recebido.value;

            uint8_t msb = (uint8_t)((value >> 8) & 0xFF);
            uint8_t lsb = (uint8_t)(value & 0xFF);
            uint8_t eop = 0xFF;

            putchar_raw(button);
            putchar_raw(lsb);
            putchar_raw(msb);
            putchar_raw(eop);
            // vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void x_task(void *p) {
    
    btn_t recebido;

    static int samples[5] = {0, 0, 0, 0, 0};
    static int count = 0;

    while (1) {
        adc_select_input(0);
        int valor_adc = adc_read();

        // media movel
        if (count < 4) {
            count ++;
        } else{
            count = 0;
        }
        samples[count] = valor_adc;
        int mean = (samples[0] + samples[1] + samples[2] + samples[3] + samples[4]) / 5;
        // printf("Media X: %d\n", mean);

        // mudando para escala -255 +255
        // Centralizar em torno de zero
        int centro = 2048;
        int delta = mean - centro; // Agora varia de -2048 a +2047

        int scaled_value = (delta * 255) / 2048; 
        // scaled_value agora vai aproximadamente de -255 a +255

        // Vendo se esta na zona morta -30 a +30
        if (scaled_value >= -30 && scaled_value <= 30) {
            scaled_value = 0;
        }

        recebido.button = 0;      // 0 para eixo X
        recebido.value = scaled_value;

        if (recebido.value != 0){
            xQueueSend(xQueue, &recebido, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void y_task(void *p) {
    btn_t recebido;

    static int samples[5] = {0, 0, 0, 0, 0};
    static int count = 0;

    while (1) {
        adc_select_input(1);
        int valor_adc = adc_read();

        if (count < 4) {
            count ++;
        } else{
            count = 0;
        }
        samples[count] = valor_adc;
        int mean = (samples[0] + samples[1] + samples[2] + samples[3] + samples[4]) / 5;
        // printf("Media Y: %d\n", mean);

        // mudando para escala -255 +255
        // Centralizar em torno de zero
        int centro = 2048;
        int delta = mean - centro; // Agora varia de -2048 a +2047

        int scaled_value = (delta * 255) / 2048; 
        // scaled_value agora vai aproximadamente de -255 a +255

        // Vendo se esta na zona morta -30 a +30
        if (scaled_value >= -30 && scaled_value <= 30) {
            scaled_value = 0;
        }

        recebido.button = 1;      // 1 para eixo Y
        recebido.value = scaled_value;
        if (recebido.value != 0){
            xQueueSend(xQueue, &recebido, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

int main() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(VRY);
    adc_gpio_init(VRX);
    oled1_btn_led_init();

    gpio_set_irq_enabled_with_callback(START_BTN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);
    gpio_set_irq_enabled(BTN_HOME, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_1, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_2, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_A, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(BTN_B, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    xQueue = xQueueCreate(32, sizeof(btn_t)); // Criando fila para mandar valores dos botões

    xTaskCreate(uart_task, "uart task", 4095, NULL, 1, NULL);
    xTaskCreate(x_task, "x task", 4095, NULL, 1, NULL);
    xTaskCreate(y_task, "y task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}

// Fazer duas filas? 
// Uma uart task para tudo? 
// lugar para colocar o value do botão como 0
// Como fazer o botão de ligar? 
// fall e rise botão 
// 