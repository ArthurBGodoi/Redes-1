#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pwd.h>
#include <termios.h>
#include <unistd.h>          


// Constantes que serão usadas no protocolo

#define TAM_MAX_DADOS 127               // Tamanho máximo dos dados (127 bytes)
#define MAX_NOME 63                     // Máximo de bytes de nome
#define MAX_TIMEOUT 500000              // Quantidade máxima de retransmissões
#define DURACAO_TIMEOUT 5               // Duração do timeout


// Constantes que serão usadas no jogo

#define DIR_OBJETOS "./objetos/"        // Diretório de onde serão enviados os tesouros
#define MAX_GRID 8                      // Tamanho máximo da grid do mapa
#define MAX_TESOUROS 8                  // Quantidade máxima de tesouros no mapa



// Todos os tipos de mensagem e seus respectivos códigos

typedef enum {
    MSG_ACK = 0,                   
    MSG_NACK = 1,                  
    MSG_OK_ACK = 2,                
    MSG_START = 3,          
    MSG_TAMANHO = 4,               
    MSG_DADOS = 5,                 
    MSG_TEXTO_ACK_NOME = 6,        
    MSG_VIDEO_ACK_NOME = 7,        
    MSG_IMAGEM_ACK_NOME = 8,       
    MSG_FIM_ARQUIVO = 9,           
    MSG_MOVE_DIREITA = 10,      
    MSG_MOVE_CIMA = 11,         
    MSG_MOVE_BAIXO = 12,        
    MSG_MOVE_ESQUERDA = 13,     
    MSG_INTERFACE = 14,         
    MSG_ERRO = 15,                 
} mensagem_type;

// Todos os tipos de erros e seus respectivos códigos

typedef enum {
    ARQUIVO_INEXISTENTE = 1,
    SEM_ACESSO = 2,
    SEM_ARMAZENAMENTO = 3,
    ARQUIVO_PESADO = 4,
    MOVIMENTO_PROIBIDO = 5
} erro_type;

// Frame do pacote

#pragma pack(push, 1)               // Para alinhar sem padding
typedef struct {
    
    uint8_t marcador_inicio;        // 8 bits 
    uint8_t tamanho : 7;            // 7 bits 
    uint8_t sequencia_bit_0 : 1;    // 1 bit  (primeiro bit da sequencia) 
    uint8_t sequencia_bits_4 : 4;   // 4 bits (restante da sequência)
    uint8_t tipo : 4;               // 4 bits 
    uint8_t checksum;               // 8 bits 
    uint8_t dados[127];             // Dados (127 bytes)
} struct_frame_pacote;
#pragma pack(pop)

// Estrutura de coordenadas

typedef struct {
    int x;
    int y;
} struct_coordenadas;


// Tipos de tesouro e seus respectivos códigos

typedef enum {
    TESOURO_TXT = 0,
    TESOURO_IMG = 1,
    TESOURO_MP4 = 2
} tesouro_type;

// Estrutura dos tesouros

typedef struct {
    struct_coordenadas posicao;
    uint8_t tamanho[TAM_MAX_DADOS];
    tesouro_type tipo;
    char nome_tesouro[64];
    char patch[256];
    int achado;
} struct_tesouro;

// Estrutura do protocolo e sua situação

typedef struct {
    unsigned char sequencia_atual;
    unsigned char sequencia_seguinte;
    int socket_fd;
    struct sockaddr_in ip_remoto;
    socklen_t ip_tamanho;
} struct_protocolo;

// Estrutura do jogo

typedef struct {
    int local_explorado[MAX_GRID][MAX_GRID];
    int tesouros_achados;
    int partida_iniciada;
    struct_coordenadas local_player;
    struct_tesouro tesouros[MAX_TESOUROS];
} struct_jogo;

#pragma pack(push, 1)
typedef struct{
    struct_coordenadas posicao_player;
    uint8_t pegar_tesouro;
} struct_frame_mapa;
#pragma pack(pop)

#define SOCKET_RAW SOCK_DGRAM

// Estrutura para o funcionamento do mapa

typedef struct {
    char local_explorado[MAX_GRID][MAX_GRID];
    int numero_tesouros;
    struct_coordenadas posicao_player;
    struct_coordenadas local_tesouro[MAX_TESOUROS];
} struct_mapa;

// Estrutura para a interface do cliente
typedef struct {
    int jogo_ativo;
    int tesouros_obtidos;
    struct_protocolo protocolo;
    struct_mapa mapa_ativo;
} struct_cliente;

// Funções auxiliares

uint8_t ler_sequencia(struct_frame_pacote pack);

void ler_tamanho_arquivo(const char* caminho, uint8_t tam[TAM_MAX_DADOS]);

unsigned long ler_area_livre(const char* diretorio);

int valida_arquivo(const char* caminho);

mensagem_type tipo_arquivo(const char* nome_tesouro);

// Funções do protocolo

int cria_pacote(struct_frame_pacote* pack, unsigned char seq, mensagem_type tipo, uint8_t* dados, unsigned short tamanho);

int envia_pacote(struct_protocolo* estado, const struct_frame_pacote* pack);

int recebe_pacote(struct_protocolo* estado, struct_frame_pacote* pack);

int envia_ack(struct_protocolo* estado, uint8_t seq);

int envia_nack(struct_protocolo* estado, uint8_t seq);

int envia_ok_ack(struct_protocolo* estado, uint8_t seq);

int envia_erro(struct_protocolo* estado, uint8_t seq, erro_type erro);

int aguarda_ack(struct_protocolo* estado);

// Funções do jogo

void setup_jogo(struct_jogo* jogo);

int move_player(struct_jogo* jogo, mensagem_type direcao);

int valida_tesouro(struct_jogo* jogo, struct_coordenadas posicao);

void interface_servidor(struct_jogo* jogo);

void interface_cliente(struct_cliente* cliente);

void reseta_interface();

#endif // PROTOCOLO_H
