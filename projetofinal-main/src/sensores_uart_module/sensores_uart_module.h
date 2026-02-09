/**
 * @file sensores_uart_module.h
 * @brief Módulo para sensores (MPU6050, AHT10), UART e controle de botões
 */

#ifndef SENSORES_UART_MODULE_H
#define SENSORES_UART_MODULE_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

// ==================== DEFINIÇÕES DE PINOS I2C ====================
#define I2C0_SDA_PIN 0
#define I2C0_SCL_PIN 1
#define I2C1_SDA_PIN 14
#define I2C1_SCL_PIN 15

// ==================== DEFINIÇÕES UART ====================
#define UART_ESP_ID uart1
#define UART_ESP_BAUD_RATE 115200
#define UART_ESP_TX_PIN 8
#define UART_ESP_RX_PIN 9

// ==================== DEFINIÇÕES BOTÕES ====================
#define BOTAO_A_PIN 5  // Inicia transmissão UART
#define BOTAO_B_PIN 6  // Para transmissão UART

// ==================== FUNÇÕES PÚBLICAS ====================

/**
 * @brief Inicializa o barramento I2C0 para sensores (MPU6050 e AHT10)
 */
void i2c0_init_sensors(void);

/**
 * @brief Inicializa o barramento I2C1 para display OLED
 */
void i2c1_init_display(void);

/**
 * @brief Realiza scan do barramento I2C para diagnóstico
 * @param i2c Instância do I2C (i2c0 ou i2c1)
 * @param bus_name Nome do barramento para log
 */
void i2c_scan(i2c_inst_t *i2c, const char* bus_name);

/**
 * @brief Inicializa a UART para comunicação com ESP32
 */
void uart_esp_init(void);

/**
 * @brief Envia dados dos sensores para ESP32 via UART
 * @param temperatura Temperatura em °C
 * @param umidade Umidade em %
 * @param angulo Ângulo de inclinação em graus
 * @param alerta true se alerta ativo, false caso contrário
 * 
 * Formato de envio: "TEMP,UMID,ANGULO,ALERTA\n"
 * Exemplo: "25.5,60.2,35.0,0\n"
 */
void uart_esp_enviar_dados(float temperatura, float umidade, float angulo, bool alerta);

/**
 * @brief Inicializa os botões com interrupção
 */
void botoes_init(void);

/**
 * @brief Verifica se a transmissão UART está ativa
 * @return true se transmissão está ativa, false caso contrário
 */
bool uart_transmissao_esta_ativa(void);

#endif // SENSORES_UART_MODULE_H
