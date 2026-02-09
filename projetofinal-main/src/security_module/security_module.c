/**
 * @file security_module.c
 * @brief Implementação do módulo de segurança com criptografia AES CBC
 */

#include "security_module.h"
#include <stdio.h>
#include <string.h>
#include "aes/aes.h"

// ==================== CHAVES DE CRIPTOGRAFIA ====================
// Chave e IV fixos para exemplo (substitua por valores seguros em produção)
static const uint8_t AES_KEY[16] = { 'S', 'E', 'G', 'U', 'R', 'A', 'N', 'C', 'A', '1', '2', '3', '4', '5', '6', '7' };
static const uint8_t AES_IV[16]  = { 'I', 'N', 'I', 'C', 'I', 'A', 'L', 'I', 'V', '1', '2', '3', '4', '5', '6', '7' };

// ==================== IMPLEMENTAÇÃO ====================

bool security_encrypt_message(const char* message, uint8_t* output, size_t* output_len) {
    if (!message || !output || !output_len) {
        return false;
    }

    // Copiar IV para uso (será modificado durante criptografia)
    uint8_t iv[16];
    memcpy(iv, AES_IV, 16);

    // Inicializar contexto AES
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, AES_KEY, iv);

    // Calcular tamanho com PKCS7 padding
    size_t msg_len = strlen(message);
    size_t padded_len = ((msg_len / 16) + 1) * 16;
    
    if (padded_len > 128) {
        printf("[SECURITY] Mensagem muito longa para criptografia\n");
        return false;
    }

    // Copiar mensagem para buffer de saída
    memset(output, 0, padded_len);
    memcpy(output, message, msg_len);

    // Aplicar PKCS7 padding
    uint8_t pad = padded_len - msg_len;
    for (size_t i = msg_len; i < padded_len; i++) {
        output[i] = pad;
    }

    // Criptografar
    AES_CBC_encrypt_buffer(&ctx, output, padded_len);

    *output_len = padded_len;
    return true;
}

bool security_decrypt_message(const uint8_t* encrypted, size_t encrypted_len, char* output, size_t output_size) {
    if (!encrypted || !output || encrypted_len == 0 || encrypted_len % 16 != 0) {
        return false;
    }

    if (encrypted_len > output_size) {
        return false;
    }

    // Copiar IV para uso
    uint8_t iv[16];
    memcpy(iv, AES_IV, 16);

    // Inicializar contexto AES
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, AES_KEY, iv);

    // Copiar dados para buffer de saída
    memcpy(output, encrypted, encrypted_len);

    // Descriptografar
    AES_CBC_decrypt_buffer(&ctx, (uint8_t*)output, encrypted_len);

    // Remover PKCS7 padding
    uint8_t pad = output[encrypted_len - 1];
    if (pad > 0 && pad <= 16) {
        output[encrypted_len - pad] = '\0';
    } else {
        output[encrypted_len] = '\0';
    }

    return true;
}
