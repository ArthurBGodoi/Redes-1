#include "rawSocket.h"


// Define o destino
int destino_rawsocket(rawsocket_t* rs, const char* ip_destino, unsigned short porta_destino) {
    if (!rs || !ip_destino) return -1;
    
    rs->ip_destino = inet_addr(ip_destino);
    rs->porta_destino = htons(porta_destino);
    
    // Tentar resolver MAC de destino
    if (endereco_mac(rs->mac_destino) < 0) {
        fprintf(stderr, "Aviso: Não foi possível resolver MAC do destino, usando broadcast\n");
        memset(rs->mac_destino, 0xFF, 6); // Broadcast MAC
    }
    
    memcpy(rs->socket_address.sll_addr, rs->mac_destino, 6);
    
    return 0;
}


int inicia_rawsocket(rawsocket_t* rs, const char* interface) {
    if (!rs || !interface) return -1;
    
    memset(rs, 0, sizeof(rawsocket_t));
    strncpy(rs->interface, interface, IF_NAMESIZE - 1);
    
    // Criar raw socket
    rs->sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (rs->sockfd < 0) {
        perror("Erro ao criar raw socket");
        return -1;
    }
    
    // Obter informações da interface
    if (dados_interface(interface, rs->mac_origem, &rs->ip_origem) < 0) {
        close(rs->sockfd);
        return -1;
    }
    
    // Configurar endereço do socket
    memset(&rs->socket_address, 0, sizeof(rs->socket_address));
    rs->socket_address.sll_family = AF_PACKET;
    rs->socket_address.sll_protocol = htons(ETH_P_IP);
    rs->socket_address.sll_ifindex = if_nametoindex(interface);
    rs->socket_address.sll_hatype = ARPHRD_ETHER;
    rs->socket_address.sll_pkttype = PACKET_OTHERHOST;
    rs->socket_address.sll_halen = ETH_ALEN;
    
    printf("Raw socket inicializado na interface %s\n", interface);
    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", 
           rs->mac_origem[0], rs->mac_origem[1], rs->mac_origem[2],
           rs->mac_origem[3], rs->mac_origem[4], rs->mac_origem[5]);
    printf("IP: %s\n", inet_ntoa(*(struct in_addr*)&rs->ip_origem));
    
    return 0;
}


// Envia dados usando raw socket
int envia_rawsocket(rawsocket_t* rs, const void* data, size_t data_len) {
    if (!rs || !data || data_len == 0) return -1;
    // Buffer para o pacote completo
    unsigned char packet[MAXIMO_PACOTE];
    memset(packet, 0, sizeof(packet));
    
    // Calcular tamanhos
    size_t eth_header_size = sizeof(struct cabecalho_ethernet);
    size_t cabecalho_ip_size = sizeof(struct cabecalho_ip);
    size_t udp_header_size = sizeof(struct udp_header);
    size_t total_size = eth_header_size + cabecalho_ip_size + udp_header_size + data_len;
    
    if (total_size > MAXIMO_PACOTE) {
        fprintf(stderr, "Pacote muito grande: %zu bytes\n", total_size);
        return -1;
    }
    
    // Cabeçalho Ethernet
    struct cabecalho_ethernet* eth_hdr = (struct cabecalho_ethernet*)packet;
    memcpy(eth_hdr->mac_destino, rs->mac_destino, 6);
    memcpy(eth_hdr->mac_origem, rs->mac_origem, 6);
    eth_hdr->eth_hdr = htons(0x0800); // IP
    
    // Cabeçalho IP
    struct cabecalho_ip* ip_hdr = (struct cabecalho_ip*)(packet + eth_header_size);
    ip_hdr->versao_ihl = 0x45; // Versão 4, IHL 5 (20 bytes)
    ip_hdr->servico = 0;
    ip_hdr->comprimento = htons(cabecalho_ip_size + udp_header_size + data_len);
    ip_hdr->id = htons(getpid());
    ip_hdr->frag = 0;
    ip_hdr->time_to_live = 64;
    ip_hdr->protocol = 17; // UDP
    ip_hdr->checksum = 0;
    ip_hdr->endereco_origem = rs->ip_origem;
    ip_hdr->endereco_destino = rs->ip_destino;
    
    // Calcular checksum IP
    ip_hdr->checksum = calcula_checksum((unsigned short*)ip_hdr, cabecalho_ip_size);
    
    // Cabeçalho UDP
    struct udp_header* udp_hdr = (struct udp_header*)(packet + eth_header_size + cabecalho_ip_size);
    udp_hdr->porta_origem = rs->porta_origem;
    udp_hdr->porta_destino = rs->porta_destino;
    udp_hdr->comprimento = htons(udp_header_size + data_len);
    udp_hdr->checksum = 0;
    
    // Copiar dados
    memcpy(packet + eth_header_size + cabecalho_ip_size + udp_header_size, data, data_len);
    
    // Enviar pacote
    ssize_t sent = sendto(rs->sockfd, packet, total_size, 0,
                         (struct sockaddr*)&rs->socket_address,
                         sizeof(rs->socket_address));
    
    if (sent < 0) {
        perror("Erro ao enviar pacote");
        return -1;
    }
    
    return sent;
}

int origem_rawsocket(rawsocket_t* rs, unsigned short porta_origem) {
    if (!rs) return -1;
    
    rs->porta_origem = htons(porta_origem);
    return 0;
}
// Recebe dados usando raw socket
int recebe_rawsocket(rawsocket_t* rs, void* buffer, size_t buffer_size,
                     unsigned int* ip_origem, unsigned short* porta_origem) {
    if (!rs || !buffer) return -1;
    
    unsigned char packet[MAXIMO_PACOTE];
    struct sockaddr_ll addr;
    socklen_t addr_len = sizeof(addr);
    
    ssize_t received = recvfrom(rs->sockfd, packet, sizeof(packet), 0,
                               (struct sockaddr*)&addr, &addr_len);
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2; // Timeout
        }
        perror("Erro ao receber pacote");
        return -1;
    }
    
    // Verificar se é um pacote IP
    struct cabecalho_ethernet* eth_hdr = (struct cabecalho_ethernet*)packet;
    if (ntohs(eth_hdr->eth_hdr) != 0x0800) {
        return 0; // Não é IP, ignorar
    }
    
    // Verificar cabeçalho IP
    struct cabecalho_ip* ip_hdr = (struct cabecalho_ip*)(packet + sizeof(struct cabecalho_ethernet));
    if (ip_hdr->protocol != 17) {
        return 0; // Não é UDP, ignorar
    }
    
    // Verificar se é para nós
    if (ip_hdr->endereco_destino != rs->ip_origem) {
        return 0; // Não é para nós, ignorar
    }
    
    // Cabeçalho UDP
    struct udp_header* udp_hdr = (struct udp_header*)(packet + sizeof(struct cabecalho_ethernet) + sizeof(struct cabecalho_ip));
    
    // Verificar porta
    if (udp_hdr->porta_destino != rs->porta_origem) {
        return 0; // Não é para nossa porta, ignorar
    }
    
    // Extrair dados
    size_t header_size = sizeof(struct cabecalho_ethernet) + sizeof(struct cabecalho_ip) + sizeof(struct udp_header);
    size_t data_size = ntohs(udp_hdr->comprimento) - sizeof(struct udp_header);
    
    if (data_size > buffer_size) {
        fprintf(stderr, "Buffer muito pequeno para os dados recebidos\n");
        return -1;
    }
    
    memcpy(buffer, packet + header_size, data_size);
    
    // Retornar informações do remetente se solicitado
    if (ip_origem) *ip_origem = ip_hdr->endereco_origem;
    if (porta_origem) *porta_origem = ntohs(udp_hdr->porta_origem);
    
    return data_size;
}

// Fecha o raw socket
void fecha_rawsocket(rawsocket_t* rs) {
    if (rs && rs->sockfd >= 0) {
        close(rs->sockfd);
        rs->sockfd = -1;
    }
}


int dados_interface(const char* interface, unsigned char* mac, unsigned int* ip) {
    if (!interface || !mac || !ip) return -1;

    // raw socket
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd < 0) {
        perror("Erro ao criar raw socket");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    // MAC address
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("Erro ao obter MAC da interface");
        close(sockfd);
        return -1;
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

    // IP address
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0) {
        perror("Erro ao obter IP da interface");
        close(sockfd);
        return -1;
    }
    *ip = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;

    close(sockfd);
    return 0;
}


// Calcula checksum
unsigned short calcula_checksum(unsigned short* ptr, int nbytes) {
    long sum = 0;
    unsigned short oddbyte;
    
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    
    if (nbytes == 1) {
        oddbyte = 0;
        *((unsigned char*)&oddbyte) = *(unsigned char*)ptr;
        sum += oddbyte;
    }
    
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)~sum;
}


// Resolve endereço MAC (implementação simplificada)
int endereco_mac(unsigned char* mac_destino){
    // Implementação simplificada - usa broadcast MAC
    // Em uma implementação completa, seria necessário fazer ARP
    memset(mac_destino, 0xFF, 6); // Broadcast MAC
    return 0;
}

