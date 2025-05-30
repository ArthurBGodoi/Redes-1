#include "proto.h"

uint8_t montar_seq_tipo(uint8_t seq, uint8_t tipo) {
    return ((seq & 0x1F) << 3) | (tipo & 0x0F);
}

void extrair_seq_tipo(uint8_t seq_tipo, uint8_t *seq, uint8_t *tipo) {
    *seq = (seq_tipo >> 3) & 0x1F;
    *tipo = seq_tipo & 0x0F;
}

uint8_t calcular_checksum(uint8_t tamanho, uint8_t seq_tipo, uint8_t *dados) {
    uint16_t soma = tamanho + seq_tipo;
    for (int i = 0; i < tamanho; i++)
        soma += dados[i];
    return soma % 256;
}
