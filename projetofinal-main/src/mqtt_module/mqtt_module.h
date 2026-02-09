/**
 * @file mqtt_module.h
 * @brief Módulo para comunicação MQTT com criptografia AES
 */

#ifndef MQTT_MODULE_H
#define MQTT_MODULE_H

#include <stdbool.h>
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"

// ==================== CONFIGURAÇÕES MQTT ====================
#define MQTT_BROKER "test.mosquitto.org"
#define MY_MQTT_PORT 1883
#define MQTT_CLIENT_ID "pico_hospital_bed_12345"

// ==================== TÓPICOS MQTT ====================
#define TOPIC_TEMPERATURA "hospital/cama/temperatura"
#define TOPIC_UMIDADE "hospital/cama/umidade"
#define TOPIC_ANGULO "hospital/cama/angulo"
#define TOPIC_STATUS "hospital/cama/status"
#define TOPIC_ALERTA "hospital/cama01/alerta"

// ==================== ESTRUTURA DE ESTADO ====================
typedef struct {
    mqtt_client_t *mqtt_client;
    ip_addr_t remote_addr;
    bool connected;
    bool wifi_connected;
} MQTT_STATE_T;

// ==================== FUNÇÕES PÚBLICAS ====================

/**
 * @brief Obtém o ponteiro para o estado MQTT global
 * @return Ponteiro para estrutura MQTT_STATE_T
 */
MQTT_STATE_T* mqtt_get_state(void);

/**
 * @brief Publica uma mensagem MQTT com criptografia AES
 * @param topic Tópico MQTT
 * @param message Mensagem em texto plano
 */
void mqtt_publish_message(const char* topic, const char* message);

/**
 * @brief Conecta ao broker MQTT
 */
void conectar_mqtt(void);

/**
 * @brief Verifica se o cliente MQTT está conectado
 * @return true se conectado, false caso contrário
 */
bool mqtt_esta_conectado(void);

/**
 * @brief Define o estado de conexão WiFi no módulo MQTT
 * @param conectado true se conectado, false caso contrário
 */
void mqtt_set_wifi_conectado(bool conectado);

#endif // MQTT_MODULE_H
