# Makefile para Protocolo Cliente-Servidor

# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -g
LDFLAGS =

# Nomes dos executáveis
SERVIDOR = servidor
CLIENTE = cliente

# Arquivos fonte
PROTOCOL_SRC = protocolo.c
SERVIDOR_SRC = servidor.c
CLIENTE_SRC = cliente.c
RAWSOCKET_SRC = rawSocket.c

# Arquivos objeto
PROTOCOL_OBJ = protocolo.o
SERVIDOR_OBJ = servidor.o
CLIENTE_OBJ = cliente.o
RAWSOCKET_OBJ = rawSocket.o

# Arquivos de cabeçalho
HEADERS = protocol.h rawSocket.h

# Diretórios
ARQUIVOS_DIR = objetos
DOWNLOADS_DIR = transferidos

# Regra padrão
all: $(SERVIDOR) $(CLIENTE) setup

# Compilar servidor
$(SERVIDOR): $(SERVIDOR_OBJ) $(PROTOCOL_OBJ) $(RAWSOCKET_OBJ)
	@echo "=== Configurando servidor ==="
	$(CC) $(SERVIDOR_OBJ) $(PROTOCOL_OBJ) $(RAWSOCKET_OBJ) -o $(SERVIDOR) $(LDFLAGS)
	@echo "=== Servidor compilado sem serros ==="

# Compilar cliente
$(CLIENTE): $(CLIENTE_OBJ) $(PROTOCOL_OBJ) $(RAWSOCKET_OBJ)
	@echo "=== Configurando cliente ==="
	$(CC) $(CLIENTE_OBJ) $(PROTOCOL_OBJ) $(RAWSOCKET_OBJ) -o $(CLIENTE) $(LDFLAGS)
	@echo "=== Cliente compilado sem erros ==="

# Compilar arquivos objeto
%.o: %.c $(HEADERS)
	@echo "Compilando $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Executar servidor
run-servidor: $(SERVIDOR) setup
	@echo "Iniciando servidor..."
	./$(SERVIDOR)

# Executar cliente
run-cliente: $(CLIENTE) setup
	@if [ -z "$(IP)" ]; then \
		echo "Erro: Especifique o IP com IP=<endereço>"; \
		echo "Exemplo: make run-cliente IP=192.168.1.100"; \
		exit 1; \
	fi
	@echo "Iniciando cliente (conectando em $(IP))..."
	./$(CLIENTE) $(IP)

# Criar diretórios necessários
setup:
	@mkdir -p $(ARQUIVOS_DIR) $(DOWNLOADS_DIR)

# Limpeza
clean:
	@echo "=== Removendo arquivos objeto ==="
	rm -f *.o $(SERVIDOR) $(CLIENTE)
