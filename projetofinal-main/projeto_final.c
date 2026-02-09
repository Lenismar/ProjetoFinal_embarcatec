/**
 * Projeto: Sistema de Monitoramento de Leito Hospitalar com MQTT

 * Funcionalidades:
 * - Leitura do acelerômetro MPU6050 para monitorar ângulo da cama
 * - Leitura do sensor de temperatura/umidade AHT10
 * - Display OLED para visualização dos dados
 * - Alerta visual (LED) e sonoro (buzzer) quando fora da faixa 30°-45°
 * - Controle de servo motor para ajuste de angulação
 * - Publicação de dados via MQTT (temperatura, umidade, ângulo)
 * - Conectividade WiFi para monitoramento remoto
 * 
 * 
 * Tópicos MQTT:
 * -  hospital/cama01/temperatura 
 * -  hospital/cama01/umidade
 * -  hospital/cama01/angulo
 * -  hospital/cama01/status
 * -  hospital/cama01/alerta
 * 
 * Pinagem:
 * - I2C0 (MPU6050 + AHT10): SDA=GPIO0, SCL=GPIO1
 * - I2C1 (Display OLED): SDA=GPIO14, SCL=GPIO15
 * - Servo Motor: GPIO2
 * - LED Alerta: GPIO13
 * - Buzzer: GPIO10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"
#include <stdbool.h>

// Includes do FreeRTOS (tasks, filas, semáforos e timers)
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

// Drivers dos sensores e periféricos que usamos no projeto
#include "mpu6050.h"
#include "aht10.h"
#include "ssd1306.h"
#include "servo.h"

// Nossos módulos internos (WiFi, segurança, atuadores, UART e MQTT)
#include "wifi_module/wifi_module.h"
#include "security_module/security_module.h"
#include "atuadores_module/atuadores_module.h"
#include "sensores_uart_module/sensores_uart_module.h"
#include "mqtt_module/mqtt_module.h"

// ==================== CONFIGURAÇÕES ====================
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_ADDR       0x3C

// Prioridades: quanto maior o número, mais urgente é a task
#define TASK_PRIORITY_SENSORES     (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_ALERTAS      (tskIDLE_PRIORITY + 4)
#define TASK_PRIORITY_DISPLAY      (tskIDLE_PRIORITY + 2)
#define TASK_PRIORITY_MQTT         (tskIDLE_PRIORITY + 1)
#define TASK_PRIORITY_UART         (tskIDLE_PRIORITY + 1)
#define TASK_PRIORITY_WIFI_MONITOR (tskIDLE_PRIORITY + 2)

// Memória reservada pra cada task (em words de 4 bytes)
#define STACK_SIZE_SENSORES     1024
#define STACK_SIZE_ALERTAS      512
#define STACK_SIZE_DISPLAY      1024
#define STACK_SIZE_MQTT         2048
#define STACK_SIZE_UART         512
#define STACK_SIZE_WIFI_MONITOR 1024

// De quanto em quanto tempo cada task roda (em milissegundos)
#define PERIODO_SENSORES_MS     250
#define PERIODO_ALERTAS_MS      200
#define PERIODO_DISPLAY_MS      500
#define PERIODO_MQTT_MS         5000
#define PERIODO_UART_MS         2000
#define PERIODO_WIFI_MONITOR_MS 10000

// ==================== ESTRUTURAS DE DADOS ====================

/**
 * Guarda todos os dados importantes do sistema num lugar só.
 * Como várias tasks precisam acessar esses dados, protegemos com mutex.
 */
typedef struct {
    float angulo_x;
    float temperatura;
    float umidade;
    bool  alerta_ativo;
    bool  wifi_conectado;
    bool  mqtt_conectado;
    bool  dados_validos;      // Fica true depois da primeira leitura bem-sucedida
} dados_sistema_t;

// ==================== VARIÁVEIS GLOBAIS ====================
static ssd1306_t display;
static dados_sistema_t dados_sistema = {0};

// Mutexes — cada um protege um recurso que várias tasks querem usar
static SemaphoreHandle_t mutex_i2c0 = NULL;   // Barramento dos sensores (MPU6050 + AHT10)
static SemaphoreHandle_t mutex_i2c1 = NULL;   // Barramento do display OLED
static SemaphoreHandle_t mutex_dados = NULL;  // Struct compartilhada de dados

// Referências pra cada task (útil pra debug e monitoramento)
static TaskHandle_t handle_task_sensores = NULL;
static TaskHandle_t handle_task_alertas = NULL;
static TaskHandle_t handle_task_display = NULL;
static TaskHandle_t handle_task_mqtt = NULL;
static TaskHandle_t handle_task_uart = NULL;
static TaskHandle_t handle_task_wifi_monitor = NULL;

// ==================== FUNÇÕES AUXILIARES ====================

// Copia os dados do sistema de forma segura (com mutex, pra nenhuma task pisar na outra)
static void dados_sistema_ler(dados_sistema_t *dest) {
    if (xSemaphoreTake(mutex_dados, pdMS_TO_TICKS(50)) == pdTRUE) {
        *dest = dados_sistema;
        xSemaphoreGive(mutex_dados);
    }
}

// Salva os valores novos dos sensores na struct compartilhada
static void dados_sistema_atualizar_sensores(float angulo, float temp, float umid, bool dados_ok) {
    if (xSemaphoreTake(mutex_dados, pdMS_TO_TICKS(50)) == pdTRUE) {
        dados_sistema.angulo_x = angulo;
        dados_sistema.temperatura = temp;
        dados_sistema.umidade = umid;
        dados_sistema.alerta_ativo = !angulo_na_faixa(angulo);
        dados_sistema.dados_validos = dados_ok;
        xSemaphoreGive(mutex_dados);
    }
}

static void dados_sistema_atualizar_conectividade(bool wifi, bool mqtt) {
    if (xSemaphoreTake(mutex_dados, pdMS_TO_TICKS(50)) == pdTRUE) {
        dados_sistema.wifi_conectado = wifi;
        dados_sistema.mqtt_conectado = mqtt;
        xSemaphoreGive(mutex_dados);
    }
}

// ==================== TASKS DO FREERTOS ====================

/**
 * Task dos sensores — roda a cada 250ms
 * 
 * Lê o acelerômetro (MPU6050) pra saber o ângulo da cama
 * e o sensor de temperatura/umidade (AHT10) a cada ~3 segundos.
 * Tudo passa pelo I2C0, então trava o mutex antes de cada acesso.
 */
static void task_sensores(void *pvParameters) {
    (void)pvParameters;
    
    int16_t ax, ay, az;
    float angulo_x = 0;
    float temperatura = 0, umidade = 0;
    int contador_aht = 0;
    bool dados_temp_validos = false;
    
    printf("[TASK_SENSORES] Iniciada (prioridade=%lu)\n", 
           (unsigned long)uxTaskPriorityGet(NULL));
    
    // Acorda o MPU6050 (tirar do modo sleep escrevendo 0 no registrador 0x6B)
    if (xSemaphoreTake(mutex_i2c0, pdMS_TO_TICKS(200)) == pdTRUE) {
        uint8_t reset[2] = {0x6B, 0x00};
        i2c_write_blocking(i2c0, 0x68, reset, 2, false);
        xSemaphoreGive(mutex_i2c0);
        printf("[TASK_SENSORES] MPU6050 inicializado\n");
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Reseta e configura o AHT10 (calibração inicial)
    if (xSemaphoreTake(mutex_i2c0, pdMS_TO_TICKS(200)) == pdTRUE) {
        uint8_t reset_cmd = 0xBA;
        i2c_write_blocking(i2c0, 0x38, &reset_cmd, 1, false);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        uint8_t cmd[] = {0xE1, 0x08, 0x00};
        i2c_write_blocking(i2c0, 0x38, cmd, 3, false);
        xSemaphoreGive(mutex_i2c0);
        printf("[TASK_SENSORES] AHT10 inicializado\n");
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        // Lê o acelerômetro e calcula o ângulo de inclinação
        if (xSemaphoreTake(mutex_i2c0, pdMS_TO_TICKS(100)) == pdTRUE) {
            mpu6050_read_raw(&ax, &ay, &az);
            xSemaphoreGive(mutex_i2c0);
            angulo_x = mpu6050_get_inclination(ax, ay, az);
        }
        
        // Lê temperatura e umidade a cada ~3s (não precisa ser tão frequente)
        if (++contador_aht >= 12) {
            contador_aht = 0;
            
            if (xSemaphoreTake(mutex_i2c0, pdMS_TO_TICKS(200)) == pdTRUE) {
                // Manda o AHT10 fazer uma medição
                uint8_t cmd[] = {0xAC, 0x33, 0x00};
                i2c_write_blocking(i2c0, 0x38, cmd, 3, false);
                
                // Solta o I2C enquanto espera (assim outras tasks podem usar)
                xSemaphoreGive(mutex_i2c0);
                vTaskDelay(pdMS_TO_TICKS(80));
                
                // Agora pega o resultado da medição
                if (xSemaphoreTake(mutex_i2c0, pdMS_TO_TICKS(200)) == pdTRUE) {
                    uint8_t data[6] = {0};
                    int res = i2c_read_blocking(i2c0, 0x38, data, 6, false);
                    xSemaphoreGive(mutex_i2c0);
                    
                    if (res == 6 && !(data[0] & 0x80)) {
                        uint32_t hum_raw = ((uint32_t)data[1] << 12) | 
                                          ((uint32_t)data[2] << 4) | 
                                          ((data[3] >> 4) & 0x0F);
                        uint32_t temp_raw = (((uint32_t)data[3] & 0x0F) << 16) | 
                                           ((uint32_t)data[4] << 8) | data[5];
                        
                        umidade = ((float)hum_raw / 1048576.0f) * 100.0f;
                        temperatura = ((float)temp_raw / 1048576.0f) * 200.0f - 50.0f;
                        dados_temp_validos = true;
                        
                        printf("[SENSORES] Temp: %.1fC, Umid: %.1f%%\n", 
                               temperatura, umidade);
                    } else {
                        printf("[SENSORES] AHT10 erro leitura (res=%d, status=0x%02X)\n", 
                               res, data[0]);
                    }
                }
            }
        }
        
        // Salva tudo na struct global pras outras tasks usarem
        dados_sistema_atualizar_sensores(angulo_x, temperatura, umidade, dados_temp_validos);
        
        // Espera até o próximo ciclo de 250ms
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_SENSORES_MS));
    }
}

/**
 * Task dos alertas — roda a cada 200ms (é a mais rápida)
 * 
 * Fica de olho no ângulo da cama: se saiu da faixa segura,
 * liga o LED, faz o buzzer apitar e move o servo pra tentar corrigir.
 * Quando tá tudo certo, desliga tudo e deixa o servo no neutro.
 */
static void task_alertas(void *pvParameters) {
    (void)pvParameters;
    
    int contador_buzzer = 0;
    
    printf("[TASK_ALERTAS] Iniciada (prioridade=%lu)\n", 
           (unsigned long)uxTaskPriorityGet(NULL));
    
    // Prepara os pinos do LED, buzzer e servo
    alertas_init();
    servo_init();
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        dados_sistema_t local;
        dados_sistema_ler(&local);
        
        if (local.alerta_ativo) {
            // Acende o LED vermelho
            gpio_put(LED_PIN, 1);
            
            // Buzzer bipa de forma alternada (liga/desliga a cada ciclo)
            if (++contador_buzzer % 2 == 0) {
                buzzer_toggle();
            }
            
            // Move o servo tentando corrigir a inclinação
            uint angulo_servo = calcular_angulo_servo(local.angulo_x);
            servo_set_angle(angulo_servo);
        } else {
            // Tudo normal: desliga LED e buzzer
            gpio_put(LED_PIN, 0);
            gpio_put(BUZZER_PIN, 0);
            contador_buzzer = 0;
            
            // Servo volta pro centro (90°)
            servo_set_angle(90);
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_ALERTAS_MS));
    }
}

/**
 * Task do display — atualiza a tela OLED a cada 500ms
 * 
 * Mostra na telinha tudo que tá acontecendo: ângulo da cama,
 * temperatura, umidade, se o WiFi e o MQTT estão conectados,
 * e um alerta visual quando algo sai da faixa.
 */
static void task_display(void *pvParameters) {
    (void)pvParameters;
    
    char buffer[32];
    
    printf("[TASK_DISPLAY] Iniciada (prioridade=%lu)\n", 
           (unsigned long)uxTaskPriorityGet(NULL));
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        dados_sistema_t local;
        dados_sistema_ler(&local);
        
        if (xSemaphoreTake(mutex_i2c1, pdMS_TO_TICKS(100)) == pdTRUE) {
            ssd1306_clear(&display);
            
            // Título fixo no topo
            ssd1306_draw_string(&display, 0, 0, 1, "CAMA HOSPITALAR");
            
            // Mostra "W" se WiFi ok e "M" se MQTT ok (canto superior direito)
            if (local.wifi_conectado) {
                ssd1306_draw_string(&display, 90, 0, 1, "W");
            }
            if (local.mqtt_conectado) {
                ssd1306_draw_string(&display, 100, 0, 1, "M");
            }
            
            // "F" indica que o FreeRTOS tá rodando
            ssd1306_draw_string(&display, 115, 0, 1, "F");
            
            ssd1306_draw_line(&display, 0, 10, 127, 10);
            
            // Mostra o ângulo atual da cama
            snprintf(buffer, sizeof(buffer), "Angulo: %.1f", local.angulo_x);
            ssd1306_draw_string(&display, 0, 14, 1, buffer);
            
            // Avisa se o ângulo tá fora da faixa aceitável
            if (local.alerta_ativo) {
                if (local.angulo_x < ANGULO_MIN) {
                    ssd1306_draw_string(&display, 0, 24, 1, "! BAIXO !");
                } else {
                    ssd1306_draw_string(&display, 0, 24, 1, "! ALTO !");
                }
            } else {
                ssd1306_draw_string(&display, 0, 24, 1, "OK (30-45)");
            }
            
            ssd1306_draw_line(&display, 0, 34, 127, 34);
            
            // Temperatura e umidade (mostra "Lendo..." enquanto não tem dados)
            if (local.dados_validos) {
                snprintf(buffer, sizeof(buffer), "Temp: %.1f C", local.temperatura);
                ssd1306_draw_string(&display, 0, 38, 1, buffer);
                
                snprintf(buffer, sizeof(buffer), "Umid: %.1f %%", local.umidade);
                ssd1306_draw_string(&display, 0, 48, 1, buffer);
            } else {
                ssd1306_draw_string(&display, 0, 38, 1, "Temp: Lendo...");
                ssd1306_draw_string(&display, 0, 48, 1, "Umid: Lendo...");
            }
            
            // Quadradinho piscando no canto quando tem alerta
            if (local.alerta_ativo) {
                static bool pisca = false;
                pisca = !pisca;
                if (pisca) {
                    ssd1306_draw_square(&display, 120, 0, 8, 8);
                }
            }
            
            // No rodapé mostra quantas tasks o FreeRTOS tá gerenciando
            snprintf(buffer, sizeof(buffer), "Tasks: %lu", 
                     (unsigned long)uxTaskGetNumberOfTasks());
            ssd1306_draw_string(&display, 0, 56, 1, buffer);
            
            ssd1306_show(&display);
            xSemaphoreGive(mutex_i2c1);
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_DISPLAY_MS));
    }
}

/**
 * Task do MQTT — publica dados a cada 5 segundos
 * 
 * Manda temperatura, umidade, ângulo e status de alerta pro broker.
 * Se perdeu a conexão, tenta reconectar automaticamente antes de publicar.
 */
static void task_mqtt(void *pvParameters) {
    (void)pvParameters;
    
    printf("[TASK_MQTT] Iniciada (prioridade=%lu)\n", 
           (unsigned long)uxTaskPriorityGet(NULL));
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        dados_sistema_t local;
        dados_sistema_ler(&local);
        
        if (local.wifi_conectado) {
            cyw43_arch_poll();
            
            // Se caiu a conexão com o broker, tenta reconectar
            if (!mqtt_esta_conectado()) {
                printf("[MQTT] Tentando reconectar ao broker...\n");
                conectar_mqtt();
                
                // Fica fazendo polling até conectar (ou até estourar o limite)
                for (int i = 0; i < 30; i++) {
                    cyw43_arch_poll();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    if (mqtt_esta_conectado()) break;
                }
            }
            
            // Se tá conectado, manda todos os dados pros tópicos
            if (mqtt_esta_conectado()) {
                char msg[16];
                
                // Temperatura
                snprintf(msg, sizeof(msg), "%.1f", local.temperatura);
                mqtt_publish_message(TOPIC_TEMPERATURA, msg);
                cyw43_arch_poll();
                vTaskDelay(pdMS_TO_TICKS(100));
                
                // Umidade
                snprintf(msg, sizeof(msg), "%.1f", local.umidade);
                mqtt_publish_message(TOPIC_UMIDADE, msg);
                cyw43_arch_poll();
                vTaskDelay(pdMS_TO_TICKS(100));
                
                // Ângulo
                snprintf(msg, sizeof(msg), "%.1f", local.angulo_x);
                mqtt_publish_message(TOPIC_ANGULO, msg);
                cyw43_arch_poll();
                vTaskDelay(pdMS_TO_TICKS(100));
                
                // Alerta
                mqtt_publish_message(TOPIC_ALERTA, local.alerta_ativo ? "ATIVO" : "OK");
                cyw43_arch_poll();
                vTaskDelay(pdMS_TO_TICKS(100));
                
                // Status
                mqtt_publish_message(TOPIC_STATUS, "online");
                cyw43_arch_poll();
                
                printf("[MQTT] Dados publicados: T=%.1f U=%.1f A=%.1f alerta=%s\n",
                       local.temperatura, local.umidade, local.angulo_x,
                       local.alerta_ativo ? "SIM" : "NAO");
            }
            
            // Atualiza o status de rede na struct global
            dados_sistema_atualizar_conectividade(
                wifi_esta_conectado(), mqtt_esta_conectado());
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_MQTT_MS));
    }
}

/**
 * Task da UART — envia dados pro ESP32 a cada 2 segundos
 * 
 * Monta um pacote com os dados e transmite pela serial.
 * Só envia se a transmissão estiver habilitada (controlada pelos botões).
 */
static void task_uart(void *pvParameters) {
    (void)pvParameters;
    
    printf("[TASK_UART] Iniciada (prioridade=%lu)\n", 
           (unsigned long)uxTaskPriorityGet(NULL));
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        dados_sistema_t local;
        dados_sistema_ler(&local);
        
        // Só transmite se o usuário ativou pelo botão
        if (uart_transmissao_esta_ativa()) {
            uart_esp_enviar_dados(
                local.temperatura, 
                local.umidade, 
                local.angulo_x, 
                local.alerta_ativo
            );
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_UART_MS));
    }
}

/**
 * Task do WiFi — checa a conexão a cada 10 segundos
 * 
 * Se o WiFi caiu, tenta reconectar e atualiza o status no sistema.
 * Roda com menos frequência porque reconexão é um processo pesado.
 */
static void task_wifi_monitor(void *pvParameters) {
    (void)pvParameters;
    
    printf("[TASK_WIFI_MONITOR] Iniciada (prioridade=%lu)\n", 
           (unsigned long)uxTaskPriorityGet(NULL));
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        if (!wifi_esta_conectado()) {
            printf("[WIFI_MONITOR] WiFi desconectado, tentando reconectar...\n");
            bool reconectou = conectar_wifi();
            if (reconectou) {
                mqtt_set_wifi_conectado(true);
                printf("[WIFI_MONITOR] WiFi reconectado! IP: %s\n", obter_ip_local());
            } else {
                mqtt_set_wifi_conectado(false);
                printf("[WIFI_MONITOR] Falha na reconexão WiFi\n");
            }
            
            dados_sistema_atualizar_conectividade(reconectou, mqtt_esta_conectado());
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_WIFI_MONITOR_MS));
    }
}

// ==================== INICIALIZAÇÃO DO SISTEMA ====================

/**
 * Liga e configura tudo: I2C, pinos, display, UART, botões.
 * Essa função roda uma vez só, antes de criar as tasks.
 */
static void inicializar_hardware(void) {
    // Saída serial pra debug
    stdio_init_all();
    printf("\n========== TESTE SISTEMA FreeRTOS ==========\n");
    printf("[INIT] Versão FreeRTOS: %s\n", tskKERNEL_VERSION_NUMBER);
    
    // Sobe o barramento I2C0 (onde ficam os sensores)
    i2c0_init_sensors();
    sleep_ms(100);
    i2c_scan(i2c0, "I2C0");
    
    // Sobe o barramento I2C1 (dedicado pro display OLED)
    i2c1_init_display();
    sleep_ms(100);
    
    // Configura os pinos do LED e buzzer
    alertas_init();
    
    // Inicia a comunicação serial com o ESP32
    uart_esp_init();
    
    // Configura os botões (com interrupção por hardware)
    botoes_init();
    
    // Inicializa o display OLED
    printf("[INIT] Inicializando OLED...\n");
    if (!ssd1306_init(&display, OLED_WIDTH, OLED_HEIGHT, OLED_ADDR, i2c1)) {
        printf("[ERRO] Falha ao inicializar display OLED!\n");
    }
    sleep_ms(100);
    
    // Mostra uma tela de boas-vindas enquanto o sistema carrega
    ssd1306_clear(&display);
    ssd1306_draw_string(&display, 10, 10, 2, "HOSPITAL");
    ssd1306_draw_line(&display, 0, 35, 127, 35);
    ssd1306_draw_string(&display, 5, 40, 1, "FreeRTOS Test Init...");
    ssd1306_show(&display);
    sleep_ms(1500);
    
    printf("[INIT] Hardware inicializado com sucesso\n");
}

/**
 * Tenta conectar WiFi e MQTT antes de iniciar as tasks.
 * Se falhar, o sistema continua funcionando offline.
 */
static bool inicializar_rede(void) {
    // Avisa no display que tá tentando conectar
    ssd1306_clear(&display);
    ssd1306_draw_string(&display, 5, 15, 1, "Conectando WiFi...");
    ssd1306_show(&display);
    
    bool wifi_ok = conectar_wifi();
    
    if (wifi_ok) {
        mqtt_set_wifi_conectado(true);
        printf("[INIT] WiFi conectado! IP: %s\n", obter_ip_local());
        
        ssd1306_clear(&display);
        ssd1306_draw_string(&display, 10, 10, 1, "WiFi: CONECTADO");
        char ip_str[32];
        snprintf(ip_str, sizeof(ip_str), "IP: %s", obter_ip_local());
        ssd1306_draw_string(&display, 0, 25, 1, ip_str);
        ssd1306_draw_string(&display, 5, 40, 1, "Conectando MQTT...");
        ssd1306_show(&display);
        
        // Agora tenta o MQTT
        conectar_mqtt();
        for (int i = 0; i < 50; i++) {
            cyw43_arch_poll();
            sleep_ms(100);
            if (mqtt_esta_conectado()) {
                printf("[INIT] MQTT conectado após %d ms\n", i * 100);
                break;
            }
        }
        
        if (mqtt_esta_conectado()) {
            ssd1306_clear(&display);
            ssd1306_draw_string(&display, 10, 15, 1, "WiFi: OK");
            ssd1306_draw_string(&display, 10, 30, 1, "MQTT: OK");
            ssd1306_draw_string(&display, 5, 48, 1, "Iniciando tasks...");
            ssd1306_show(&display);
        } else {
            printf("[INIT] MQTT nao conectou (continuando...)\n");
        }
    } else {
        printf("[INIT] WiFi nao conectou (continuando sem rede)\n");
        ssd1306_clear(&display);
        ssd1306_draw_string(&display, 5, 15, 1, "WiFi: FALHA");
        ssd1306_draw_string(&display, 5, 30, 1, "Modo offline");
        ssd1306_draw_string(&display, 5, 48, 1, "Iniciando tasks...");
        ssd1306_show(&display);
    }
    
    sleep_ms(1000);
    
    dados_sistema_atualizar_conectividade(wifi_ok, mqtt_esta_conectado());
    return wifi_ok;
}

// Cria os 3 mutexes que protegem os recursos compartilhados
static bool criar_mutexes(void) {
    mutex_i2c0 = xSemaphoreCreateMutex();
    mutex_i2c1 = xSemaphoreCreateMutex();
    mutex_dados = xSemaphoreCreateMutex();
    
    if (!mutex_i2c0 || !mutex_i2c1 || !mutex_dados) {
        printf("[ERRO] Falha ao criar mutexes!\n");
        return false;
    }
    
    printf("[INIT] Mutexes criados (I2C0, I2C1, Dados)\n");
    return true;
}

// Registra as 6 tasks no FreeRTOS com suas prioridades e tamanhos de pilha
static bool criar_tasks(void) {
    BaseType_t ret;
    
    // Sensores: lê MPU6050 e AHT10 pelo I2C0
    ret = xTaskCreate(task_sensores, "Sensores", STACK_SIZE_SENSORES,
                      NULL, TASK_PRIORITY_SENSORES, &handle_task_sensores);
    if (ret != pdPASS) {
        printf("[ERRO] Falha ao criar task_sensores\n");
        return false;
    }
    
    // Alertas: controla LED, buzzer e servo
    ret = xTaskCreate(task_alertas, "Alertas", STACK_SIZE_ALERTAS,
                      NULL, TASK_PRIORITY_ALERTAS, &handle_task_alertas);
    if (ret != pdPASS) {
        printf("[ERRO] Falha ao criar task_alertas\n");
        return false;
    }
    
    // Display: atualiza a tela OLED
    ret = xTaskCreate(task_display, "Display", STACK_SIZE_DISPLAY,
                      NULL, TASK_PRIORITY_DISPLAY, &handle_task_display);
    if (ret != pdPASS) {
        printf("[ERRO] Falha ao criar task_display\n");
        return false;
    }
    
    // MQTT: publica dados no broker
    ret = xTaskCreate(task_mqtt, "MQTT", STACK_SIZE_MQTT,
                      NULL, TASK_PRIORITY_MQTT, &handle_task_mqtt);
    if (ret != pdPASS) {
        printf("[ERRO] Falha ao criar task_mqtt\n");
        return false;
    }
    
    // UART: envia dados pro ESP32
    ret = xTaskCreate(task_uart, "UART", STACK_SIZE_UART,
                      NULL, TASK_PRIORITY_UART, &handle_task_uart);
    if (ret != pdPASS) {
        printf("[ERRO] Falha ao criar task_uart\n");
        return false;
    }
    
    // WiFi: fica checando se a conexão caiu
    ret = xTaskCreate(task_wifi_monitor, "WiFiMon", STACK_SIZE_WIFI_MONITOR,
                      NULL, TASK_PRIORITY_WIFI_MONITOR, &handle_task_wifi_monitor);
    if (ret != pdPASS) {
        printf("[ERRO] Falha ao criar task_wifi_monitor\n");
        return false;
    }
    
    printf("[INIT] Todas as 6 tasks criadas com sucesso\n");
    printf("  - Sensores:    prio=%d stack=%d\n", TASK_PRIORITY_SENSORES, STACK_SIZE_SENSORES);
    printf("  - Alertas:     prio=%d stack=%d\n", TASK_PRIORITY_ALERTAS, STACK_SIZE_ALERTAS);
    printf("  - Display:     prio=%d stack=%d\n", TASK_PRIORITY_DISPLAY, STACK_SIZE_DISPLAY);
    printf("  - MQTT:        prio=%d stack=%d\n", TASK_PRIORITY_MQTT, STACK_SIZE_MQTT);
    printf("  - UART:        prio=%d stack=%d\n", TASK_PRIORITY_UART, STACK_SIZE_UART);
    printf("  - WiFi Monitor:prio=%d stack=%d\n", TASK_PRIORITY_WIFI_MONITOR, STACK_SIZE_WIFI_MONITOR);
    
    return true;
}

// ==================== FUNÇÃO PRINCIPAL ====================

int main(void) {
    // Primeiro liga tudo (I2C, display, pinos, UART...)
    inicializar_hardware();
    
    // Cria os mutexes antes de qualquer coisa que use recursos compartilhados
    if (!criar_mutexes()) {
        printf("[FATAL] Nao foi possivel criar mutexes. Sistema parado.\n");
        while (1) { tight_loop_contents(); }
    }
    
    // Tenta conectar na rede (se falhar, segue em modo offline)
    inicializar_rede();
    
    // Agora cria as 6 tasks do sistema
    if (!criar_tasks()) {
        printf("[FATAL] Nao foi possivel criar tasks. Sistema parado.\n");
        while (1) { tight_loop_contents(); }
    }
    
    // Tudo pronto — entrega o controle pro scheduler do FreeRTOS
    printf("\n[INIT] ========================================\n");
    printf("[INIT] Iniciando FreeRTOS Scheduler...\n");
    printf("[INIT] Heap livre: %u bytes\n", (unsigned)xPortGetFreeHeapSize());
    printf("[INIT] ========================================\n\n");
    
    vTaskStartScheduler();
    
    // Se chegou aqui, algo deu muito errado (provavelmente faltou memória)
    printf("[FATAL] Scheduler retornou! Heap insuficiente?\n");
    while (1) { tight_loop_contents(); }
    
    return 0;
}
