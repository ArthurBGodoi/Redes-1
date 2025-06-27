#include "protocolo.h"
#include "rawSocket.h"

#define DIRETORIO_TESOUROS "./transferidos/"

//////////// Protótipos das funções ////////////

// Cria o socket raw e configura o endereço do servidor
// Inicializa o estado do protocolo do cliente
int conectar_com_servidor(struct_cliente* cliente, const char* ip_servidor, int porta);

// Envia o comando para iniciar o jogo e aguarda confirmação do servidor
// Após o ACK, recebe o mapa inicial e configura o estado do cliente
int requisitar_inicio_jogo(struct_cliente* cliente);

// Exibe o mapa atualizado do cliente no terminal
// Mostra posição atual, tesouros coletados e áreas exploradas
void exibir_mapa_cliente(struct_cliente* cliente);

// Exibe o menu de opções de movimentação do jogador
// Mostra as teclas disponíveis para se deslocar ou sair do jogo
void imprimir_opcoes_movimento();

// Processa o comando do jogador e envia o movimento correspondente ao servidor
// Valida o comando, transmite o movimento e aguarda resposta
int gerenciar_comando_movimento(struct_cliente* cliente, char comando);

// Cria e envia o pacote de movimento solicitado pelo jogador
// Incrementa a sequência e transmite via protocolo do cliente
int transmitir_movimento(struct_cliente* cliente, mensagem_type tipo_movimento);

// Processa a resposta recebida do servidor e atualiza o estado do cliente
// Trata erros, movimentação e possível coleta de tesouros
int gerenciar_resposta_servidor(struct_cliente* cliente);

// Recebe as informações e o arquivo do tesouro enviado pelo servidor
// Confirma o recebimento do tamanho e processa o arquivo recebido
int baixar_tesouro(struct_cliente* cliente);

// Recebe o arquivo do tesouro em blocos e salva no diretório local
// Garante integridade com ACKs e exibe o conteúdo ao final
int salvar_tesouro(struct_cliente* cliente, const char* nome_tesouro, mensagem_type tipo, uint64_t tamanho);

// Exibe o conteúdo do tesouro recebido, conforme o tipo identificado
// Mostra vídeo, imagem ou texto e imprime o nome do tesouro
void visualizar_tesouro(const char* nome_tesouro, const char* caminho_completo, mensagem_type tipo);

void reseta_interface();

// Retorna o nome textual correspondente ao código de direção
// Retorna "DESCONHECIDO" se o código for inválido
const char* converter_direcao(mensagem_type tipo);


// Configura o terminal para modo raw, desativando buffer e eco de teclas
// Permite leitura imediata de teclas no terminal do cliente (sem buffer)
void configurar_terminal_raw(struct termios *orig) 
{
    tcgetattr(STDIN_FILENO, orig);
    struct termios raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO);  // Desativa buffer de linha e eco
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


// Restaura o terminal para o modo padrão de funcionamento
// Reativa o buffer e o eco das teclas no terminal
void restaurar_modo_terminal(struct termios *orig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}



int main(int argc, char* argv[]) {
    struct termios orig_termios;  // Variável para guardar o estado original
    configurar_terminal_raw(&orig_termios);

    //cria variaveis do programa
    struct_cliente cliente;
    char ip_servidor[16];
    int porta_servidor = PORTA_SERVIDOR;
    
    // Limpar estrutura do cliente
    memset(&cliente, 0, sizeof(cliente));
    
    printf("====== MINIMAPA GRID 8x8 CLIENTE ======\n");
    
    // Obter IP do servidor
    if (argc > 1) {
        strcpy(ip_servidor, argv[1]);
    } else {
        printf("Digite o IP do servidor: ");
        if (scanf("%15s", ip_servidor) != 1) {
            fprintf(stderr, "🔴 Erro ao ler IP do servidor\n");
            return 1;
        }
    }
    
    // Conectar ao servidor
    if (conectar_com_servidor(&cliente, ip_servidor, porta_servidor) < 0) {
        fprintf(stderr, "🔴 Erro ao conectar ao servidor\n");
        return 1;
    }
    
    printf("Conectado ao servidor %s:%d\n", ip_servidor, porta_servidor);
    
    // Criar diretório de tesouros se não existir
    system("mkdir -p " DIRETORIO_TESOUROS);
    
    // Iniciar jogo
    if (requisitar_inicio_jogo(&cliente) < 0) {
        fprintf(stderr, "🔴 Erro ao iniciar jogo\n");
        finalizar_protocolo(&cliente.protocolo);
        return 1;
    }
    

    printf("🟢 Jogo iniciado! Encontre todos os tesouros.\n");
    printf("ENTER continuar...");
    getchar(); // Aguardar ENTER

    // Loop principal do jogo
    while (cliente.jogo_ativo) {
        reseta_interface();
        exibir_mapa_cliente(&cliente);
        imprimir_opcoes_movimento();
        
        char comando;
        printf("Digite sua escolha: ");        
        int entrada = getchar();
    
        // Converte para char se não for EOF
        if (entrada != EOF) 
            comando = (char)entrada; 
        else
            continue;
        
        if (comando == 'q') {
            printf("Saindo...\n");
            break;
        }
        
        int p = gerenciar_comando_movimento(&cliente, comando);
        if (p == -4){
            reseta_interface();
            perror("Erro fatal!!! Digite ENTER para matar o progrma\n");
            finalizar_protocolo(&cliente.protocolo);
            getchar();
            restaurar_modo_terminal(&orig_termios);
            return 0;
        }
        if (p < 0)
            continue;
        if(cliente.mapa_ativo.numero_tesouros == MAX_TESOUROS)
            cliente.jogo_ativo = 0;
    }

    // encerra o programa
    reseta_interface();
    finalizar_protocolo(&cliente.protocolo);
    printf("Jogo Finalizado. Para sair aperte ENTER\n");
    getchar();
    restaurar_modo_terminal(&orig_termios);
    return 0;
}



// Cria o socket raw e configura o endereço do servidor
// Inicializa o estado do protocolo do cliente
int conectar_com_servidor(struct_cliente* cliente, const char* ip_servidor, int porta) 
{
    // Inicializar protocolo com raw socket
    if (inicializar_protocolo(&cliente->protocolo, ip_servidor, PORTA_CLIENTE, porta, INTERFACE_PADRAO) < 0) {
        fprintf(stderr, "Erro ao inicializar protocolo cliente\n");
        return -1;
    }
    
    return 0;
}

// Verifica se há tesouro registrado na posição (x, y)
// Retorna 1 se existir, ou 0 caso contrário
int buscar_tesouro_mapa(posicao_t local_tesouro[MAX_TESOUROS], int numero_tesouros, int x, int y){
    for(int i = 0; i < numero_tesouros; i++){
        if((local_tesouro[i].x == x)&&
                (local_tesouro[i].y == y)){
            return 1;
        }
    }
    return 0;
}


// Registra um novo tesouro coletado na posição atual do jogador
// Retorna 1 se um novo tesouro for salvo, ou 0 se já existia
int registrar_tesouro(mapa_cliente_t *clientMap){
    if(!buscar_tesouro_mapa(clientMap->local_tesouro,
                clientMap->numero_tesouros, clientMap->posicao_player.x, clientMap->posicao_player.y)){
        clientMap->local_tesouro[clientMap->numero_tesouros].x = clientMap->posicao_player.x;
        clientMap->local_tesouro[clientMap->numero_tesouros].y = clientMap->posicao_player.y;
        ++(clientMap->numero_tesouros);
        return 1;
    }
    return 0;
}


// Atualiza o mapa do cliente com as informações recebidas do servidor
// Registra o tesouro ou marca a posição atual como explorada
void atualizar_mapa(mapa_cliente_t *clientMap, struct_frame_mapa frameMap, int *newTreasure){

    clientMap->posicao_player.x =frameMap.posicao_player.x;
    clientMap->posicao_player.y =frameMap.posicao_player.y;

    if(frameMap.pegar_tesouro){
        *newTreasure = registrar_tesouro(clientMap);
    }
    else
        clientMap->local_explorado[clientMap->posicao_player.x][clientMap->posicao_player.y] = 1;
}



// Envia o comando para iniciar o jogo e aguarda confirmação do servidor
// Após o ACK, recebe o mapa inicial e configura o estado do cliente
int requisitar_inicio_jogo(struct_cliente* cliente) {
    // Enviar comando para iniciar jogo
    pack_t frame_inicio;
    criar_pacote(&frame_inicio, cliente->protocolo.seq_atual, MSG_START, NULL, 0);
    
    for(int i = 0; i < TAMANHO_MAPA; i++){
        for(int j=0; j < TAMANHO_MAPA; j++){
            cliente->mapa_ativo.local_explorado[i][j] = 0;
        }
    }

    while(1){
        if (enviar_pacote(&cliente->protocolo, &frame_inicio) < 0) {
            printf("🔴 Erro ao enviar comando de início\n");
            sleep(TIMEOUT_S);
            continue;
        }
        else
            printf("🟢 Comando de inicio enviado Seq: %d \n", cliente->protocolo.seq_atual);

        // Aguardar ACK do servidor
        if (esperar_ack(&cliente->protocolo) < 0) {
            printf(" 🟡 Servidor não confirmou início do jogo\n");
            continue;
        }
        else{
            printf(" 🟢 Servidor confirmou início do jogo\n");
            break;
        }
    }

    while(1){
        // Receber mapa inicial
        pack_t frameRecebido;
        if (receber_pacote(&cliente->protocolo, &frameRecebido) < 0) {
            printf("🔴 Erro ao receber mapa.\n");
            enviar_nack(&cliente->protocolo, getSeq(frameRecebido));
            continue;
        }
        else{
            printf("🟢 Recebido mapa inicial\n");
            cliente->protocolo.seq_atual = getSeq(frameRecebido);
            enviar_ack(&cliente->protocolo, getSeq(frameRecebido));
        }
        if (frameRecebido.tipo != MSG_INTERFACE) {
            printf("Resposta inesperada do servidor: tipo %d\n", frameRecebido.tipo);
            continue;
        }
        struct_frame_mapa frameMapa;
        memcpy(&frameMapa, frameRecebido.dados, sizeof(struct_frame_mapa));

        // Copiar dados do mapa
        cliente->mapa_ativo.numero_tesouros = 0;
        atualizar_mapa(&cliente->mapa_ativo, frameMapa, 0); 
        cliente->jogo_ativo = 1;
        cliente->tesouros_obtidos = cliente->mapa_ativo.numero_tesouros;
        break;
    }

    return 0;
}



// Exibe o mapa atualizado do cliente no terminal
// Mostra posição atual, tesouros coletados e áreas exploradas
void exibir_mapa_cliente(struct_cliente* cliente) {
    printf("\n=== MAPA DO CAÇA AO TESOURO ===\n");
    printf("Posição atual: (%d,%d)\n", 
        cliente->mapa_ativo.posicao_player.x, 
           cliente->mapa_ativo.posicao_player.y);
    printf("Tesouros encontrados: %d/8\n", cliente->mapa_ativo.numero_tesouros);
    
    printf("\nLegenda: 🗿 = Você, ✅ = Tesouro achado, 😓 = Explorado, ⚫️ = Não Explorado\n");
    printf("────────────────────────────────────────\n");
    
    // Mostrar grid (y crescente para cima)
    for (int y = TAMANHO_MAPA - 1; y >= 0; y--) {
        printf("%d │", y);
        for (int x = 0; x < TAMANHO_MAPA; x++) {
            if (cliente->mapa_ativo.posicao_player.x == x && 
                cliente->mapa_ativo.posicao_player.y == y) {
                printf(" 🗿");
            } else if (buscar_tesouro_mapa(cliente->mapa_ativo.local_tesouro, cliente->mapa_ativo.numero_tesouros, x, y)) {
                printf(" ✅");
            } else if (cliente->mapa_ativo.local_explorado[x][y]) {
                printf(" 😓");
            } else {
                printf(" ⚫️");
            }
        }
        printf("\n");
    }
    
    printf("  └");
    for (int i = 0; i < TAMANHO_MAPA; i++) {
        printf("───");
    }
    printf("\n   ");
    for (int x = 0; x < TAMANHO_MAPA; x++) {
        printf(" %d ", x);
    }
    printf("\n────────────────────────────────────────\n");
}



// Exibe o menu de opções de movimentação do jogador
// Mostra as teclas disponíveis para se deslocar ou sair do jogo
void imprimir_opcoes_movimento() {
    printf("\nMOVIMENTOS DISPONÍVEIS:\n");
    printf("w. Mover para CIMA\n");
    printf("a. Mover para BAIXO\n");
    printf("s. Mover para ESQUERDA\n");
    printf("d. Mover para DIREITA\n");
    printf("q. Sair do jogo\n");
    printf("═══════════════════════════════\n");
}



// Processa o comando do jogador e envia o movimento correspondente ao servidor
// Valida o comando, transmite o movimento e aguarda resposta
int gerenciar_comando_movimento(struct_cliente* cliente, char comando) {
    mensagem_type tipo_movimento;
    switch (comando) {
        case 'w':
            tipo_movimento = MSG_MOVE_CIMA;
            break;
        case 's':
            tipo_movimento = MSG_MOVE_BAIXO;
            break;
        case 'a':
            tipo_movimento = MSG_MOVE_ESQUERDA;
            break;
        case 'd':
            tipo_movimento = MSG_MOVE_DIREITA;
            break;
        default:
            printf("Comando inválido!\n");
            return -1;
    }
    
    printf("🟢 Enviando movimento: %s...\n", converter_direcao(tipo_movimento));
    

    // Enviar movimento para o servidor
    if (transmitir_movimento(cliente, tipo_movimento) < 0) {
        return -1;
    }
    
    // Processar resposta do servidor
    return gerenciar_resposta_servidor(cliente);
}



// Cria e envia o pacote de movimento solicitado pelo jogador
// Incrementa a sequência e transmite via protocolo do cliente
int transmitir_movimento(struct_cliente* cliente, mensagem_type tipo_movimento) {
    pack_t frame_movimento;


    cliente->protocolo.seq_atual = (cliente->protocolo.seq_atual + 1) % 32;
    
    criar_pacote(&frame_movimento, cliente->protocolo.seq_atual, tipo_movimento, NULL, 0);
    while(1){
        enviar_pacote(&cliente->protocolo, &frame_movimento);
        int i = esperar_ack(&cliente->protocolo);
        if(i < 0){

            continue;
        }
        if(i == 0){
            printf("🔴 Movimento inválido!\n");
            printf("ENTER para continuar...");
            getchar();
            return -1;
        }
        else if (i == 1){
            printf("🟢 Movimento valido!\n");
            break;
        }
        else{
            printf("erro na menssagem recebida!\n");
            continue;
        }
    }
    return 0;
}




// Processa a resposta recebida do servidor e atualiza o estado do cliente
// Trata erros, movimentação e possível coleta de tesouros
int gerenciar_resposta_servidor(struct_cliente* cliente) {
    pack_t frame_resposta;
    struct_frame_mapa frameMapa;

    // Receber resposta do servidor
    int result = receber_pacote(&cliente->protocolo, &frame_resposta);
    if (result < 0) {
        if (result == -2) {
            printf("Timeout - servidor não respondeu\n");
        } else {
            printf("🔴 Erro ao receber resposta do servidor\n");
        }
        return -1;
    }

    uint8_t tipo = frame_resposta.tipo;
    uint8_t seq = getSeq(frame_resposta);
    int isSeq = seqCheck( cliente->protocolo.seq_atual, seq);
    if(isSeq != 1){
        return reenvio(&cliente->protocolo, cliente->protocolo.pack);
    }
    int newTreasure = 0;
    switch (tipo) {
        case MSG_ERRO:
            cliente->protocolo.seq_atual = getSeq(frame_resposta);
            printf("🔴 Erro do servidor: %d\n", frame_resposta.dados[0]);
            printf("ENTER continuar...");
            getchar();
            return 0;

        case MSG_INTERFACE:
            cliente->protocolo.seq_atual = getSeq(frame_resposta);
            memcpy(&frameMapa, frame_resposta.dados, sizeof(struct_frame_mapa));
            atualizar_mapa(&cliente->mapa_ativo, frameMapa, &newTreasure);


            if (cliente->mapa_ativo.numero_tesouros > cliente->tesouros_obtidos) {
                printf("🌟 Tesouro descoberto! 🌟\n");
                cliente->tesouros_obtidos = cliente->mapa_ativo.numero_tesouros;
            }
            enviar_ack(&cliente->protocolo, cliente->protocolo.seq_atual); // usa seq recuperado com macro
            if(frameMapa.pegar_tesouro && newTreasure)
                if(baixar_tesouro(cliente) == -4)
                    return -4;
            return 0;
       
        default:
            printf("Resposta inesperada do servidor: tipo %d\n", tipo);
            return -1;
    }
}



// Recebe as informações e o arquivo do tesouro enviado pelo servidor
// Confirma o recebimento do tamanho e processa o arquivo recebido
int baixar_tesouro(struct_cliente* cliente) {
    //Preparando pra receber o tipo tamanho
    pack_t pack;
    int debug = receber_pacote(&cliente->protocolo, &pack);

    //tenta receber
    while(1){
        if (debug < 0) {
            printf("🔴 Erro ao receber informações do tesouro\n");
            debug = receber_pacote(&cliente->protocolo, &pack);
            continue;
        }
        if(pack.tipo != MSG_TAMANHO){
            debug = receber_pacote(&cliente->protocolo, &pack);
            continue;
        }
        else{
            uint64_t tamanho_lido;
            enviar_ack(&cliente->protocolo, getSeq(pack));
            uint8_t tam[MAX_FRAME];
            memcpy(tam, pack.dados, sizeof(uint64_t));
            while(1){
                memcpy(&tamanho_lido, tam, sizeof(uint64_t));
                printf("Arquivo do tesouro possui = %llu bytes\n", (unsigned long long)tamanho_lido);
                break;
            }
            receber_pacote(&cliente->protocolo, &pack);
            int tipo = pack.tipo;
            if(tipo >= MSG_TEXTO_ACK_NOME && tipo <= MSG_IMAGEM_ACK_NOME){

                mensagem_type tipo_arquivo = pack.tipo;
                char nome_tesouro[256];

                strncpy(nome_tesouro, (char*)pack.dados, pack.tamanho);
            
                uint64_t tamanhoLivre = obter_espaco_livre(DIRETORIO_TESOUROS);
                if(tamanhoLivre < tamanho_lido){
                    fprintf(stderr, "impossivel armazenar tamanho disponivel: %llu, tamanho necessario: %llu\n", (unsigned long long) tamanhoLivre, (unsigned long long) tamanho_lido);
                    printf("Pressione ENTER para continuar...\n");
                    getchar();
                    enviar_erro(&cliente->protocolo, getSeq(pack), ESPACO_INSUFICIENTE);
                    return -4;
                }
                enviar_ack(&cliente->protocolo, getSeq(pack));
                printf("Tamanho disponivel: %llu, tamanho necessario: %llu\n", (unsigned long long) tamanhoLivre, (unsigned long long) tamanho_lido);
                return salvar_tesouro(cliente, nome_tesouro, tipo_arquivo, tamanho_lido);
            }
            else if(tipo == MSG_ERRO){
                perror("Erro ao abrir arquivo do tesouro\n");
                printf("ENTER para continuar...\n");
                getchar();
                return -4;
            }
            else
                continue;
            break;
        }
    }

    return 0;
}



// Recebe o arquivo do tesouro em blocos e salva no diretório local
// Garante integridade com ACKs e exibe o conteúdo ao final
int salvar_tesouro(struct_cliente* cliente, const char* nome_tesouro, mensagem_type tipo, uint64_t tamanho) {
    char caminho_completo[512];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s%s", DIRETORIO_TESOUROS, nome_tesouro);

    FILE* arquivo = fopen(caminho_completo, "wb");
    if (!arquivo) {
        perror("Erro ao criar arquivo do tesouro");
        return -1;
    }

    uint64_t bytes_recebidos = 0;
    pack_t pack;
    uint8_t seqAtual;
    uint8_t seq = -1;

    while (bytes_recebidos < tamanho) {
        memset(&pack, 0, sizeof(pack));
        if (receber_pacote(&cliente->protocolo, &pack) < 0) {
            printf("🔴 Erro ao receber dados do tesouro\n");
            continue;
        }

        // Verificar tipo de dados
        if (pack.tipo != MSG_DADOS){
            printf("Tipo de pacote inesperado: %d\n", pack.tipo);
            continue;
        }

        
        seqAtual = getSeq(pack);
        if(seq == seqAtual){
            enviar_ack(&cliente->protocolo, getSeq(pack));
            continue;
        }

        if((pack.tamanho < 127) && ((tamanho - (uint64_t)bytes_recebidos) > (uint64_t)pack.tamanho)){
            enviar_nack(&cliente->protocolo, getSeq(pack));
            continue;
        }
        else if  ((pack.tamanho < 127) && ((tamanho - (uint64_t)bytes_recebidos) < (uint64_t)pack.tamanho)){
            enviar_nack(&cliente->protocolo, getSeq(pack));
            continue;
    }
        // Escrever dados no arquivo
        size_t bytes_escritos = fwrite(pack.dados, 1, pack.tamanho, arquivo);
        
        if (bytes_escritos != pack.tamanho) {
            perror("Erro ao escrever no arquivo");
            continue;
        }
        
        seq = seqAtual;
        cliente->protocolo.seq_atual = seq;
        bytes_recebidos += bytes_escritos;
        printf("🟢 Pacote recebido %u (%zd / %llu)\n", getSeq(pack) ,bytes_recebidos, (unsigned long long)tamanho);
        enviar_ack(&cliente->protocolo, getSeq(pack));
    }
    fclose(arquivo);


    // Se estiver rodando como root, tenta pegar o dono real
    const char* user_name = getenv("SUDO_USER");
    if (!user_name) {
        // Não foi com sudo, usa o UID atual mesmo
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            chown(caminho_completo, pw->pw_uid, pw->pw_gid);
        }
    } else {
        // Foi com sudo, pega informações do usuário real
        struct passwd* pw = getpwnam(user_name);
        if (pw) {
            chown(caminho_completo, pw->pw_uid, pw->pw_gid);
        }
    }
    visualizar_tesouro(nome_tesouro, caminho_completo, tipo);

    return 0;
}




// Exibe o conteúdo textual de um tesouro no terminal
// Lê o arquivo linha por linha e aguarda ENTER ao final
void visualizar_texto(const char *caminho_arquivo) {
    FILE *arquivo = fopen(caminho_arquivo, "r");
    if (arquivo == NULL) {
        perror("Erro ao abrir arquivo");
        return;
    }

    char linha[1024];
    printf("\n========= Conteúdo do Tesouro (Texto) =========\n");
    while (fgets(linha, sizeof(linha), arquivo)) {
        printf("%s", linha);
    }
    fclose(arquivo);

    printf("\n\nENTER para continuar...\n");
    getchar();

}


// Abre uma imagem do tesouro utilizando o visualizador padrão do sistema
// Aguarda o usuário pressionar ENTER após a visualização
void visualizar_imagem(const char *caminho) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdg-open %s || display %s || eog %s &", caminho, caminho, caminho);
    system(cmd);

    printf("ENTER para continuar...\n");
    getchar();
}



// Abre um vídeo do tesouro utilizando o visualizador ou player disponível
// Aguarda o usuário pressionar ENTER após a abertura do vídeo
void visualizar_video(const char *caminho) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "xdg-open \"%s\" > /dev/null 2>&1 || mpv \"%s\" --quiet > /dev/null 2>&1 || vlc \"%s\" --play-and-exit > /dev/null 2>&1 &",
        caminho, caminho, caminho);
    system(cmd);

    printf("ENTER para continuar...\n");
    getchar();
}




// Exibe o conteúdo do tesouro recebido, conforme o tipo identificado
// Mostra vídeo, imagem ou texto e imprime o nome do tesouro
void visualizar_tesouro(const char* nome_tesouro, const char* caminho_arquivo, mensagem_type tipo){
    switch(tipo){
        case MSG_VIDEO_ACK_NOME:
            printf("Tesouro em vídeo (%s)\n", nome_tesouro);
            visualizar_video(caminho_arquivo);
            return;
        case MSG_IMAGEM_ACK_NOME:
            printf("Tesouro em imagem (%s)\n", nome_tesouro);
            visualizar_imagem(caminho_arquivo);
            return;
        case MSG_TEXTO_ACK_NOME:
            printf("Tesouro em texto (%s)\n", nome_tesouro);
            visualizar_texto(caminho_arquivo);
            return;
        default:
            return;
    }
}

// Retorna o nome textual correspondente ao código de direção
// Retorna "DESCONHECIDO" se o código for inválido
const char* converter_direcao(mensagem_type tipo) {
    switch (tipo) {
        case MSG_MOVE_CIMA: return "CIMA";
        case MSG_MOVE_BAIXO: return "BAIXO";
        case MSG_MOVE_ESQUERDA: return "ESQUERDA";
        case MSG_MOVE_DIREITA: return "DIREITA";
        default: return "DESCONHECIDO";
    }
}
