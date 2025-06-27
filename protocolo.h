#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>

#include <termios.h>
#include <unistd.h>      // Para usleep()
#include <sys/time.h>    // Para struct timeval
#include <strings.h>     // Para strcasecmp()
#include "rawSocket.h"   // Incluir o raw socket



#define MAX_FRAME 127               // ok
#define MAX_RETRY 3                 // ok
#define TIMEOUT_S 1                 // ok
#define MAX_ESPACO 1048576          // ok (não usa)
#define MAX_NOME 63                 // ok (não usa)
#define PORTA_CLIENTE 23623        // ok
#define PORTA_SERVIDOR 43531       // ok

#define INTERFACE "enp0s31f6"        


#define TAMANHO_MAPA 8              
#define MAX_TESOUROS 8              

#define PASTA_OBJETOS "./objetos/"  


//////////// Tipos de mensagem  ////////////

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


//////////// Tipos de erro ////////////
typedef enum {
    SEM_PERMISSAO = 0,              
    ESPACO_INSUFICIENTE = 1,        
} erro_type;


//////////// ESTRUTURAS DO PACOTE ////////////

#pragma pack(push, 1)  
typedef struct {
    
    uint8_t marcador;              
    
    uint8_t tamanho : 7;           

    uint8_t seq_inicio : 1;        
    
    uint8_t seq_fim : 4;           

    uint8_t tipo : 4;              

    uint8_t checksum;              

    uint8_t dados[127];            

} pack_t;
#pragma pack(pop)

 
//////////// Tipos de tesouro ////////////

typedef enum {
    TESOURO_TXT = 0,        
    TESOURO_IMG = 1,        
    TESOURO_VID = 2         
} tesouro_type;

//////////// Estrura de coordenada ////////////

typedef struct {            
    int x;                  
    int y;                  
} posicao_t;




//////////// Estrutura de tesouro ////////////

typedef struct {
    posicao_t posicao;              //
    char nome_tesouro[64];          //
    char patch[256];                // caminho_completo 
    uint8_t tamanho[MAX_FRAME];     //
    tesouro_type tipo;              //
    int encontrado;                 
} tesouro_t;




//////////// Estrutura do estado de protocolo ////////////

typedef struct {
    rawsocket_t rawsock;         // Raw socket context

    uint8_t seq_atual;
    unsigned char seq_esperada;

    char ip_destino[16];            // IP de destino

    unsigned short porta_destino;    // Porta de destino
    unsigned short porta_origem;     // Porta de origem

    pack_t pack;
} protocolo_type;                




//////////// Estrutura do estado do jogo ////////////

typedef struct {
    posicao_t local_player;                             
    tesouro_t tesouros[MAX_TESOUROS];                   
    int local_explorado[TAMANHO_MAPA][TAMANHO_MAPA];    
    int tesouros_achados;                               
    int partida_iniciada;                              

} struct_jogo;                                      




#pragma pack(push, 1)
typedef struct{
    posicao_t posicao_player;       
    uint8_t pegar_tesouro;          
} struct_frame_mapa;                
#pragma pack(pop)



// Estrutura para informações do mapa do cliente
typedef struct {
    posicao_t posicao_player;                               //
    posicao_t local_tesouro[MAX_TESOUROS];                  // coletar tesouro
    char local_explorado[TAMANHO_MAPA][TAMANHO_MAPA];       //
    int numero_tesouros;                                    //
} mapa_cliente_t;                                           // colletTreasures








// Estrutura para o estado do cliente
typedef struct {
    int jogo_ativo;                 //
    int tesouros_obtidos;           //
    protocolo_type protocolo;       //
    mapa_cliente_t mapa_ativo;      //
} struct_cliente;                   //







// Funcao de reenviar o pacote
int reenvio(protocolo_type* estado, pack_t pack);

// Funcao para checar a sequencia dos dados do pacote
int seqCheck(uint8_t seqAtual, uint8_t seqPack);

// Funções do protocolo 
int inicializar_protocolo(protocolo_type* estado, const char* ip_destino, unsigned short orig_port,
                                 unsigned short porta_destino, const char* interface);

// Funções para gerenciamento do protocolo, conectando a porta do cliente e do servidor
int criar_pacote(pack_t* pack, unsigned char seq, mensagem_type tipo, uint8_t* dados, unsigned short tamanho);

// Funcao para enviar um pacote
int enviar_pacote(protocolo_type* estado, const pack_t* pack); 

// Funcao que recebe um pacote
int receber_pacote(protocolo_type* estado, pack_t* pack);               

// Funcao que envia ack
int enviar_ack(protocolo_type* estado, uint8_t seq);

// Funcao que envia nack
int enviar_nack(protocolo_type* estado, uint8_t seq); 

// Funcao que envia ok ack
int enviar_ok_ack(protocolo_type* estado, uint8_t seq);

// Funcao que envia erro
int enviar_erro(protocolo_type* estado, uint8_t seq, erro_type erro);

// Funcao que espera o recebimento de um ack
int esperar_ack(protocolo_type* estado);                                

// Finaliza o protocolo fechando o raw socket
void finalizar_protocolo(protocolo_type* estado);

//////////// Funções do jogo ////////////

// Comeca o jogo definindo posicoes 0 para o jogador e sorteia os tesouros, alem de colocar cada tipo no tesouro
void setup_jogo(struct_jogo* jogo);                                     

// Realiza a movimentacao do player alem de verificar se ele bateu na parede
int move_player(struct_jogo* jogo, mensagem_type direcao); 

// Retorna o indice do tesouro encontrado
int valida_tesouro(struct_jogo* jogo, posicao_t posicao);   

// Responsavel pela interface grafica do servidor
void interface_servidor(struct_jogo* jogo);

// Responsavel por limpar a interface
void reseta_interface();                                                

//////////// Funções auxiliares ////////////

// Retorna a sequencia do pacote
uint8_t getSeq(pack_t pack);

// Retorna o tamanho do arquivo
void obter_tamanho_arquivo(const char* caminho, uint8_t tam[MAX_FRAME]);

// Determina o tipo de arquivo se e mp4, jpg ou txt
mensagem_type determinar_tipo_arquivo(const char* nome_tesouro);

// Ve se o arquivo tem espaco o suficiente
uint64_t obter_espaco_livre(const char* caminho);

#endif // PROTOCOLO_H
