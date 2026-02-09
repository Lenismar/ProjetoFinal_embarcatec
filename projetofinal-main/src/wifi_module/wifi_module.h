/**
 * @file wifi_module.h
 * @brief Módulo de conexão WiFi para Raspberry Pi Pico W
 */

#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include <stdbool.h>

// ==================== CONFIGURAÇÕES WiFi ====================
#define WIFI_SSID "brisa-580702"
#define WIFI_PASSWORD "tfvi9c88"

// ==================== FUNÇÕES PÚBLICAS ====================

/**
 * @brief Conecta ao WiFi com sistema de retry
 * @return true se conectou com sucesso, false caso contrário
 */
bool conectar_wifi(void);

/**
 * @brief Obtém o endereço IP local
 * @return String com o IP local ou string vazia se não conectado
 */
char* obter_ip_local(void);

/**
 * @brief Verifica se o WiFi está conectado
 * @return true se conectado, false caso contrário
 */
bool wifi_esta_conectado(void);

/**
 * @brief Define o estado de conexão WiFi
 * @param conectado true se conectado, false caso contrário
 */
void wifi_set_conectado(bool conectado);

#endif // WIFI_MODULE_H
