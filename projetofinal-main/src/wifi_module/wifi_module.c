/**
 * @file wifi_module.c
 * @brief Implementação do módulo de conexão WiFi
 */

#include "wifi_module.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"

// ==================== VARIÁVEIS PRIVADAS ====================
static bool wifi_connected = false;

// ==================== IMPLEMENTAÇÃO ====================

bool conectar_wifi(void) {
    printf("[WiFi] Inicializando WiFi...\n");
    if (cyw43_arch_init()) {
        printf("[WiFi] Erro na inicialização do chip CYW43\n");
        return false;
    }
    
    cyw43_arch_enable_sta_mode();
    printf("[WiFi] Modo STA habilitado\n");
    
    // Aguardar estabilização do chip WiFi
    sleep_ms(1000);
    
    printf("[WiFi] SSID: %s\n", WIFI_SSID);
    printf("[WiFi] Senha: %d caracteres\n", (int)strlen(WIFI_PASSWORD));
    
    // Sistema de retry - tentar até 5 vezes
    const int MAX_TENTATIVAS = 5;
    int wifi_status = -1;
    
    for (int tentativa = 1; tentativa <= MAX_TENTATIVAS; tentativa++) {
        printf("[WiFi] Tentativa %d/%d - Conectando...\n", tentativa, MAX_TENTATIVAS);
        
        wifi_status = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 15000);
        
        if (wifi_status == 0) {
            printf("[WiFi] ✅ CONECTADO AO WIFI: %s\n", WIFI_SSID);
            printf("[WiFi] ✅ INTERNET DISPONÍVEL - PRONTO PARA MQTT\n");
            wifi_connected = true;
            return true;
        }
        
        // Interpretar código de erro
        printf("[WiFi] Falha na tentativa %d, status: %d\n", tentativa, wifi_status);
        switch(wifi_status) {
            case -1:
                printf("[WiFi] -> Erro genérico (verifique se a rede existe)\n");
                break;
            case -2:
                printf("[WiFi] -> Timeout (sinal fraco ou rede ocupada)\n");
                break;
            case -3:
                printf("[WiFi] -> Autenticação falhou (senha incorreta)\n");
                break;
            default:
                printf("[WiFi] -> Código de erro desconhecido\n");
                break;
        }
        
        if (tentativa < MAX_TENTATIVAS) {
            printf("[WiFi] Aguardando 3 segundos antes de tentar novamente...\n");
            sleep_ms(3000);
            
            // Reinicializar o chip WiFi entre tentativas
            cyw43_arch_deinit();
            sleep_ms(500);
            if (cyw43_arch_init()) {
                printf("[WiFi] Erro ao reinicializar chip\n");
                return false;
            }
            cyw43_arch_enable_sta_mode();
            sleep_ms(500);
        }
    }
    
    printf("[WiFi] ❌ FALHA após %d tentativas\n", MAX_TENTATIVAS);
    printf("[WiFi] Verifique: 1) Roteador ligado 2) SSID correto 3) Senha correta 4) Distância\n");
    cyw43_arch_deinit();
    return false;
}

char* obter_ip_local(void) {
    static char ip_str[16];
    struct netif *netif = netif_default;
    if (netif && netif_is_up(netif)) {
        snprintf(ip_str, sizeof(ip_str), "%s", ipaddr_ntoa(&netif->ip_addr));
        return ip_str;
    }
    return "";
}

bool wifi_esta_conectado(void) {
    return wifi_connected;
}

void wifi_set_conectado(bool conectado) {
    wifi_connected = conectado;
}
