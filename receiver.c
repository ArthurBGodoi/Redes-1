#include "proto.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    Pacote p;
    socklen_t len = sizeof(addr);
    ssize_t n = recvfrom(sock, &p, sizeof(p), 0, (struct sockaddr*)&addr, &len);

    if (n < 4) {
        printf("Pacote muito pequeno!\n");
        return 1;
    }

    if (p.marcador != MARCADOR_INICIO) {
        printf("Marcador inválido\n");
        return 1;
    }

    uint8_t seq, tipo;
    extrair_seq_tipo(p.seq_tipo, &seq, &tipo);

    uint8_t chksum = calcular_checksum(p.tamanho, p.seq_tipo, p.dados);
    if (chksum != p.checksum) {
        printf("Checksum inválido\n");
        return 1;
    }

    printf("Recebido pacote:\n");
    printf("- Sequência: %u\n", seq);
    printf("- Tipo: %u\n", tipo);
    printf("- Dados: %.*s\n", p.tamanho, p.dados);

    close(sock);
    return 0;
}