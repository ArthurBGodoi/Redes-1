#include "protocolo.h"

uint8_t getSeq(pack_t pack){
    return pack.seq_inicio | (pack.seq_fim << 1);
}

void reseta_interface() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}


int reenvio(protocolo_type* estado, pack_t pack){
    if((pack.tipo == MSG_ACK)||(pack.tipo == MSG_NACK)||(pack.tipo == MSG_OK_ACK))
        return 0;
    return enviar_pacote(estado, &pack);
}

int seqCheck(uint8_t seqAtual, uint8_t seqPack){
    if(seqAtual == seqPack){
        return 0;
    } else if (seqPack == ((seqAtual + 1) % 32)){
        return 1;
    } else{
        return -1;
    }

}


// --- Cálculo de checksum ---
uint8_t calcular_checksum(const pack_t* p) {
    uint8_t checksum = 0;
    checksum ^= p->tamanho;
    checksum ^= (p->seq_inicio | (p->seq_fim << 1));
    checksum ^= p->tipo;
    for (int i = 0; i < p->tamanho; i++) {
        checksum ^= p->dados[i];
    }
    return checksum;
}

// Onde o raw socket esta sendo configurado
int inicializar_protocolo(protocolo_type* estado, const char* ip_destino, unsigned short orig_port,
                                 unsigned short porta_destino, const char* interface) {
    if (!estado || !ip_destino || !interface) return -1;
    
    memset(estado, 0, sizeof(protocolo_type));
    
    // Inicializar raw socket
    if (inicia_rawsocket(&estado->rawsock, interface) < 0) {
        fprintf(stderr, "Erro ao inicializar raw socket cliente\n");
        return -1;
    }
    
    // Configurar destino
    strncpy(estado->ip_destino, ip_destino, sizeof(estado->ip_destino) - 1);
    estado->porta_destino = porta_destino;
    
    if (destino_rawsocket(&estado->rawsock, ip_destino, porta_destino) < 0) {
        fprintf(stderr, "🔴 Erro ao configurar destino\n");
        fecha_rawsocket(&estado->rawsock);
        return -1;
    }
    
    // Porta de origem aleatória para cliente
    estado->porta_origem = orig_port;
    if (origem_rawsocket(&estado->rawsock, estado->porta_origem) < 0) {
        fprintf(stderr, "🔴 Erro ao configurar porta do cliente\n");
        fecha_rawsocket(&estado->rawsock);
        return -1;
    }
    
    estado->seq_atual = 0;
    estado->seq_esperada = 0;
    
    printf("Cliente inicializado, conectando para %s:%d (porta local: %d)\n", 
           ip_destino, porta_destino, estado->porta_origem);
    return 0;
}

void finalizar_protocolo(protocolo_type* estado) {
    if (estado) {
        fecha_rawsocket(&estado->rawsock);
        memset(estado, 0, sizeof(protocolo_type));
    }
}

int criar_pacote(pack_t* pack, unsigned char seq, mensagem_type tipo,
               uint8_t* dados, unsigned short tamanho) {
    if (!pack || tamanho > 127 || seq > 31 || tipo > 15) {
        printf("🔴 Erro: valores fora dos limites do cabeçalho\n");
        return -1;
    }

    pack->marcador = 0x7E;
    pack->tamanho = tamanho & 0x7F;      // Garante 7 bits
    pack->seq_inicio = seq & 0x01;         // Pega o bit 0 da sequência
    pack->seq_fim = (seq >> 1) & 0x0F; // Pega bits 1-4
    pack->tipo = tipo & 0x0F;            // Garante 4 bits

    // Limpar dados antes de copiar
    memset(pack->dados, 0, sizeof(pack->dados));
    
    if (dados && tamanho > 0) {
        memcpy(pack->dados, dados, tamanho);
    }

    // Aplicar checksum
    pack->checksum = calcular_checksum(pack);

    return 0;
}



int enviar_pacote(protocolo_type* estado, const pack_t* pack) {
    if (!estado || !pack) return -1;

    int tamanho_total = 4 + pack->tamanho;

    
    for (int tentativa = 0; tentativa < MAX_RETRY; tentativa++) {
        int enviados = envia_rawsocket(&estado->rawsock, pack, tamanho_total);
        


        if (enviados == 42 + tamanho_total) {
           // fprintf(stderr,"Tamanho enviado %d oq achamos que seria enviado %d", enviados, tam );

            return 0;
        }

        if (enviados < 0) {
            fprintf(stderr, "🔴 Erro no envio (tentativa %d/%d)\n", 
                   tentativa + 1, MAX_RETRY);
        }

        sleep(TIMEOUT_S);
    }

    fprintf(stderr, "🔴 Falha ao enviar após %d tentativas\n", MAX_RETRY);
    return -4;
}

const char *uint8_to_bits(uint8_t num) {
    static char bits[9]; // 8 bits + null terminator
    for (int i = 7; i >= 0; i--) {
        bits[7 - i] = (num & (1 << i)) ? '1' : '0';
    }
    bits[8] = '\0';
    return bits;
}

int receber_pacote(protocolo_type* estado, pack_t* pack) {
    if (!estado || !pack) return -1;

    // Configurar timeout no raw socket
    struct timeval timeout = { .tv_sec = TIMEOUT_S, .tv_usec = 0 };
    if (setsockopt(estado->rawsock.sockfd, SOL_SOCKET, SO_RCVTIMEO, 
                   &timeout, sizeof(timeout)) < 0) {
        perror("🔴 Erro ao configurar timeout");
        return -1;
    }

    unsigned int ip_origem;
    unsigned short porta_origem;
    
    int recebidos = recebe_rawsocket(&estado->rawsock, pack, sizeof(pack_t), 
                                     &ip_origem, &porta_origem);

    if (recebidos < 0) {
        if (recebidos == -2) {
            return -2; // Timeout
        }
        fprintf(stderr, "Erro no recebimento\n");
        return -1;
    }
    
    if (recebidos == 0) {
        return -2; // Nenhum pacote relevante recebido
    }

    if (recebidos < 4) {
        fprintf(stderr, "Pacote muito pequeno recebido: %d bytes\n", recebidos);
        return -1;
    }

    uint8_t tamanho = pack->tamanho;
    if (tamanho > 127 || recebidos != 4 + tamanho) {
        fprintf(stderr, "Tamanho de pacote inválido: recebido %d, esperado %u\n", 
               recebidos, 4 + tamanho);
        return -1;
    }
        //fprintf(stderr, "Tamanho de pack válido: recebido %d, esperado %u\n", 
          //     recebidos, 4 + tamanho);

    uint8_t mark = pack->marcador;
    if (mark != 0x7E) {
        fprintf(stderr, "Marcador inválido: recebido %s, esperado %s\n", 
               uint8_to_bits(mark), uint8_to_bits(0x7E));
        return -1;
    }

    // Verificar checksum
    uint8_t checksum_recebido = pack->checksum;
    uint8_t checksum_calculado = calcular_checksum(pack);
    if (checksum_recebido != checksum_calculado) {
        fprintf(stderr, "Checksum inválido: esperado %u, recebido %u\n",
                checksum_calculado, checksum_recebido);
        return -3; // erro de integridade
    }

    // Atualizar informações do remetente (para respostas)
    struct in_addr addr;
    addr.s_addr = ip_origem;
    strncpy(estado->ip_destino, inet_ntoa(addr), sizeof(estado->ip_destino) - 1);
    estado->porta_destino = porta_origem;
    
    // Atualizar destino no raw socket
    destino_rawsocket(&estado->rawsock, estado->ip_destino, estado->porta_destino);

    return 0;
}

int enviar_ack(protocolo_type* estado, uint8_t seq)  {
    pack_t pack;
    criar_pacote(&pack, seq, MSG_ACK, NULL, 0);
    memcpy(&estado->pack, &pack, sizeof(pack_t));
    return enviar_pacote(estado, &pack);
}

int enviar_nack(protocolo_type* estado, uint8_t seq) {
    pack_t pack;
    criar_pacote(&pack, seq, MSG_NACK, NULL, 0);
    memcpy(&estado->pack, &pack, sizeof(pack_t));
    return enviar_pacote(estado, &pack);
}

int enviar_ok_ack (protocolo_type* estado, uint8_t seq) {
    pack_t pack;
    criar_pacote(&pack, seq, MSG_OK_ACK, NULL, 0);
    memcpy(&estado->pack, &pack, sizeof(pack_t));
    return enviar_pacote(estado, &pack);
}

int enviar_erro(protocolo_type* estado, uint8_t seq, erro_type erro) {
    pack_t pack;
    uint8_t dados_erro = (uint8_t)erro;
    criar_pacote(&pack, seq, MSG_ERRO, &dados_erro, 1);
    memcpy(&estado->pack, &pack, sizeof(pack_t));
    return enviar_pacote(estado, &pack);
}

int esperar_ack(protocolo_type* estado) {
    pack_t resposta;

    do{
        int result = receber_pacote(estado, &resposta);
        int isSeq = seqCheck(estado->seq_atual, getSeq(resposta));
        if (isSeq == 0){
            if (result < 0) {
                return result;
            }
            if (resposta.tipo == MSG_OK_ACK){
                return 1;
            } else if (resposta.tipo == MSG_ACK) {
                return 0;
            } else if (resposta.tipo == MSG_NACK) {
                return -3; 
            }
        }
        else 
            return -1;
    } while(1);
     // Resposta inesperada
}

void setup_jogo(struct_jogo* jogo) {

    if (!jogo) return;
    
    // Limpar estruturas
    memset(jogo, 0, sizeof(struct_jogo));
    
    // Posição inicial do jogador
    jogo->local_player.x = 0;
    jogo->local_player.y = 0;
    jogo->local_explorado[0][0] = 1;
    
    // Seed para random
    srand(time(NULL));
    
    // Sortear posições dos tesouros
    printf("Sorteando tesouros...\n");
    
    for (int i = 0; i < MAX_TESOUROS; i++) {
        int x, y;
        int posicao_ocupada;
        
        do {
            x = rand() % TAMANHO_MAPA;
            y = rand() % TAMANHO_MAPA;
            
            // Verificar se posição já está ocupada
            posicao_ocupada = 0;
            
            // Não colocar tesouro na posição inicial (0,0)
            if (x == 0 && y == 0) {
                posicao_ocupada = 1;
            }
            
            // Verificar outros tesouros
            for (int j = 0; j < i; j++) {
                if (jogo->tesouros[j].posicao.x == x && jogo->tesouros[j].posicao.y == y) {
                    posicao_ocupada = 1;
                    break;
                }
            }
        } while (posicao_ocupada);
        
        jogo->tesouros[i].posicao.x = x;
        jogo->tesouros[i].posicao.y = y;
        jogo->tesouros[i].encontrado = 0;
        
        // Definir nome e tipo do arquivo
        if (i < 3) {
            // Primeiros 3 são textos
            snprintf(jogo->tesouros[i].nome_tesouro, sizeof(jogo->tesouros[i].nome_tesouro),
                     "%d.txt", i + 1);
            jogo->tesouros[i].tipo = TESOURO_TXT;
        } else if (i < 6) {
            // Próximos 3 são imagens
            snprintf(jogo->tesouros[i].nome_tesouro, sizeof(jogo->tesouros[i].nome_tesouro),
                     "%d.jpg", i + 1);
            jogo->tesouros[i].tipo = TESOURO_IMG;
        } else {
            // Últimos 2 são vídeos
            snprintf(jogo->tesouros[i].nome_tesouro, sizeof(jogo->tesouros[i].nome_tesouro),
                     "%d.mp4", i + 1);
            jogo->tesouros[i].tipo = TESOURO_VID;
        }
        
        char nome_temp[MAX_NOME];
        strncpy(nome_temp, jogo->tesouros[i].nome_tesouro, sizeof(nome_temp));
        nome_temp[sizeof(nome_temp) - 1] = '\0';

        snprintf(jogo->tesouros[i].patch, sizeof(jogo->tesouros[i].patch),
             "./objetos/%s", nome_temp);

        // Obter tamanho do arquivo
        obter_tamanho_arquivo(jogo->tesouros[i].patch, jogo->tesouros[i].tamanho);
        
        uint64_t tamanho_arquivo;
        memcpy(&tamanho_arquivo, jogo->tesouros[i].tamanho, sizeof(uint64_t));
        
        printf("Tesouro %d: %s na posição (%d,%d) - %llu bytes\n",
               i + 1, jogo->tesouros[i].nome_tesouro, x, y, 
               (unsigned long long)tamanho_arquivo);
    }
}

int move_player(struct_jogo* jogo, mensagem_type direcao) {
    if (!jogo) return -1;
    
    posicao_t nova_posicao = jogo->local_player;
    
    switch (direcao) {
        case MSG_MOVE_DIREITA:
            nova_posicao.x++;
            break;
        case MSG_MOVE_ESQUERDA:
            nova_posicao.x--;
            break;
        case MSG_MOVE_CIMA:
            nova_posicao.y++;
            break;
        case MSG_MOVE_BAIXO:
            nova_posicao.y--;
            break;
        default:
            return -1;
    }
    
    // Verificar limites do grid
    if (nova_posicao.x < 0 || nova_posicao.x >= TAMANHO_MAPA ||
        nova_posicao.y < 0 || nova_posicao.y >= TAMANHO_MAPA) {
        return -1; // Movimento inválido
    }
    
    // Atualizar posição
    jogo->local_player = nova_posicao;
    jogo->local_explorado[nova_posicao.x][nova_posicao.y] = 1;
    
    return 0; // Movimento válido
}

int valida_tesouro(struct_jogo* jogo, posicao_t posicao) {
    if (!jogo) return -1;
    
    for (int i = 0; i < MAX_TESOUROS; i++) {
        if (jogo->tesouros[i].posicao.x == posicao.x &&
            jogo->tesouros[i].posicao.y == posicao.y &&
            !jogo->tesouros[i].encontrado) {
            
            jogo->tesouros[i].encontrado = 1;
            jogo->tesouros_achados++;
            return i; // Índice do tesouro encontrado
        }
    }
    
    return -1; // Nenhum tesouro encontrado
}

void interface_servidor(struct_jogo* jogo) {
    if (!jogo) return;
    reseta_interface();  
    printf("\n=== MAPA DO SERVIDOR ===\n");
    printf("Posição do jogador: (%d,%d)\n", jogo->local_player.x, jogo->local_player.y);
    printf("Tesouros encontrados: %d/%d\n", jogo->tesouros_achados, MAX_TESOUROS);
    
    printf("\nLegenda: 🗿 = Jogador, 👑 = Tesouro, ✅  = Tesouro achado, 😓 = Explorado, ⚫️ = Não Explorado\n");
    
    // Mostrar grid (y crescente para cima)
    for (int y = TAMANHO_MAPA - 1; y >= 0; y--) {
        printf("%d ", y);
        for (int x = 0; x < TAMANHO_MAPA; x++) {
            if (jogo->local_player.x == x && jogo->local_player.y == y) {
                printf("🗿 ");
            } else {
                // Verificar se há tesouro nesta posição
                int tem_tesouro = 0;
                for (int i = 0; i < MAX_TESOUROS; i++) {
                    if (jogo->tesouros[i].posicao.x == x && jogo->tesouros[i].posicao.y == y) {
                        if (jogo->tesouros[i].encontrado) {
                            printf("✅ ");
                        } else {    
                            printf("👑 ");
                        }
                        tem_tesouro = 1;
                        break;
                    }
                }
                
                if (!tem_tesouro) {
                    if (jogo->local_explorado[x][y]) {
                        printf("😓 ");
                    } else {
                        printf("⚫️ ");
                    }
                }
            }
        }
        printf("\n");
    }
    
    printf("  ");
    for (int x = 0; x < TAMANHO_MAPA; x++) {
        printf("%d ", x);
    }
    printf("\n========================\n");
}

void obter_tamanho_arquivo(const char* caminho, uint8_t tam[MAX_FRAME]) {
    if (!caminho || !tam) return;
    
    struct stat st;
    uint64_t tamanho = 0;
    
    if (stat(caminho, &st) == 0) {
        tamanho = (uint64_t)st.st_size;
    }
    
    memcpy(tam, &tamanho, sizeof(uint64_t));
}

mensagem_type determinar_tipo_arquivo(const char* nome_tesouro) {
    if (!nome_tesouro) return MSG_TEXTO_ACK_NOME;
    
    const char* extensao = strrchr(nome_tesouro, '.');
    if (!extensao) return MSG_TEXTO_ACK_NOME;
    
    if (strcasecmp(extensao, ".txt") == 0) {
        return MSG_TEXTO_ACK_NOME;
    } else if (strcasecmp(extensao, ".jpg") == 0 || strcasecmp(extensao, ".jpeg") == 0) {
        return MSG_IMAGEM_ACK_NOME;
    } else if (strcasecmp(extensao, ".mp4") == 0 || strcasecmp(extensao, ".mp3") == 0) {
        return MSG_VIDEO_ACK_NOME;
    }
    
    return MSG_TEXTO_ACK_NOME;
}

uint64_t obter_espaco_livre(const char* caminho) {
    struct statvfs info;

    if (statvfs(caminho, &info) != 0) {
        perror("🔴 Erro ao obter informações do sistema de arquivos");
        return 0;
    }

    return (uint64_t) info.f_bsize * info.f_bavail;
}
