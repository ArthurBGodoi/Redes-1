#include "protocolo.h"


int envia_ack(struct_protocolo* estado, uint8_t seq)  
{
    struct_frame_pacote pacote;

    cria_pacote(&pacote, seq, MSG_ACK, NULL, 0);

    return envia_pacote(estado, &pacote);
}


int envia_nack(struct_protocolo* estado, uint8_t seq) 
{
    struct_frame_pacote pacote;

    cria_pacote(&pacote, seq, MSG_NACK, NULL, 0);

    return envia_pacote(estado, &pacote);
}


int envia_ok_ack (struct_protocolo* estado, uint8_t seq) 
{
    struct_frame_pacote pacote;

    cria_pacote(&pacote, seq, MSG_NACK, NULL, 0);

    return envia_pacote(estado, &pacote);
}


int envia_erro(struct_protocolo* estado, uint8_t seq, erro_type erro) 
{
    struct_frame_pacote pacote;
    uint8_t log_erro;

    log_erro = (uint8_t)erro;
    
    cria_pacote(&pacote, seq, MSG_ERRO, &log_erro, 1);

    return envia_pacote(estado, &pacote);
}


int aguarda_ack(struct_protocolo* estado) 
{
    struct_frame_pacote retorno;

    int pacote_recebido = recebe_pacote(estado, &retorno);

    if (pacote_recebido < 0)
        return pacote_recebido;

    if (retorno.tipo == MSG_ACK) 
        return 0;  // Recebeu um ACK

    else if (retorno.tipo == MSG_NACK) 
        return -3; // NRecebeu um NACK

    else if (retorno.tipo == MSG_ERRO)
        return -4; // Recebeu um ERRO

    return -1;     // Recebeu algo inválido
}


void setup_jogo(struct_jogo* jogo) 
{
    // Se der algum erro retorne

    if (!jogo)                               
        return;

    // Função para resetar a memória e limpá-la

    memset(jogo, 0, sizeof(struct_jogo));
    
    // Posições iniciais do player

    jogo->local_player.x = 0;
    jogo->local_player.y = 0;
    jogo->local_explorado[0][0] = 1;
    
    srand(time(NULL));
    
    // Colocando os tesouros de forma randômica
    
    for (int i = 0; i < MAX_TESOUROS; i++) 
    {
        int x, y;
        int grid_preenchido;
        
        do {
            x = rand() % MAX_GRID;
            y = rand() % MAX_GRID;
            
            // Vê se o grid já está preenchido

            grid_preenchido = 0;
            
            // Se for a posição inicial não coloca tesouro

            if (x == 0 && y == 0) 
                grid_preenchido = 1;
            
            // Coloca os tesouros

            for (int j = 0; j < i; j++) {
                if (jogo->tesouros[j].posicao.x == x && jogo->tesouros[j].posicao.y == y) {
                    grid_preenchido = 1;
                    break;
                }
            }
        } while (grid_preenchido);
        
        jogo->tesouros[i].posicao.x = x;
        jogo->tesouros[i].posicao.y = y;
        jogo->tesouros[i].achado = 0;
        

        // Definir nome e tipo do arquivo

        // 3 textos

        if (i < 3) {
            snprintf(jogo->tesouros[i].nome_tesouro, sizeof(jogo->tesouros[i].nome_tesouro), "%d.txt", i + 1);
            jogo->tesouros[i].tipo = TESOURO_TXT;
        } 
        
        // 3 imagens
        else if (i < 6) {
            snprintf(jogo->tesouros[i].nome_tesouro, sizeof(jogo->tesouros[i].nome_tesouro), "%d.jpg", i + 1);
            jogo->tesouros[i].tipo = TESOURO_IMG;
        } 
        
        // 2 vídeos
        else {
            snprintf(jogo->tesouros[i].nome_tesouro, sizeof(jogo->tesouros[i].nome_tesouro), "%d.mp4", i + 1);
            jogo->tesouros[i].tipo = TESOURO_MP4;
        }
        
        char aux_nome[MAX_NOME];

        strncpy(aux_nome, jogo->tesouros[i].nome_tesouro, sizeof(aux_nome));
        aux_nome[sizeof(aux_nome) - 1] = '\0';

        snprintf(jogo->tesouros[i].patch, sizeof(jogo->tesouros[i].patch), "./objetos/%s", aux_nome);

        // Lê o tamanho do arquivo

        ler_tamanho_arquivo(jogo->tesouros[i].patch, jogo->tesouros[i].tamanho);
        
        printf("Tesouro %d: %s na posição (%d,%d) - %u bytes\n", i + 1, jogo->tesouros[i].nome_tesouro, x, y, *(jogo->tesouros[i].tamanho));
    }
}


int move_player(struct_jogo* jogo, mensagem_type direcao) 
{
    if (!jogo) 
        return -1;
    
    struct_coordenadas nova_posicao = jogo->local_player;
    
    switch (direcao) 
    {
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
    
    // Vẽ se chegou na borda do mapa
    if (nova_posicao.x < 0 || nova_posicao.x >= MAX_GRID || nova_posicao.y < 0 || nova_posicao.y >= MAX_GRID)
        return -1; // Movimento proibido
    
    // Atualizar posição

    jogo->local_player = nova_posicao;
    jogo->local_explorado[nova_posicao.x][nova_posicao.y] = 1;
    
    return 0; // Movimento válido
}


int valida_tesouro(struct_jogo* jogo, struct_coordenadas posicao) 
{
    if (!jogo) 
        return -1;
    
    for (int i = 0; i < MAX_TESOUROS; i++) {
        if (jogo->tesouros[i].posicao.x == posicao.x && jogo->tesouros[i].posicao.y == posicao.y && !jogo->tesouros[i].achado) {
            
            jogo->tesouros[i].achado = 1;
            jogo->tesouros_achados++;

            return i; // Índice de onde o tesouro foi encontrado
        }
    }
    
    return -1; // Nenhum tesouro achado
}


void reseta_interface() 
{
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}


void interface_servidor(struct_jogo* jogo) 
{
    if (!jogo) 
        return;

    reseta_interface();

    printf("\n====== MINIMAPA GRID 8x8 SERVIDOR ======\n");
    printf("Tesouros achados: %d/%d\n", jogo->tesouros_achados, MAX_TESOUROS);
    printf("Posição atual do player: (%d,%d)\n", jogo->local_player.x, jogo->local_player.y);
    
    printf("\n Legenda dos ícones: 👑 = Tesouro 🗿 = Jogador, ✅  = Tesouro achado, ⚫️ = Não Explorado 😓 = Explorado \n\n");
    
    // Mostrar grid

    for (int y = MAX_GRID - 1; y >= 0; y--) {
        printf("%d ", y);
        for (int x = 0; x < MAX_GRID; x++) {
            if (jogo->local_player.x == x && jogo->local_player.y == y) {
                printf("🗿 ");
            } else {
                // Verificar se há tesouro nesta posição
                int encontrou_tesouro = 0;

                for (int i = 0; i < MAX_TESOUROS; i++) {
                    if (jogo->tesouros[i].posicao.x == x && jogo->tesouros[i].posicao.y == y) {
                        if (jogo->tesouros[i].achado) {
                            printf("✅  ");
                        } else {
                            printf("👑 ");
                        }
                        encontrou_tesouro = 1;
                        break;
                    }
                }
                
                if (!encontrou_tesouro) {
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

    for (int x = 0; x < MAX_GRID; x++) 
        printf("%d  ", x);
}

void ler_tamanho_arquivo(const char* caminho, uint8_t tam[TAM_MAX_DADOS]) 
{
    if (!caminho) 
        return ;
    
    struct stat st;
    
    if (stat(caminho, &st) == 0) {
        memcpy(tam, (uint64_t *)&st.st_size, sizeof(uint64_t)); // Copia 8 bytes
        return;
    }

    return;
}

unsigned long ler_area_livre(const char* diretorio) 
{
    if (!diretorio) 
        return 0;
    
    struct statvfs st;

    if (statvfs(diretorio, &st) == 0) 
        return st.f_bavail * st.f_frsize;

    return 0;
}

int valida_arquivo(const char* caminho) 
{
    if (!caminho) 
        return 0;
    
    struct stat st;

    if (stat(caminho, &st) == 0) 
        return S_ISREG(st.st_mode);

    return 0;
}

mensagem_type tipo_arquivo(const char* nome_tesouro) 
{
    if (!nome_tesouro) 
        return MSG_TEXTO_ACK_NOME;
    
    const char* extensao = strrchr(nome_tesouro, '.');

    if (!extensao) 
        return MSG_TEXTO_ACK_NOME;
    
    if (strcasecmp(extensao, ".txt") == 0) 
        return MSG_TEXTO_ACK_NOME;

    else if (strcasecmp(extensao, ".jpg") == 0 || strcasecmp(extensao, ".jpeg") == 0) 
        return MSG_IMAGEM_ACK_NOME;

    else if (strcasecmp(extensao, ".mp4") == 0 || strcasecmp(extensao, ".mp3") == 0) 
        return MSG_VIDEO_ACK_NOME;
    
    return MSG_TEXTO_ACK_NOME;
}


uint8_t ler_sequencia(struct_frame_pacote pack)
{
    return pack.sequencia_bit_0 | (pack.sequencia_bits_4 << 1);
}


uint8_t calcular_checksum(const struct_frame_pacote* p) 
{
    uint8_t checksum = 0;

    checksum ^= p->tamanho;
    checksum ^= (p->sequencia_bit_0 | (p->sequencia_bits_4 << 1));
    checksum ^= p->tipo;

    for (int i = 0; i < p->tamanho; i++)
        checksum ^= p->dados[i];

    return checksum;
}

int cria_pacote(struct_frame_pacote* pack, unsigned char seq, mensagem_type tipo, uint8_t* dados, unsigned short tamanho) 
{
    if (!pack || tamanho > 127 || seq > 31 || tipo > 15) {
        printf("Valores não condizem com o frame do protocolo\n");
        return -1;
    }

    pack->marcador_inicio = 0x7E;
    pack->tamanho = tamanho & 0x7F;             // Recebe 7 bits
    pack->sequencia_bit_0 = seq & 0x01;         // Pega o bit 0 da sequência
    pack->sequencia_bits_4 = (seq >> 1) & 0x0F; // Pega bits 1-4
    pack->tipo = tipo & 0x0F;                   // Recebe 4 bits

    if (dados && tamanho > 0)
        memcpy(pack->dados, dados, tamanho);

    pack->checksum = calcular_checksum(pack);

    return 0;
}

int envia_pacote(struct_protocolo* estado, const struct_frame_pacote* pack) 
{
    if (!estado || !pack) 
        return -1;

    int tamanho_total = 4 + pack->tamanho;

    for (int tentativa = 0; tentativa < MAX_TIMEOUT; tentativa++) {
        ssize_t enviados = sendto(estado->socket_fd, pack, tamanho_total, 0, (struct sockaddr*)&estado->ip_remoto, estado->ip_tamanho);
        
        if (enviados == tamanho_total) return 0;

        if (enviados < 0) perror("Erro ao enviar");

        sleep(DURACAO_TIMEOUT);
    }

    return -1;
}

const char *uint8_to_bits(uint8_t num) 
{
    static char bits[9]; 

    for (int i = 7; i >= 0; i--)
        bits[7 - i] = (num & (1 << i)) ? '1' : '0';

    bits[8] = '\0';

    return bits;
}

int recebe_pacote(struct_protocolo* estado, struct_frame_pacote* pack) 
{
    if (!estado || !pack) 
        return -1;

    struct timeval timeout = { .tv_sec = DURACAO_TIMEOUT, .tv_usec = 0 };

    if (setsockopt(estado->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Erro no timeout");
        return -1;
    }

    ssize_t recebidos = recvfrom(estado->socket_fd, pack, sizeof(struct_frame_pacote), 0, (struct sockaddr*)&estado->ip_remoto, &estado->ip_tamanho);

    if (recebidos < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return -2; // Timeout

        perror("Erro no recvfrom");
        return -1;
    }

    if (recebidos < 4) {
        fprintf(stderr, "Pacote com pouco tamanho\n");
        return -1;
    }

    uint8_t tamanho = pack->tamanho;

    if (tamanho > 127 || recebidos != 4 + tamanho) {
        fprintf(stderr, "Erro no tamanho do pacote, recebido %zd esperado %u\n", recebidos, 4 + tamanho);
        sleep(DURACAO_TIMEOUT);
        return -1;
    }

    uint8_t mark = pack->marcador_inicio;

    if (mark != 126){
        fprintf(stderr, "Erro no marcador de início, recebido %s esperado %s\n", uint8_to_bits(mark), uint8_to_bits(126));
        return -1;
    }

    uint8_t checksum_recebido = pack->checksum;
    uint8_t checksum_calculado = calcular_checksum(pack);

    if (checksum_recebido != checksum_calculado) {
        fprintf(stderr, "Erro no checksum: esperado %u, recebido %u\n", checksum_calculado, checksum_recebido);
        return -3; // Erro no checksum
    }

    return 0;
}