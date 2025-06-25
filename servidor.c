#include "protocolo.h"
#include "servidor.h"

struct_protocolo estado_servidor;
struct_jogo jogo;

int main() 
{

    printf("\n🏆 === SERVIDOR CAÇA AO TESOURO ATIVO === 🏆\n");

    // Inicializar servidor
    if (setup_servidor() < 0) {
        fprintf(stderr, "🔴 Erro ao inicializar o servidor\n");
        return 1;
    }
    
    printf("🟢 Servidor porta %d\n", PORTA_SERVIDOR);
    
    // Inicializar jogo
    setup_jogo(&jogo);
    interface_servidor(&jogo);
    
    printf("\nAguardando conexão do cliente no IP do servidor...\n");
    
    // Loop principal do servidor
    while (1) {
        if (gerenciar_mensagem_cliente() < 0) {
            continue; // Continuar aguardando próxima mensagem
        }
        
        // Verificar se jogo terminou
        if (jogo.tesouros_achados >= MAX_TESOUROS) {

            printf("\n🟢 Missão cumprida! Tesouros coletados com sucesso!\n");
            printf("Reiniciando...\n");

            setup_jogo(&jogo);
            interface_servidor(&jogo);
        }
    }
    
    close(estado_servidor.socket_fd);
    return 0;
}


// Cria o socket UDP e configura o endereço do servidor
// Inicializa variáveis de controle do estado do servidor
int setup_servidor() 
{
    // Criar socket UDP
    estado_servidor.socket_fd = socket(AF_INET, SOCKET_RAW, 0);
    if (estado_servidor.socket_fd < 0) {
        perror("Erro ao criar socket");
        return -1;
    }
    
    // Configurar endereço do servidor
    struct sockaddr_in endereco_servidor;
    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;
    endereco_servidor.sin_port = htons(PORTA_SERVIDOR);
    
    // Bind do socket
    if (bind(estado_servidor.socket_fd, (struct sockaddr*)&endereco_servidor, 
             sizeof(endereco_servidor)) < 0) {
        perror("Erro no bind");
        close(estado_servidor.socket_fd);
        return -1;
    }
    
    // Inicializar estado
    estado_servidor.sequencia_atual = 0;
    estado_servidor.sequencia_seguinte = 0;
    estado_servidor.ip_tamanho = sizeof(estado_servidor.ip_remoto);
    
    return 0;
}


// Processa a mensagem recebida do cliente pelo socket UDP
// Executa ações de jogo conforme o tipo de mensagem
int gerenciar_mensagem_cliente() 
{
    struct_frame_pacote pack;
    
    // Receber frame do cliente
    int result = recebe_pacote(&estado_servidor, &pack);
    if (result < 0) {
        if (result == -2) {
            // Timeout normal
            return 0;
        }
        return -1;
    }
    
    // Processar mensagem baseado no tipo
    switch (pack.tipo) {
        case MSG_START:
            printf("🟢 Solicitação de início do jogo recebida\n");
            jogo.partida_iniciada = 1;
        
            // Enviar ACK com a mesma sequência recebida
            if (envia_ack(&estado_servidor, ler_sequencia(pack)) < 0) {
                printf("🔴 Erro crítico ao enviar ACK\n");
                return -1;
            }
            while(1){
                // Enviar mapa inicial com nova sequência
                estado_servidor.sequencia_atual = ler_sequencia(pack) + 1 % 32;
                transmitir_mapa_cliente();
                if(aguarda_ack(&estado_servidor) < 0){
                    continue;
                }
                else
                    break;
            }
            return 1;
            
        // ... resto do código permanece igual ...
        case MSG_MOVE_DIREITA:
            return gerenciar_movimento(MSG_MOVE_DIREITA);
            
        case MSG_MOVE_ESQUERDA:
            return gerenciar_movimento(MSG_MOVE_ESQUERDA);
            
        case MSG_MOVE_CIMA:
            return gerenciar_movimento(MSG_MOVE_CIMA);
            
        case MSG_MOVE_BAIXO:
            return gerenciar_movimento(MSG_MOVE_BAIXO);
            
        default:
            printf("Mensagem não reconhecida: %d\n", pack.tipo);
            return envia_erro(&estado_servidor, (pack.sequencia_bit_0 | (pack.sequencia_bits_4 << 1)), SEM_ACESSO);
    }
    return 1;
}


// Processa o movimento solicitado e atualiza a posição do jogador
// Envia o mapa atualizado ou o tesouro se encontrado
int gerenciar_movimento(mensagem_type direcao) 
{
    const char* nome_direcao;
    switch (direcao) {
        case MSG_MOVE_DIREITA: nome_direcao = "DIREITA"; break;
        case MSG_MOVE_ESQUERDA: nome_direcao = "ESQUERDA"; break;
        case MSG_MOVE_CIMA: nome_direcao = "CIMA"; break;
        case MSG_MOVE_BAIXO: nome_direcao = "BAIXO"; break;
        default: nome_direcao = "DESCONHECIDA"; break;
    }
    
    // Tentar mover jogador
    if (move_player(&jogo, direcao) < 0) {
        printf("🔴 Movimento %s inválido: (%d,%d)\n", 
               nome_direcao, jogo.local_player.x, jogo.local_player.y);
        imprimir_movimento(nome_direcao, 0);
        
        // Enviar erro de movimento inválido
        return envia_erro(&estado_servidor, estado_servidor.sequencia_atual, MOVIMENTO_PROIBIDO);
    }
    
    printf("🟢 Movimento %s realizado: (%d,%d)\n", 
           nome_direcao, jogo.local_player.x, jogo.local_player.y);
    imprimir_movimento(nome_direcao, 1);
    
    // Mostrar mapa atualizado
    interface_servidor(&jogo);
    
    // Verificar se há tesouro na nova posição
    int indice_tesouro = valida_tesouro(&jogo, jogo.local_player);
    if (indice_tesouro >= 0) {
        printf("🌟 Tesouro descoberto 🌟 %s em (%d,%d)\n", 
               jogo.tesouros[indice_tesouro].nome_tesouro,
               jogo.local_player.x, jogo.local_player.y);
        while(1){
            transmitir_mapa_cliente();
            if(aguarda_ack(&estado_servidor) < 0){
                continue;
            }
            else
                break;
        }
        transmitir_tesouro(indice_tesouro);
        return 1;
    } else {
        while(1){
        // Apenas enviar nova posição
        transmitir_mapa_cliente();
        if(aguarda_ack(&estado_servidor) < 0)
                continue;
        else
            break;
        }
        return 1;
    }
}


// Verifica se existe um tesouro na posição informada
// Retorna 1 se existir, ou 0 caso contrário
int checar_tesouro_posicao(struct_tesouro treasures[MAX_TESOUROS], struct_coordenadas pos)
{
    for(int i = 0; i < MAX_TESOUROS; i++){
        if((treasures[i].posicao.x == pos.x)&&
                (treasures[i].posicao.y == pos.y)){
            return 1;
        }
    }
    return 0;
}


// Prepara o pacote com o mapa atualizado e envia ao cliente
// Informa posição do jogador e se encontrou tesouro
int transmitir_mapa_cliente() 
{
    struct_frame_pacote pack;
    struct_frame_mapa mapa_dados;
    
    // Preparar dados do mapa para o cliente
    mapa_dados.posicao_player = jogo.local_player;
    
    //confere se achou um tesouro
    mapa_dados.pegar_tesouro = checar_tesouro_posicao(jogo.tesouros, mapa_dados.posicao_player);

    cria_pacote(&pack, estado_servidor.sequencia_atual, MSG_INTERFACE, 
                (uint8_t*)&mapa_dados, sizeof(mapa_dados));
    
    return envia_pacote(&estado_servidor, &pack);
}


// Envia informações do tesouro encontrado para o cliente
// Depois transmite o arquivo associado ao tesouro
int transmitir_tesouro(int indice_tesouro) 
{
    struct_tesouro* tesouro = &jogo.tesouros[indice_tesouro];
    
    // Cria o pack
    struct_frame_pacote pack;
    
    if (cria_pacote(&pack, estado_servidor.sequencia_atual, MSG_TAMANHO,
                   tesouro->tamanho, sizeof(tesouro->tamanho)) < 0) {
        fprintf(stderr, "🔴 Erro ao criar pack do tesouro\n");
        return -1;
    }
    while(1){
        // Envia o pack
        if (envia_pacote(&estado_servidor, &pack) < 0)
            continue;

        // Aguardar ACK
        if (aguarda_ack(&estado_servidor) < 0)
            continue;
        else{
            while(1){
                uint64_t tamanho_lido;
                memcpy(&tamanho_lido, tesouro->tamanho, sizeof(uint64_t));
                printf("Arquivo possui = %llu bytes\n", (unsigned long long)tamanho_lido);

                break;
            }

            break;
        }
    }

    // Determinar tipo do arquivo
    mensagem_type tipo = tipo_arquivo(tesouro->nome_tesouro);

    transmitir_arquivo_tesouro(tesouro->patch, tesouro->nome_tesouro, tipo);
    return 0;
}


// Envia o nome e o conteúdo do arquivo de tesouro para o cliente
// Realiza o envio em blocos, aguardando ACK para cada pacote
int transmitir_arquivo_tesouro(const char* caminho_arquivo, const char* nome_tesouro, mensagem_type tipo) 
{

    FILE* arquivo = fopen(caminho_arquivo, "rb");
    if (!arquivo) {
        perror("Erro ao abrir arquivo do tesouro");
        return -1;
    }

    printf("NOME DO ARQUIVO: %s\n", nome_tesouro);



    // Primeiro enviar o nome do arquivo
    struct_frame_pacote pack_nome;
    estado_servidor.sequencia_atual = (estado_servidor.sequencia_atual + 1) % 32;
    cria_pacote(&pack_nome, estado_servidor.sequencia_atual, tipo, 
              (uint8_t*)nome_tesouro, strlen(nome_tesouro) + 1);
    
    while(1) {
        if (envia_pacote(&estado_servidor, &pack_nome) < 0) {
            continue;
        }
        if (aguarda_ack(&estado_servidor) < 0) {
            continue;
        }
        break;
    }

    
    // Enviar o arquivo em chunks
    uint8_t buffer[TAM_MAX_DADOS];
    size_t bytes_lidos;
    memset(buffer, 0, sizeof(buffer));
    size_t bytes_enviados = 0;
    while ((bytes_lidos = fread(buffer, 1, TAM_MAX_DADOS, arquivo)) > 0) {
        struct_frame_pacote pack_dados;
        estado_servidor.sequencia_atual = (estado_servidor.sequencia_atual + 1) % 32;
               
        cria_pacote(&pack_dados, estado_servidor.sequencia_atual, MSG_DADOS, buffer, bytes_lidos);
        
        while(1) {
            printf("🟢 Total de bytes enviados: %zd\n", bytes_enviados);
            if (envia_pacote(&estado_servidor, &pack_dados) < 0) {
                bytes_lidos = 0;
                memset(buffer, 0, sizeof(buffer));
                continue;
            }
            if (aguarda_ack(&estado_servidor) < 0) {
                bytes_lidos = 0;
                memset(buffer, 0, sizeof(buffer));
                continue;
            }
            break;
        }
        bytes_enviados+=bytes_lidos;
        bytes_lidos = 0;
        memset(buffer, 0, sizeof(buffer));
    }

    fclose(arquivo);
    return 0;
}


// Exibe no terminal o log do movimento do jogador
// Mostra horário, direção, posição e quantidade de tesouros
void imprimir_movimento(const char* direcao, int sucesso) 
{
    time_t now = time(NULL);
    char* time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remover \n
    
    printf("[%s] Movimento %s: %s | Posição: (%d,%d) | Tesouros: %d/%d\n",
           time_str, direcao, sucesso ? "OK" : "INVÁLIDO",
           jogo.local_player.x, jogo.local_player.y,
           jogo.tesouros_achados, MAX_TESOUROS);
}
