#ifndef CLIENTE_H
#define CLIENTE_H

#include "protocolo.h"
#include <termios.h>

// Caminho padrão para salvar os arquivos de tesouro
#define DIRETORIO_TESOUROS "./transferidos/"

// Protótipos principais
int conectar_com_servidor(struct_cliente* cliente, const char* ip_servidor, int porta);
int requisitar_inicio_jogo(struct_cliente* cliente);
int gerenciar_comando_movimento(struct_cliente* cliente, char comando);
int transmitir_movimento(struct_cliente* cliente, mensagem_type tipo_movimento);
int gerenciar_resposta_servidor(struct_cliente* cliente);
int baixar_tesouro(struct_cliente* cliente);
int salvar_tesouro(struct_cliente* cliente, const char* nome_tesouro, mensagem_type tipo, uint64_t tamanho);

// Interface e mapa
void exibir_mapa_cliente(struct_cliente* cliente);
void imprimir_opcoes_movimento();
void reseta_interface();
const char* converter_direcao(mensagem_type tipo);

// Terminal
void configurar_terminal_raw(struct termios *orig);
void restaurar_modo_terminal(struct termios *orig);

// Tesouros
void visualizar_tesouro(const char* nome_tesouro, const char* caminho_arquivo, mensagem_type tipo);
void visualizar_texto(const char *caminho_arquivo);
void visualizar_imagem(const char *caminho);
void visualizar_video(const char *caminho);

// Auxiliares internas ao cliente
int buscar_tesouro_mapa(struct_coordenadas local_tesouro[MAX_TESOUROS], int numero_tesouros, int x, int y);
int registrar_tesouro(struct_mapa *clientMap);
void atualizar_mapa(struct_mapa *clientMap, struct_frame_mapa frameMap, int *newTreasure);

#endif // CLIENTE_H
