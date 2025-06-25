#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "protocolo.h"

// Porta padrão do servidor
#define PORTA_SERVIDOR 50969

// Variáveis globais (declaração extern)
extern struct_protocolo estado_servidor;
extern struct_jogo jogo;

// Protótipos de funções

// Inicialização
int setup_servidor(void);

// Gerenciamento
int gerenciar_mensagem_cliente(void);
int gerenciar_movimento(mensagem_type direcao);

// Transmissão de dados
int transmitir_mapa_cliente(void);
int transmitir_tesouro(int indice_tesouro);
int transmitir_arquivo_tesouro(const char* caminho_arquivo, const char* nome_tesouro, mensagem_type tipo);

// Utilidades
void imprimir_movimento(const char* direcao, int sucesso);
int checar_tesouro_posicao(struct_tesouro treasures[MAX_TESOUROS], struct_coordenadas pos);

#endif // SERVIDOR_H
