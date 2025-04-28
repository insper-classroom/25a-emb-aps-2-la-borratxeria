# RP2040 freertos with OLED1

basic freertos project, with code quality enabled.

Link do Design: https://www.thingiverse.com/thing:2619161

# Controle de Mario Kart Wii com Raspberry Pi Pico

> **Descrição rápida:** Interface de controle customizada para Mario Kart Wii através do emulador Dolphin usando Raspberry Pi Pico, MPU-6050, botões e comunicação UART→PC.

---

## 📑 Sumário

1. [🎮 Jogo](#🎮-jogo)  
2. [💡 Ideia do Controle](#💡-ideia-do-controle)  
3. [🎛️ Inputs e Outputs](#🎛️-inputs-e-outputs)  
4. [🔗 Protocolo Utilizado](#🔗-protocolo-utilizado)  
5. [⚙️ Arquitetura do Firmware](#⚙️-arquitetura-do-firmware)  
   - [Diagrama de Blocos](#diagrama-de-blocos)  
   - [Tasks (FreeRTOS)](#⚙️-tasks-freertos)  
   - [Fila](#🗄️-fila)  
   - [ISRs](#⏱️-isrs)  
6. [🖼️ Imagens](#🖼️-imagens)  
   - [Protótipo Real](#protótipo-real)  
 


---

## 🎮 Jogo

![Capa do Jogo](/img/capa.png)  

**Título:** Mario Kart Wii  
**Gênero:** Corrida / Party Game  
**Plataforma Original:** Nintendo Wii  
**Link de Download:** https://www.romspedia.com/roms/nintendo-wii/mario-kart-wii

### Descrição Geral
Mario Kart Wii é um clássico jogo de corrida estrelado por personagens do universo Mario. Lançado em 2008, incorpora personagens jogáveis da série Mario, que participam de corridas de kart em 32 pistas de corrida diferentes usando power-ups para dificultar os adversários ou ganhar vantagens.

### Controle Nativo Wii
O Wii Remote utiliza os seguintes botões principais em Mario Kart Wii:

- **A**: Acelerar / Selecionar  
- **B** (gatilho): Freio / Ré  
- **+ / –**: Pausar / Menu  
- **D-Pad**: Itens rápidos / Seleção de opções  
- **1 / 2**: Uso de Power-Ups adicionais  
- **Home**: Botão sistema (não usado no jogo)  
- **Tilting**: Girar o volante inclinando o controle (steering)

### Uso via Emulador
Por usar o computador para desenvolver o código para controle, usamos o **Dolphin Emulator** para emular o console Wii:

- **Emulador:** https://br.dolphin-emu.org/download/?cr=br  
- **Configuração:** Dentro do emulador fizemos o mapeamento de teclas para correspondência com nosso protocolo UART. 

### Mapeamento de Teclas  
No Dolphin, associamos os eventos vindos do Raspberry Pi Pico às teclas do teclado que simulam o Wii Remote:

| Função Wii      | Tecla de Teclado | Protocolo (button code) |
| --------------- | ---------------- | ----------------------- |
| Acelerar (A)    | Z                | 3                       |
| Freio (B)       | X                | 4                       |
| Itens (+)       | Enter            | 7                       |
| Itens (–)       | Right Shift      | 2                       |
| Power-Up 1 (1)  | C                | 5                       |
| Power-Up 2 (2)  | V                | 6                       |
| Tilt Esquerda   | ←                | 8                       |
| Tilt Direita    | →                | 9                       | 

---

## 💡 Ideia do Controle

![Volante Wii Original](/img/wheel.png)  

O nosso projeto recria a experiência do volante do Mario Kart Wii, mas de forma totalmente customizável e sem fios. Em vez de usar apenas o D-Pad para fazer curvas, você poderá inclinar o controle no ar e sentir o volante de verdade — assim como no console original.

### Objetivo Principal
Criar um periférico que combine:
- **Controles físicos clássicos** (botões A, B, 1, 2, +, –, Home)  
- **Steering real via giroscópio** (MPU-6050)  
- **Conectividade sem fio** (módulo Bluetooth)  


### Motivação e Diferenciais
- **Imersão aprimorada:** o giroscópio permite curvas naturais, eliminando a dependência do D-Pad.  
- **Sem fios:** graças ao Bluetooth, o jogador não fica preso a cabos nem a uma única estação.  
 
---

## 🎛️ Inputs e Outputs

- **MPU-6050** (giroscópio → tilt/volante)  
- **ADC**  (Joystick → mouse do computador) 
- **Botões com LED** (Click → Acionamento da tecla) 
- **LED RGB** (Verde → Conectado com o Bluetooth) / (Vermelho → Não conectado com o Bluetooth)
- **Switch On/Off** (Conecta/Desconecta bateria do controle)  
- **Microswitch** (Click → Acionamento da tecla)  

---


## 🔗 Protocolo Utilizado

### Formato dos Pacotes
Cada evento enviado pelo Raspberry Pi Pico é composto por 4 bytes:

| Byte índice | Significado                             | Exemplo       |
| ----------- | --------------------------------------- | ------------- |
| **0**       | Código do botão/ação (`evt.button`)     | `3` (A)       |
| **1**       | LSB do valor (`evt.value & 0xFF`)       | `0x10`        |
| **2**       | MSB do valor (`(evt.value >> 8) & 0xFF`)| `0x00`        |
| **3**       | Terminador fixo (`0xFF`)                | `0xFF`        |

- O byte `0xFF` atua como marcador de fim de pacote, sem uso de checksum adicional.

### UART / USB CDC
- **Velocidade (Baud Rate):** 115200 bps  
- **Interface no Pico:**  
  - Inicialização via `stdio_init_all()` com CDC USB habilitado  
  - Saída de dados usando `putchar_raw()`  
- **Configuração no PC (Python):**  
  ```python
  ser = serial.Serial(port_name, baudrate=115200, timeout=1)
  ```
- **Timeout:** 1 segundo para evitar bloqueios de leitura.

### Fluxo de Comunicação (Unidirecional)
1. **Geração de Eventos**  
   - ISRs de botão e tarefas (`x_task`, `y_task`, `mpu6050_task`) empilham estruturas `btn_t` em `xQueue`.  
2. **Envio pela UART**  
   - `uart_task` consome eventos da fila e transmite os 4 bytes de cada pacote.  
3. **Leitura no Host**  
   - O script Python aguarda o byte `0xFF` como sincronizador, então lê os 3 bytes anteriores.  
   - Os dados brutos são convertidos em `(button, value)` via `parse_data()`.  
4. **Ação no PC**  
   - Com base no código e no valor, o Python usa `pyautogui` para simular pressionamentos de tecla ou movimento de cursor.


---

## ⚙️ Arquitetura do Firmware

### Diagrama de Blocos

![image](https://github.com/user-attachments/assets/2f14cd80-f52c-4f1f-9467-a19a93743546)


### ⚙️ Tasks (FreeRTOS)

| Task Name        | Descrição                                                                                     |
| ---------------- | --------------------------------------------------------------------------------------------- |
| `uart_task`      | • Aguarda em `xQueueReceive()` (`portMAX_DELAY`)<br>• Para cada `btn_t` retirado da fila:<br>  – Serializa: `button` (1 byte), `value` (LSB, MSB), terminador `0xFF`<br>  – Envia via `putchar_raw()` (USB CDC/UART) |
| `x_task`         | • A cada 50 ms lê ADC0 (VRX)<br>• Coleta 5 amostras, faz média e centraliza em zero<br>• Se `scaled != 0`, empacota em `btn_t { button = 0; value = scaled }` e envia para a fila |
| `y_task`         | • A cada 50 ms lê ADC1 (VRY)<br>• Coleta 5 amostras, faz média e centraliza em zero<br>• Se `scaled != 0`, empacota em `btn_t { button = 1; value = -scaled }` e envia para a fila |
| `mpu6050_task`   | • Inicializa I²C a 400 kHz e reseta MPU-6050<br>• A cada 10 ms lê acelerômetro e giroscópio<br>• Atualiza AHRS (Fusion) e converte para ângulos Euler<br>• Detecta tilt: se roll < threshold envia `{ button = 8, value = 1 }`; se saiu do tilt, envia `{ button = 8, value = 0 }`; idem para código 9 (direita) |


---

### 🗄️ Fila

- **Nome:** `xQueue`  
- **Tipo de mensagem:** `btn_t` (estrutura com `int button; int value;`)  
- **Comprimento:** 32 elementos  
- **Uso:** centraliza todos os eventos (botões, eixos, tilt) antes do envio pelo `uart_task`  

```c
xQueue = xQueueCreate(32, sizeof(btn_t));
```

---

### ⏱️ ISRs

- **Função:** `btn_callback(uint gpio, uint32_t events)`  
- **Configuração de IRQ:**  
  ```c
  gpio_set_irq_enabled_with_callback(START_BTN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true, &btn_callback);
  // idem para BTN_HOME, BTN_A, BTN_B, BTN_1, BTN_2
  ```
- **Comportamento:**  
  1. Detecta borda de subida (`events == GPIO_IRQ_EDGE_RISE`) ou descida (`GPIO_IRQ_EDGE_FALL`).  
  2. Constrói `btn_t evt` com:  
     - `evt.button` baseado no GPIO (ex. `3` para A, `4` para B, etc.)  
     - `evt.value = 1` (pressionado) ou `0` (solto)  
  3. Envia para a fila com `xQueueSendFromISR(xQueue, &evt, NULL);`  

Isso garante resposta imediata a mudanças de estado dos botões, desacoplando a lógica de leitura do envio serial.  
 

---

## 🖼️ Imagens


### Protótipo Real

![Controle montado](./imgs/prototipo.jpg)

---
