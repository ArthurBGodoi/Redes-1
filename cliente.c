#include "protocolo.h"
#include "cliente.h"

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
void restaurar_modo_terminal(struct termios *orig) 
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);  // Volta ao original
}


int main(int argc, char* argv[]) 
{
    struct termios orig_termios;  // Variável global para guardar o estado original
    configurar_terminal_raw(&orig_termios);
    struct_cliente cliente;
    char ip_servidor[16];
    int porta_servidor = 50969;
    
    // Limpar estrutura do cliente
    memset(&cliente, 0, sizeof(cliente));
    
    printf("\n====== MINIMAPA GRID 8x8 CLIENTE ======\n");
    
    // Obter IP do servidor
    if (argc > 1) {
        strcpy(ip_servidor, argv[1]);
    } else {
        printf("Digite o IP do servidor: ");
        if (scanf("%15s", ip_servidor) != 1) {
            fprintf(stderr, "Erro ao ler IP do servidor\n");
            return 1;
        }
    }
    
    // Conectar ao servidor
    if (conectar_com_servidor(&cliente, ip_servidor, porta_servidor) < 0) {
        fprintf(stderr, "Erro ao conectar ao servidor\n");
        return 1;
    }
    
    printf("Conectado ao servidor %s:%d\n", ip_servidor, porta_servidor);
    
    // Criar diretório de tesouros se não existir
    system("mkdir -p " DIRETORIO_TESOUROS);
    
    // Iniciar jogo
    if (requisitar_inicio_jogo(&cliente) < 0) {
        fprintf(stderr, "Erro ao iniciar jogo\n");
        close(cliente.protocolo.socket_fd);
        return 1;
    }
    
    printf("Jogo em andamento! Colete todos os tesouros\n");
    printf("Aperte ENTER para continuar");
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
            printf("Saindo do jogo...\n");
            break;
        }
        
        if (gerenciar_comando_movimento(&cliente, comando) < 0)
            continue;
        if(cliente.mapa_ativo.numero_tesouros == MAX_TESOUROS)
            cliente.jogo_ativo = 0;
    }
    reseta_interface();
    close(cliente.protocolo.socket_fd);
    printf("Jogo Finalizado. Para sair aperte ENTER.\n");
    getchar();

    restaurar_modo_terminal(&orig_termios);
    return 0;
}


// Cria o socket UDP e configura o endereço do servidor
// Inicializa o estado do protocolo do cliente
int conectar_com_servidor(struct_cliente* cliente, const char* ip_servidor, int porta) 
{
    // Criar socket UDP
    cliente->protocolo.socket_fd = socket(AF_INET, SOCKET_RAW, 0);
    if (cliente->protocolo.socket_fd < 0) {
        perror("Erro ao criar socket");
        return -1;
    }
    
    // Configurar endereço do servidor
    memset(&cliente->protocolo.ip_remoto, 0, sizeof(cliente->protocolo.ip_remoto));
    cliente->protocolo.ip_remoto.sin_family = AF_INET;
    cliente->protocolo.ip_remoto.sin_port = htons(porta);
    
    if (inet_pton(AF_INET, ip_servidor, &cliente->protocolo.ip_remoto.sin_addr) <= 0) {
        fprintf(stderr, "🔴 Endereço IP incorreto: %s\n", ip_servidor);
        close(cliente->protocolo.socket_fd);
        return -1;
    }
    
    // Inicializar estado do protocolo
    cliente->protocolo.sequencia_atual = 0;
    cliente->protocolo.sequencia_seguinte = 0;
    cliente->protocolo.ip_tamanho = sizeof(cliente->protocolo.ip_remoto);
    
    return 0;
}


// Verifica se há tesouro registrado na posição (x, y)
// Retorna 1 se existir, ou 0 caso contrário
int buscar_tesouro_mapa(struct_coordenadas local_tesouro[MAX_TESOUROS], int numero_tesouros, int x, int y)
{
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
int registrar_tesouro(struct_mapa *clientMap)
{
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
void atualizar_mapa(struct_mapa *clientMap, struct_frame_mapa frameMap, int *newTreasure)
{

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
int requisitar_inicio_jogo(struct_cliente* cliente) 
{
    // Enviar comando para iniciar jogo
    struct_frame_pacote frame_inicio;
    cria_pacote(&frame_inicio, cliente->protocolo.sequencia_atual, MSG_START, NULL, 0);
    
    for(int i = 0; i < MAX_GRID; i++){
        for(int j=0; j < MAX_GRID; j++){
            cliente->mapa_ativo.local_explorado[i][j] = 0;
        }
    }

    while(1){
        if (envia_pacote(&cliente->protocolo, &frame_inicio) < 0) {
            printf("🔴 Erro ao enviar comando de início do jogo\n");
            continue;
        }
        else
            printf("🟢 Comando de início enviado ao servidor\n");

        // Aguardar ACK do servidor
        if (aguarda_ack(&cliente->protocolo) < 0) {
            printf("🟡 Servidor não confirmou o início do jogo.\n");
            continue;
        }
        else{
            printf("🟢 Servidor confirmou o início do jogo\n");
            break;
        }
    }

    while(1){
        // Receber mapa inicial
        struct_frame_pacote frameRecebido;
        if (recebe_pacote(&cliente->protocolo, &frameRecebido) < 0) {
            printf("🔴 Erro ao receber mapa.\n");
            envia_nack(&cliente->protocolo, ler_sequencia(frameRecebido));
            continue;
        }
        else{
            printf("🟢 Mapa inicial recebido\n");
            envia_ack(&cliente->protocolo, ler_sequencia(frameRecebido));
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
void exibir_mapa_cliente(struct_cliente* cliente) 
{
    printf("\n=== MAPA DO CAÇA AO TESOURO ===\n");
    printf("Tesouros achados: %d/8\n", cliente->mapa_ativo.numero_tesouros);
    printf("Posição atual: (%d,%d)\n", cliente->mapa_ativo.posicao_player.x, cliente->mapa_ativo.posicao_player.y);
    
    printf("\nLegenda: 🗿 = Você, ✅ = Tesouro achado, 😓 = Explorado, ⚫️ = Não Explorado\n");
    printf("====================================\n"); 
    
    // Mostrar grid (y crescente para cima)
    for (int y = MAX_GRID - 1; y >= 0; y--) {
        printf("%d │", y);
        for (int x = 0; x < MAX_GRID; x++) {
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
    for (int i = 0; i < MAX_GRID; i++) {
        printf("=======");
    }
    printf("\n   ");
    for (int x = 0; x < MAX_GRID; x++) {
        printf(" %d ", x);
    }
    printf("\n====================================\n");
}


// Exibe o menu de opções de movimentação do jogador
// Mostra as teclas disponíveis para se deslocar ou sair do jogo
void imprimir_opcoes_movimento() 
{
    printf("\n====== INSTRUÇÕES =====\n");
    printf("w.  Desloca para CIMA\n");
    printf("a.  Desloca para BAIXO\n");
    printf("s.  Desloca para ESQUERDA\n");
    printf("d.  Desloca para DIREITA\n");
    printf("═══════════════════════════════\n");
}


// Processa o comando do jogador e envia o movimento correspondente ao servidor
// Valida o comando, transmite o movimento e aguarda resposta
int gerenciar_comando_movimento(struct_cliente* cliente, char comando) 
{
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
    
    printf("🟢 Enviando direção: %s...\n", converter_direcao(tipo_movimento));
    
    // Enviar movimento para o servidor
    if (transmitir_movimento(cliente, tipo_movimento) < 0) {
        return -1;
    }
    
    // Processar resposta do servidor
    return gerenciar_resposta_servidor(cliente);
}


// Cria e envia o pacote de movimento solicitado pelo jogador
// Incrementa a sequência e transmite via protocolo do cliente
int transmitir_movimento(struct_cliente* cliente, mensagem_type tipo_movimento) 
{
    struct_frame_pacote frame_movimento;
    cliente->protocolo.sequencia_atual = (cliente->protocolo.sequencia_atual + 1) % 32;
    
    cria_pacote(&frame_movimento, cliente->protocolo.sequencia_atual, tipo_movimento, NULL, 0);
    
    return envia_pacote(&cliente->protocolo, &frame_movimento);
}


// Processa a resposta recebida do servidor e atualiza o estado do cliente
// Trata erros, movimentação e possível coleta de tesouros
int gerenciar_resposta_servidor(struct_cliente* cliente) 
{
    struct_frame_pacote frame_resposta;
    struct_frame_mapa frameMapa;

    // Receber resposta do servidor
    int result = recebe_pacote(&cliente->protocolo, &frame_resposta);
    if (result < 0) {
        if (result == -2) {
            printf("🟡 Timeout - servidor não respondeu\n");
        } else {
            printf("🔴 Erro ao receber resposta do servidor\n");
        }
        return -1;
    }

    uint8_t tipo = frame_resposta.tipo;
    uint8_t seq = ler_sequencia(frame_resposta);
    int newTreasure = 0;
    switch (tipo) {
        case MSG_ERRO:
            if (frame_resposta.dados[0] == MOVIMENTO_PROIBIDO) {
                printf("🔴 Movimento inválido!\n");
            } else {
                printf("🔴 Erro do servidor: %d\n", frame_resposta.dados[0]);
            }
            printf("Pressione ENTER...");
            getchar();
            return 0;

        case MSG_INTERFACE:

            memcpy(&frameMapa, frame_resposta.dados, sizeof(struct_frame_mapa));
            atualizar_mapa(&cliente->mapa_ativo, frameMapa, &newTreasure);
            printf("🟢 Movimento ok!\n");

            if (cliente->mapa_ativo.numero_tesouros > cliente->tesouros_obtidos) {
                printf("🌟 Tesouro descoberto!\n");
                cliente->tesouros_obtidos = cliente->mapa_ativo.numero_tesouros;
            }
            envia_ack(&cliente->protocolo, seq); // usa seq recuperado com macro
            if(frameMapa.pegar_tesouro && newTreasure)
                baixar_tesouro(cliente);
            return 0;
       
        default:
            printf("Resposta inesperada SERVIDOR: tipo %d\n", tipo);
            return -1;
    }
}


// Recebe as informações e o arquivo do tesouro enviado pelo servidor
// Confirma o recebimento do tamanho e processa o arquivo recebido
int baixar_tesouro(struct_cliente* cliente) 
{
    //Preparando pra receber o tipo tamanho
    struct_frame_pacote pack;
    int debug = recebe_pacote(&cliente->protocolo, &pack);

    //tenta receber
    while(1){
        if (debug < 0) {
            printf("🔴 Erro ao receber informações do tesouro\n");
            debug = recebe_pacote(&cliente->protocolo, &pack);
            continue;
        }
        if(pack.tipo != MSG_TAMANHO){
            debug = recebe_pacote(&cliente->protocolo, &pack);
            continue;
        }
        else{
            uint64_t tamanho_lido;
            envia_ack(&cliente->protocolo, ler_sequencia(pack));
            uint8_t tam[TAM_MAX_DADOS];
            memcpy(tam, pack.dados, sizeof(uint64_t));
            while(1){
                memcpy(&tamanho_lido, tam, sizeof(uint64_t));
                printf("Arquivo do tesouro possui = %llu bytes\n", (unsigned long long)tamanho_lido);
                break;
            }
            recebe_pacote(&cliente->protocolo, &pack);
            int tipo = pack.tipo;
            if(tipo >= MSG_TEXTO_ACK_NOME && tipo <= MSG_IMAGEM_ACK_NOME){
                envia_ack(&cliente->protocolo, ler_sequencia(pack));
                mensagem_type tipo_arquivo = pack.tipo;
                char nome_tesouro[256];

                strncpy(nome_tesouro, (char*)pack.dados, pack.tamanho);
                return salvar_tesouro(cliente, nome_tesouro, tipo_arquivo, tamanho_lido);
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
int salvar_tesouro(struct_cliente* cliente, const char* nome_tesouro, mensagem_type tipo, uint64_t tamanho) 
{
    char patch[512];
    snprintf(patch, sizeof(patch), "%s%s", DIRETORIO_TESOUROS, nome_tesouro);

    FILE* arquivo = fopen(patch, "wb");
    if (!arquivo) {
        perror("Erro ao criar arquivo do tesouro");
        return -1;
    }

    uint64_t bytes_recebidos = 0;
    struct_frame_pacote pack;

    uint8_t seqAtual;
    uint8_t seq = -1;

    while (bytes_recebidos < tamanho) {
        if (recebe_pacote(&cliente->protocolo, &pack) < 0) {
            printf("🔴 Erro ao receber dados do tesouro\n");
            continue;
        }

        // Verificar tipo de dados
        if (pack.tipo != MSG_DADOS){
            printf("Tipo de pacote inesperado: %d\n", pack.tipo);
            continue;
        }
        
        seqAtual = ler_sequencia(pack);
        if(seq == seqAtual){
            envia_ack(&cliente->protocolo, ler_sequencia(pack));
            continue;        
        }

        // Escrever dados no arquivo
        size_t bytes_escritos = fwrite(pack.dados, 1, pack.tamanho, arquivo);
      //  bytes_escritos+=1;
        
        if (bytes_escritos != pack.tamanho) {
            perror("Erro ao escrever no arquivo");
            continue;
        }

        seq = seqAtual;
        bytes_recebidos += bytes_escritos;
        printf("Progresso: (%zd / %llu)\n", bytes_recebidos, (unsigned long long)tamanho);
        printf("🟢 Pacote recebido %u (%zd/%llu)\n", ler_sequencia(pack), bytes_recebidos, (unsigned long long)tamanho);
        envia_ack(&cliente->protocolo, ler_sequencia(pack));
    }
    fclose(arquivo);


    // Se estiver rodando como root, tenta pegar o dono real
    const char* user_name = getenv("SUDO_USER");
    if (!user_name) {
        // Não foi com sudo, usa o UID atual mesmo
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            chown(patch, pw->pw_uid, pw->pw_gid);
        }
    } else {
        // Foi com sudo, pega informações do usuário real
        struct passwd* pw = getpwnam(user_name);
        if (pw) {
            chown(patch, pw->pw_uid, pw->pw_gid);
        }
    }
    visualizar_tesouro(nome_tesouro, patch, tipo);

    return 0;
}


// Exibe o conteúdo textual de um tesouro no terminal
// Lê o arquivo linha por linha e aguarda ENTER ao final
void visualizar_texto(const char *caminho_arquivo) 
{
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

    printf("\n\nAperte ENTER para continuar\n");
    getchar();

}


// Abre uma imagem do tesouro utilizando o visualizador padrão do sistema
// Aguarda o usuário pressionar ENTER após a visualização
void visualizar_imagem(const char *caminho) 
{
    // Tenta vários visualizadores possíveis
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdg-open %s || display %s || eog %s &", caminho, caminho, caminho);
    system(cmd);

    printf("Aperte ENTER para continuar\n");
    getchar();
}


// Abre um vídeo do tesouro utilizando o visualizador ou player disponível
// Aguarda o usuário pressionar ENTER após a abertura do vídeo
void visualizar_video(const char *caminho) 
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "xdg-open \"%s\" > /dev/null 2>&1 || mpv \"%s\" --quiet > /dev/null 2>&1 || vlc \"%s\" --play-and-exit > /dev/null 2>&1 &",
        caminho, caminho, caminho);
    system(cmd);

    printf("Aperte ENTER para continuar\n");
    getchar();
}


// Exibe o conteúdo do tesouro recebido, conforme o tipo identificado
// Mostra vídeo, imagem ou texto e imprime o nome do tesouro
void visualizar_tesouro(const char* nome_tesouro, const char* caminho_arquivo, mensagem_type tipo)
{
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
const char* converter_direcao(mensagem_type tipo) 
{
    switch (tipo) {
        case MSG_MOVE_CIMA: return "CIMA";
        case MSG_MOVE_BAIXO: return "BAIXO";
        case MSG_MOVE_ESQUERDA: return "ESQUERDA";
        case MSG_MOVE_DIREITA: return "DIREITA";
        default: return "DESCONHECIDO";
    }
}
