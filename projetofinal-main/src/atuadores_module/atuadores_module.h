/**
 * @file atuadores_module.h
 * @brief Módulo para controle de atuadores (LED, Buzzer, Servo)
 */

#ifndef ATUADORES_MODULE_H
#define ATUADORES_MODULE_H

#include <stdbool.h>
#include "pico/stdlib.h"

// ==================== DEFINIÇÕES DE PINOS ====================
#define LED_PIN 13
#define BUZZER_PIN 10

// ==================== PARÂMETROS DO SISTEMA ====================
#define ANGULO_MIN 30.0f
#define ANGULO_MAX 45.0f
#define ANGULO_ALVO 37.5f

// ==================== FUNÇÕES PÚBLICAS ====================

/**
 * @brief Inicializa os pinos de alerta (LED e Buzzer)
 */
void alertas_init(void);

/**
 * @brief Ativa ou desativa os alertas (LED e Buzzer)
 * @param ativar true para ativar, false para desativar
 */
void alertas_set(bool ativar);

/**
 * @brief Alterna o estado do buzzer (toggle)
 */
void buzzer_toggle(void);

/**
 * @brief Verifica se o ângulo está dentro da faixa aceitável
 * @param angulo Ângulo atual em graus
 * @return true se está na faixa (30-45°), false caso contrário
 */
bool angulo_na_faixa(float angulo);

/**
 * @brief Calcula o ângulo do servo para corrigir a posição
 * @param angulo_atual Ângulo atual da cama
 * @return Ângulo para o servo motor (0-180)
 */
uint calcular_angulo_servo(float angulo_atual);

#endif // ATUADORES_MODULE_H
