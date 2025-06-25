# Makefile para Protocolo Cliente-Servidor

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -g
LDFLAGS = 

SERVIDOR = servidor
CLIENTE = cliente

PROTOCOL_SRC = protocolo.c
SERVIDOR_SRC = servidor.c
CLIENTE_SRC = cliente.c

PROTOCOL_OBJ = protocolo.o
SERVIDOR_OBJ = servidor.o
CLIENTE_OBJ = cliente.o

HEADERS = protocol.h servidor.h cliente.h

# Compilar tudo
all: $(SERVIDOR) $(CLIENTE)
	@echo ""

# Compilar servidor
$(SERVIDOR): $(SERVIDOR_OBJ) $(PROTOCOL_OBJ)
	@echo "===== Compilando servidor ====="
	$(CC) $(SERVIDOR_OBJ) $(PROTOCOL_OBJ) -o $(SERVIDOR) $(LDFLAGS)
	@echo "===== Servidor compilado sem erros ====="
	@echo ""

# Compilar cliente
$(CLIENTE): $(CLIENTE_OBJ) $(PROTOCOL_OBJ)
	@echo "===== Compilando cliente ====="
	$(CC) $(CLIENTE_OBJ) $(PROTOCOL_OBJ) -o $(CLIENTE) $(LDFLAGS)
	@echo "===== Cliente compilado sem erros ====="

# Compilar objetos
%.o: %.c $(HEADERS)
	@echo "Compilando $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza de arquivos objeto e executáveis
clean:
	@echo "===== Remoção de arquivos objetos ====="
	rm -f *.o $(SERVIDOR) $(CLIENTE)
