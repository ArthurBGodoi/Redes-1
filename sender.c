#include "proto.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0); // UDP socket
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);

    Pacote p;
    p.marcador = MARCADOR_INICIO;
    char msg[] = "Hello from sender!";
    p.tamanho = strlen(msg);
    p.seq_tipo = montar_seq_tipo(1, 5); // seq=1, tipo=5 (DADOS)
    memcpy(p.dados, msg, p.tamanho);
    p.checksum = calcular_checksum(p.tamanho, p.seq_tipo, p.dados);

    sendto(sock, &p, 4 + p.tamanho, 0, (struct sockaddr*)&dest, sizeof(dest));
    printf("Pacote enviado!\n");
    close(sock);
    return 0;
}