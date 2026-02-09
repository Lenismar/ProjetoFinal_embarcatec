/**
 * @file mqtt_module.c
 * @brief Implementação do módulo MQTT com criptografia AES
 */

#include "mqtt_module.h"
#include "security_module/security_module.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h"

// ==================== VARIÁVEIS GLOBAIS ====================
static MQTT_STATE_T mqtt_state = {0};

// ==================== FUNÇÕES PRIVADAS ====================

// Callback de conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    switch(status) {
        case MQTT_CONNECT_ACCEPTED:
            printf("[MQTT] ✅ CONECTADO AO BROKER: %s\n", MQTT_BROKER);
            printf("[MQTT] ✅ PRONTO PARA PUBLICAR DADOS\n");
            mqtt_state.connected = true;
            
            // Publicar mensagem de status
            mqtt_publish_message(TOPIC_STATUS, "online");
            break;
            
        case MQTT_CONNECT_DISCONNECTED:
            printf("[MQTT] ❌ DESCONECTADO DO BROKER\n");
            mqtt_state.connected = false;
            break;

        case MQTT_CONNECT_TIMEOUT:
            printf("[MQTT] ❌ TIMEOUT - Falha na conexão ao broker\n");
            mqtt_state.connected = false;
            break;

        default:
            printf("[MQTT] ERRO: Status desconhecido (%d)\n", status);
            mqtt_state.connected = false;
            break;
    }
}

// Callback DNS resolvido
static void dns_resolved_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    printf("[DNS] Callback chamado para: %s\n", name ? name : "NULL");
    
    if(ipaddr) {
        printf("[DNS] ✅ Resolvido: %s -> %s\n", name, ipaddr_ntoa(ipaddr));
        
        // Copiar endereço IP
        ip_addr_copy(mqtt_state.remote_addr, *ipaddr);

        // Liberar cliente MQTT anterior se existir
        if (mqtt_state.mqtt_client) {
            printf("[MQTT] Liberando cliente anterior...\n");
            mqtt_disconnect(mqtt_state.mqtt_client);
            mqtt_client_free(mqtt_state.mqtt_client);
            mqtt_state.mqtt_client = NULL;
        }

        // Criar cliente MQTT
        printf("[MQTT] Criando novo cliente MQTT...\n");
        mqtt_state.mqtt_client = mqtt_client_new();
        if (!mqtt_state.mqtt_client) {
            printf("[MQTT] ❌ Falha ao criar cliente\n");
            return;
        }
        printf("[MQTT] Cliente criado com sucesso\n");

        // Configurar informações do cliente
        struct mqtt_connect_client_info_t ci;
        memset(&ci, 0, sizeof(ci));
        ci.client_id = MQTT_CLIENT_ID;
        ci.keep_alive = 60;
        
        printf("[MQTT] Conectando ao broker %s:%d com client_id=%s...\n", 
             ipaddr_ntoa(ipaddr), MY_MQTT_PORT, MQTT_CLIENT_ID);
        
        // Conectar ao broker
        err_t err = mqtt_client_connect(mqtt_state.mqtt_client, ipaddr, MY_MQTT_PORT, mqtt_connection_cb, NULL, &ci);
        if (err != ERR_OK) {
            printf("[MQTT] ❌ Erro ao conectar: %d\n", err);
        } else {
            printf("[MQTT] Conexão iniciada, aguardando callback...\n");
        }
    } else {
        printf("[DNS] ❌ Falha ao resolver %s\n", name);
    }
}

// ==================== IMPLEMENTAÇÃO PÚBLICA ====================

MQTT_STATE_T* mqtt_get_state(void) {
    return &mqtt_state;
}

void mqtt_publish_message(const char* topic, const char* message) {
    if (!mqtt_state.mqtt_client || !mqtt_state.connected || !mqtt_client_is_connected(mqtt_state.mqtt_client)) {
        printf("[MQTT] Cliente não conectado\n");
        return;
    }

    // Criptografar mensagem usando módulo de segurança
    uint8_t encrypted_buffer[128] = {0};
    size_t encrypted_len = 0;
    
    if (!security_encrypt_message(message, encrypted_buffer, &encrypted_len)) {
        printf("[MQTT] Erro ao criptografar mensagem\n");
        return;
    }

    // Log detalhado antes da publicação
    printf("[DEBUG] Publicando MQTT | Tópico: '%s' | Mensagem criptografada (hex): ", topic ? topic : "(null)");
    for (size_t i = 0; i < encrypted_len; i++) printf("%02X", encrypted_buffer[i]);
    printf(" | Tamanho: %u\n", (unsigned int)encrypted_len);

    // Publicar dados criptografados
    err_t err = mqtt_publish(mqtt_state.mqtt_client, topic, (const char*)encrypted_buffer, encrypted_len, 0, 0, NULL, NULL);
    if(err != ERR_OK) {
        printf("[MQTT] Erro ao publicar: %d (Tópico: %s)\n", err, topic);
    } else {
        printf("[MQTT] Publicado em %s (dados criptografados)\n", topic);
    }
}

void conectar_mqtt(void) {
    printf("[MQTT] Funcao conectar_mqtt() chamada\n");
    fflush(stdout);
    
    if (!mqtt_state.wifi_connected) {
        printf("[MQTT] Não conectado ao WiFi, não é possível conectar ao broker.\n");
        return;
    }
    printf("[MQTT] WiFi OK, iniciando conexão MQTT...\n");
    printf("[MQTT] Resolvendo DNS para %s...\n", MQTT_BROKER);
    fflush(stdout);
    
    cyw43_arch_poll();  // Garantir que o stack está atualizado
    
    err_t dns_err = dns_gethostbyname(MQTT_BROKER, &mqtt_state.remote_addr, dns_resolved_cb, NULL);
    printf("[MQTT] dns_gethostbyname retornou: %d\n", dns_err);
    
    if(dns_err == ERR_OK) {
        printf("[MQTT] DNS já em cache: %s\n", ipaddr_ntoa(&mqtt_state.remote_addr));
        dns_resolved_cb(MQTT_BROKER, &mqtt_state.remote_addr, NULL);
    } else if(dns_err == ERR_INPROGRESS) {
        printf("[MQTT] Resolução DNS em progresso, aguardando callback...\n");
    } else {
        printf("[MQTT] Erro ao iniciar resolução DNS: %d\n", dns_err);
    }
}

bool mqtt_esta_conectado(void) {
    return mqtt_state.connected;
}

void mqtt_set_wifi_conectado(bool conectado) {
    mqtt_state.wifi_connected = conectado;
}
