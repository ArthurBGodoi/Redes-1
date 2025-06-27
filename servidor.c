#include "protocolo.h"
#include "rawSocket.h"

// Variáveis globais
protocolo_type estado_servidor;
struct_jogo jogo;

//////////// Protótipos das funções ////////////

// Conecta cliente com servidor
int conectar_cliente(const char* ip_servidor, int porta);

// Processa a mensagem recebida do cliente pelo socket e executa ações de jogo conforme o tipo de mensagem
int gerenciar_mensagem_cliente();

//  Processa o movimento solicitado e atualiza a posição do jogador, envia o mapa atualizado ou o tesouro se encontrado
int gerenciar_movimento(mensagem_type direcao);

// Prepara o pacote com o mapa atualizado e envia ao cliente, informa posição do jogador e se encontrou tesouro
int transmitir_mapa_cliente();

// Envia informações do tesouro encontrado para o cliente
// Depois transmite o arquivo associado ao tesouro
int transmitir_tesouro(int indice_tesouro);

// Envia o nome e o conteúdo do arquivo de tesouro para o cliente
// Realiza o envio em blocos, aguardando ACK para cada pacote
int transmitir_arquivo_tesouro(const char* caminho_arquivo, const char* nome_tesouro, mensagem_type tipo);

// Exibe no terminal o log do movimento do jogador
// Mostra horário, direção, posição e quantidade de tesouros
void imprimir_movimento(const char* direcao, int sucesso);

// Verifica se existe um tesouro na posição informada
// Retorna 1 se existir, ou 0 caso contrário
int checar_tesouro_posicao(tesouro_t treasures[MAX_TESOUROS], posicao_t pos);


int main(int argc, char* argv[]) 
{
    int porta_servidor = PORTA_CLIENTE;
    char ip_servidor[16];

    printf("=== SERVIDOR CAÇA AO TESOURO ATIVO ===\n");
    
    // Obter IP do servidor
    if (argc > 1) {
        strcpy(ip_servidor, argv[1]);
    } else {
        printf("Digite o IP do cliente: ");
        if (scanf("%15s", ip_servidor) != 1) {
            fprintf(stderr, "Erro ao ler IP do cliente\n");
            return 1;
        }
    }


        // Conectar ao servidor
    if (conectar_cliente(ip_servidor, porta_servidor) < 0) {
        fprintf(stderr, "Erro ao conectar ao cliente\n");
        return 1;
    }
    
    printf("Servidor iniciado na porta %d\n", PORTA_SERVIDOR);
    
    // Inicializar jogo    
    setup_jogo(&jogo);
    interface_servidor(&jogo);
    
    printf("\nAguardando conexão do cliente...\n");
    
    // Loop principal do servidor
    while (1) {
        int p = gerenciar_mensagem_cliente();
        if (p == -4){
            finalizar_protocolo(&estado_servidor);
            reseta_interface();
            perror("Erro fatal!!! Digite ENTER para matar o programa\n");
            getchar();
            return 0;
        }
        if (p < 0) {
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
    
    finalizar_protocolo(&estado_servidor);
    return 0;
}

int conectar_cliente(const char* ip_servidor, int porta) {
    const char* interface = INTERFACE_PADRAO;
    // Inicializar protocolo com raw socket
    if (inicializar_protocolo(&estado_servidor, ip_servidor, PORTA_SERVIDOR, porta, interface) < 0) {
        fprintf(stderr, "🔴Erro ao inicializar protocolo cliente\n");
        return -1;
    }
    
    return 0;
}

// Processa a mensagem recebida do cliente pelo socket
// Executa ações de jogo conforme o tipo de mensagem
int gerenciar_mensagem_cliente() {
    pack_t pack;
    
    // Receber frame do cliente
    int result = receber_pacote(&estado_servidor, &pack);
    if(jogo.partida_iniciada == 1){
        int isSeq = seqCheck(estado_servidor.seq_atual , getSeq(pack));
        switch(isSeq){
            case 1:
                break;
            default:
                if((pack.tipo == MSG_ACK)||(pack.tipo == MSG_NACK)||(pack.tipo == MSG_OK_ACK))
                    return 0;
                //fprintf(stderr, "Seq Esperado %u Seq recebido %u\n", ((estado_servidor.seq_atual+1)%32), getSeq(pack));
                //estado_servidor.seq_atual = getSeq(pack);

                return reenvio(&estado_servidor, estado_servidor.pack);
        }

        if (result < 0) {
            if (result == -2) {
                // Timeout normal
                return 0;
            }
            return -1;
        }


        // printf("RECEBIDO SEQ: %d \n", getSeq(pack));
        // sleep(2);
        estado_servidor.seq_atual = getSeq(pack);
        // printf("ATUAL SEQ: %d \n", estado_servidor.seq_atual);
    //        sleep(2);

    }

    // Processar mensagem baseado no tipo
    switch (pack.tipo) {
        case MSG_START:
            printf("🟢 Solicitação de início do jogo recebida\n");
            jogo.partida_iniciada = 1;

            // Enviar ACK com a mesma sequência recebida
            if (enviar_ack(&estado_servidor, getSeq(pack)) < 0) {
                printf("🔴 Erro crítico ao enviar ACK\n");
                return -1;
            }
            while(1){
                // Enviar mapa inicial com nova sequência
                estado_servidor.seq_atual = (getSeq(pack) + 1) % 32;

                if (transmitir_mapa_cliente() < 0) {
                    sleep(TIMEOUT_S);
                    continue;
                }
                if(esperar_ack(&estado_servidor) < 0){
                    continue;
                }
                else
                    break;
            }
            return 1;
            
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
            return enviar_erro(&estado_servidor, getSeq(pack), SEM_PERMISSAO);
    }
    return 1;
}


int gerenciar_movimento(mensagem_type direcao) {
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
        printf("🔴 Movimento %s inválido: - posição atual: (%d,%d)\n", 
               nome_direcao, jogo.local_player.x, jogo.local_player.y);
        imprimir_movimento(nome_direcao, 0);
        
        // Enviar erro de movimento inválido
        return enviar_ack(&estado_servidor, estado_servidor.seq_atual);
    }
    
    printf("🟢 Movimento %s realizado: (%d,%d)\n", 
           nome_direcao, jogo.local_player.x, jogo.local_player.y);
    imprimir_movimento(nome_direcao, 1);

    // Mostrar mapa atualizado
    interface_servidor(&jogo);

    if (enviar_ok_ack(&estado_servidor, estado_servidor.seq_atual) < 0) {
        return -1;
    }
    
    // Verificar se há tesouro na nova posição
    int indice_tesouro = valida_tesouro(&jogo, jogo.local_player);
    if (indice_tesouro >= 0) {
        printf("🌟 Tesouro descoberto 🌟 %s na posição (%d,%d)\n", 
               jogo.tesouros[indice_tesouro].nome_tesouro,
               jogo.local_player.x, jogo.local_player.y);
        int i = 0;
        estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
        while(1){

            if(transmitir_mapa_cliente() == -4)
                return -4;
            if(esperar_ack(&estado_servidor) >= 0){
                break;
            }
            else if(i >= MAX_RETRY)
                continue;
            i += 1;
        }
        estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
        return transmitir_tesouro(indice_tesouro);
    } else {
        int j = 0;
        estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
        while(1){


            // Apenas enviar nova posição


            if (transmitir_mapa_cliente() < 0) {
                sleep(TIMEOUT_S);
                continue;
            }
            if(esperar_ack(&estado_servidor) >= 0)
                break;
            else if(j >= MAX_RETRY)
                continue;
            j += 1;
        }
        return 1;
    }
}


// Verifica se existe um tesouro na posição informada
// Retorna 1 se existir, ou 0 caso contrário
int checar_tesouro_posicao(tesouro_t treasures[MAX_TESOUROS], posicao_t pos){
    for(int i = 0; i < MAX_TESOUROS; i++){
        if((treasures[i].posicao.x == pos.x)&&
                (treasures[i].posicao.y == pos.y)){
            return 1;
        }
    }
    return 0;
}


int transmitir_mapa_cliente() {
    pack_t pack;
    struct_frame_mapa mapa_dados;
    
    // Preparar dados do mapa para o cliente
    mapa_dados.posicao_player = jogo.local_player;
    
    //confere se achou um tesouro
    mapa_dados.pegar_tesouro = checar_tesouro_posicao(jogo.tesouros, mapa_dados.posicao_player);

    if (criar_pacote(&pack, estado_servidor.seq_atual, MSG_INTERFACE, 
                (uint8_t*)&mapa_dados, sizeof(mapa_dados)) < 0) {
        return -1;
    }

    memcpy(&estado_servidor.pack, &pack, sizeof(pack_t));    
    return enviar_pacote(&estado_servidor, &pack);
}


// Envia informações do tesouro encontrado para o cliente
// Depois transmite o arquivo associado ao tesouro
int transmitir_tesouro(int indice_tesouro) {
    tesouro_t* tesouro = &jogo.tesouros[indice_tesouro];
    
    // Cria o pack
    pack_t pack;
    
    while(1){
        if (criar_pacote(&pack, estado_servidor.seq_atual, MSG_TAMANHO,
                       tesouro->tamanho, sizeof(tesouro->tamanho)) < 0) {
            fprintf(stderr, "🔴 Erro ao criar pacote do tesouro\n");
            continue;
        }
        break;
    }
    while(1){
        printf("ENVIANDO TAMANHO TESOURO\n");
        // Envia o pack
        if (enviar_pacote(&estado_servidor, &pack) < 0){
            continue;
        }


        // Aguardar ACK
        int typeAck = esperar_ack(&estado_servidor);
        if (typeAck == -4){
            continue;
            // printf("erro fatal\n");
            // return -4;  
        }
        else if(typeAck < 0){
            continue;
        }
        else{
            uint64_t tamanho_lido;
            memcpy(&tamanho_lido, tesouro->tamanho, sizeof(uint64_t));
            printf("Arquivo possui = = %llu bytes\n", (unsigned long long)tamanho_lido);
            break;
        }
    }

    // Determinar tipo do arquivo
    mensagem_type tipo = determinar_tipo_arquivo(tesouro->nome_tesouro);

    return transmitir_arquivo_tesouro(tesouro->patch, tesouro->nome_tesouro, tipo);
}


// Envia o nome e o conteúdo do arquivo de tesouro para o cliente
// Realiza o envio em blocos, aguardando ACK para cada pacote
int transmitir_arquivo_tesouro(const char* caminho_arquivo, const char* nome_tesouro, mensagem_type tipo) {

    FILE* arquivo = fopen(caminho_arquivo, "rb");
    if (!arquivo) {
        enviar_erro(&estado_servidor, estado_servidor.seq_atual, SEM_PERMISSAO);
        fprintf(stderr, "🔴 Erro ao abrir arquivo do tesouro %s \n", nome_tesouro);
        return -1;
    }

    printf("Nome do arquivo: %s\n", nome_tesouro);

    // Primeiro enviar o nome do arquivo
    pack_t pack_nome;
    estado_servidor.seq_atual = (estado_servidor.seq_atual + 1) % 32;
    if (criar_pacote(&pack_nome, estado_servidor.seq_atual, tipo, 
              (uint8_t*)nome_tesouro, strlen(nome_tesouro) + 1) < 0) {
        fclose(arquivo);
        return -1;
    }
    
    while(1) {
        if (enviar_pacote(&estado_servidor, &pack_nome) < 0) {
            sleep(TIMEOUT_S);
            continue;
        }
        if (esperar_ack(&estado_servidor) < 0) {
            sleep(TIMEOUT_S);
            continue;
        }
        break;
    }

    // Enviar o arquivo em chunks
    uint8_t buffer[MAX_FRAME];
    size_t bytes_lidos;
    size_t bytes_enviados = 0;
    
    while ((bytes_lidos = fread(buffer, 1, MAX_FRAME, arquivo)) > 0) {
        pack_t pack_dados;
        while(1){
            int seqTemp = (estado_servidor.seq_atual + 1) % 32;

               
            if (criar_pacote(&pack_dados, seqTemp, MSG_DADOS, buffer, bytes_lidos) < 0) {
                continue;
            }

            estado_servidor.seq_atual = seqTemp;
            break;
        }

        while(1) {
            printf("Bytes enviados %zu\n", bytes_enviados);
            if (enviar_pacote(&estado_servidor, &pack_dados) < 0) {
                continue;
            }
            if (esperar_ack(&estado_servidor) < 0) {
                continue;
            }
            break;
        }
        bytes_enviados += bytes_lidos;
    }

    fclose(arquivo);
    return 0;
}


// Exibe no terminal o log do movimento do jogador
// Mostra horário, direção, posição e quantidade de tesouros
void imprimir_movimento(const char* direcao, int sucesso) {
    time_t now = time(NULL);
    char* time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remover \n
    
    printf("[%s] Movimento %s: %s - Posição: (%d,%d) - Tesouros: %d/%d\n",
           time_str, direcao, sucesso ? "OK" : "INVÁLIDO",
           jogo.local_player.x, jogo.local_player.y,
           jogo.tesouros_achados, MAX_TESOUROS);
}
