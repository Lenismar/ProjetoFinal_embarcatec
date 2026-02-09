/**
 * @file atuadores_module.c
 * @brief Implementação do módulo de atuadores
 */

#include "atuadores_module.h"
#include <stdio.h>
#include "hardware/gpio.h"

// ==================== IMPLEMENTAÇÃO ====================

void alertas_init(void) {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, 0);
}

void alertas_set(bool ativar) {
    gpio_put(LED_PIN, ativar);
    gpio_put(BUZZER_PIN, ativar);
}

void buzzer_toggle(void) {
    static bool estado = false;
    estado = !estado;
    gpio_put(BUZZER_PIN, estado);
}

bool angulo_na_faixa(float angulo) {
    return (angulo >= ANGULO_MIN && angulo <= ANGULO_MAX);
}

uint calcular_angulo_servo(float angulo_atual) {
    float diferenca = ANGULO_ALVO - angulo_atual;
    
    if (diferenca > 30) diferenca = 30;
    if (diferenca < -30) diferenca = -30;
    
    float angulo_servo = 90.0f + diferenca;
    
    if (angulo_servo < 0) angulo_servo = 0;
    if (angulo_servo > 180) angulo_servo = 180;
    
    return (uint)angulo_servo;
}
