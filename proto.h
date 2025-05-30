#ifndef PROTO_H
#define PROTO_H

#include <stdint.h>

#define MARCADOR_INICIO 0x7E
#define MAX_DADOS 127

// Montar o byte de sequência (5 bits) + tipo (4 bits)
uint8_t montar_seq_tipo(uint8_t seq, uint8_t tipo);
void extrair_seq_tipo(uint8_t seq_tipo, uint8_t *seq, uint8_t *tipo);

// Checksum baseado em tamanho, seq_tipo e dados
uint8_t calcular_checksum(uint8_t tamanho, uint8_t seq_tipo, uint8_t *dados);

typedef struct {
    uint8_t marcador; // 0x7E
    uint8_t tamanho;  // 7 bits
    uint8_t seq_tipo; // 5 bits seq + 4 bits tipo
    uint8_t checksum;
    uint8_t dados[MAX_DADOS];
} __attribute__((packed)) Pacote;

#endif
