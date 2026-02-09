/**
 * @file sensores_uart_module.c
 * @brief Implementação do módulo de sensores, UART e botões
 */

#include "sensores_uart_module.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

// ==================== VARIÁVEIS PRIVADAS ====================

// Controle de transmissão UART via interrupção
static volatile bool uart_transmissao_ativa = false;

// ==================== CALLBACK INTERRUPÇÃO BOTÕES ====================

static void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == BOTAO_A_PIN) {
        // Botão A pressionado - iniciar transmissão UART
        uart_transmissao_ativa = true;
        printf("[IRQ] Botão A - Transmissão UART INICIADA\n");
    } else if (gpio == BOTAO_B_PIN) {
        // Botão B pressionado - parar transmissão UART
        uart_transmissao_ativa = false;
        printf("[IRQ] Botão B - Transmissão UART PARADA\n");
    }
}

// ==================== IMPLEMENTAÇÃO ====================

void i2c0_init_sensors(void) {
    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA_PIN);
    gpio_pull_up(I2C0_SCL_PIN);
}

void i2c1_init_display(void) {
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA_PIN);
    gpio_pull_up(I2C1_SCL_PIN);
}

void i2c_scan(i2c_inst_t *i2c, const char* bus_name) {
    printf("[I2C] Scanning %s...\n", bus_name);
    for (int addr = 0; addr < 128; addr++) {
        uint8_t data;
        int ret = i2c_read_blocking(i2c, addr, &data, 1, false);
        if (ret >= 0) {
            printf("[I2C] Dispositivo encontrado em 0x%02X\n", addr);
        }
    }
    printf("[I2C] Scan completo\n");
}

void uart_esp_init(void) {
    // Inicializar UART1 para comunicação com ESP32
    uart_init(UART_ESP_ID, UART_ESP_BAUD_RATE);
    
    // Configurar pinos GPIO para UART
    gpio_set_function(UART_ESP_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_ESP_RX_PIN, GPIO_FUNC_UART);
    
    // Configurar formato de dados: 8 bits, 1 stop bit, sem paridade
    uart_set_format(UART_ESP_ID, 8, 1, UART_PARITY_NONE);
    
    // Habilitar FIFO
    uart_set_fifo_enabled(UART_ESP_ID, true);
    
    printf("[UART] Inicializado para ESP32 (TX=GP%d, RX=GP%d, %d baud)\n", 
           UART_ESP_TX_PIN, UART_ESP_RX_PIN, UART_ESP_BAUD_RATE);
}

void uart_esp_enviar_dados(float temperatura, float umidade, float angulo, bool alerta) {
    // Verificar se transmissão está habilitada (controlada por interrupção)
    if (!uart_transmissao_ativa) {
        return;  // Transmissão desabilitada pelo Botão B
    }
    
    char buffer[64];
    
    // Formato: TEMP,UMID,ANGULO,ALERTA\n
    int len = snprintf(buffer, sizeof(buffer), "%.1f,%.1f,%.1f,%d\n", 
                       temperatura, umidade, angulo, alerta ? 1 : 0);
    
    // Enviar via UART
    uart_write_blocking(UART_ESP_ID, (const uint8_t*)buffer, len);
    
    printf("[UART->ESP] Enviado: %s", buffer);
}

void botoes_init(void) {
    // Configurar Botão A (pino 5)
    gpio_init(BOTAO_A_PIN);
    gpio_set_dir(BOTAO_A_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_A_PIN);
    
    // Configurar Botão B (pino 6)
    gpio_init(BOTAO_B_PIN);
    gpio_set_dir(BOTAO_B_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_B_PIN);
    
    // Configurar callback de interrupção para ambos os botões
    gpio_set_irq_enabled_with_callback(BOTAO_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(BOTAO_B_PIN, GPIO_IRQ_EDGE_FALL, true);
    
    printf("[BOTOES] Inicializados (A=GP%d, B=GP%d) com interrupção\n", BOTAO_A_PIN, BOTAO_B_PIN);
}

bool uart_transmissao_esta_ativa(void) {
    return uart_transmissao_ativa;
}
