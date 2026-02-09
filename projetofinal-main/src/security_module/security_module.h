/**
 * @file security_module.h
 * @brief Módulo de segurança com criptografia AES CBC
 */

#ifndef SECURITY_MODULE_H
#define SECURITY_MODULE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ==================== FUNÇÕES PÚBLICAS ====================

/**
 * @brief Criptografa uma mensagem usando AES CBC com PKCS7 padding
 * @param message Mensagem em texto plano para criptografar
 * @param output Buffer de saída para dados criptografados (deve ter pelo menos 128 bytes)
 * @param output_len Ponteiro para armazenar o tamanho dos dados criptografados
 * @return true se criptografou com sucesso, false caso contrário
 */
bool security_encrypt_message(const char* message, uint8_t* output, size_t* output_len);

/**
 * @brief Descriptografa uma mensagem usando AES CBC com PKCS7 padding
 * @param encrypted Dados criptografados
 * @param encrypted_len Tamanho dos dados criptografados
 * @param output Buffer de saída para mensagem descriptografada
 * @param output_size Tamanho do buffer de saída
 * @return true se descriptografou com sucesso, false caso contrário
 */
bool security_decrypt_message(const uint8_t* encrypted, size_t encrypted_len, char* output, size_t output_size);

#endif // SECURITY_MODULE_H
