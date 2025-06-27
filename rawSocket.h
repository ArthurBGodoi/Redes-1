#ifndef RAWSOCKET_H
#define RAWSOCKET_H


#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <linux/if_arp.h>    
#include <net/ethernet.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>


#define INTERFACE_PADRAO "enp0s31f6" 
#define MAXIMO_PACOTE 65536


//////////// Estrutura IP ////////////

struct cabecalho_ip {
    unsigned char versao_ihl;   
    unsigned char servico;           
    unsigned short comprimento;
    unsigned short id;         
    unsigned short frag;    
    unsigned char time_to_live;         
    unsigned char protocol;     
    unsigned short checksum;     
    unsigned int endereco_origem;       
    unsigned int endereco_destino;     
};




////////////  Estrutura  Ethernet  ////////////


struct cabecalho_ethernet { 
    unsigned short eth_hdr;  
    unsigned char mac_destino[6];   
    unsigned char mac_origem[6];   
};



//////////// Estrutura RAW SOCKET ////////////


typedef struct {
    int sockfd;                          
    struct sockaddr_ll socket_address;   
    char interface[IF_NAMESIZE];
    unsigned short porta_origem;             
    unsigned short porta_destino;
    unsigned int ip_origem;                 
    unsigned int ip_destino;                    
    unsigned char mac_origem[6];            
    unsigned char mac_destino[6];                             
} rawsocket_t;


////////////  Estrutura UDP  ////////////

struct udp_header {
    unsigned short comprimento;       
    unsigned short checksum;    
    unsigned short porta_origem;     
    unsigned short porta_destino;    
};


//////////// Funções de rawsocket ////////////
int envia_rawsocket(rawsocket_t* rs, const void* data, size_t data_len);
int recebe_rawsocket(rawsocket_t* rs, void* buffer, size_t buffer_size, unsigned int* ip_origem, unsigned short* porta_origem);
void fecha_rawsocket(rawsocket_t* rs);

int inicia_rawsocket(rawsocket_t* rs, const char* interface);
int destino_rawsocket(rawsocket_t* rs, const char* ip_destino, unsigned short porta_destino);
int origem_rawsocket(rawsocket_t* rs, unsigned short porta_origem);

// Funções auxiliares
unsigned short calcula_checksum(unsigned short* ptr, int nbytes);
unsigned short calcula_udp_checksum();

int dados_interface(const char* interface, unsigned char* mac, unsigned int* ip);
int endereco_mac(unsigned char* mac_destino);

#endif // RAWSOCKET_H
