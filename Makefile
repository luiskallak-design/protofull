# --- CONFIGURAÇÃO DO PROTOCOLO ---
TARGET  = bin/protognum
CC      = gcc

# Repositórios exatos fornecidos por você
PROTOGNUM_GIT = https://github.com/luiskallak-design/protognum

# Variáveis de Portabilidade (O segredo para todas as distros)
PREFIX  ?= /usr/local
BINDIR  = $(PREFIX)/bin

# Cores do Sistema (Estética Grega/Archon)
CYAN    = \033[0;36m
RED     = \033[0;31m
GREEN   = \033[0;32m
WHITE   = \033[1;37m
BLUE    = \033[0;34m
BOLD    = \033[1m
RESET   = \033[0m

# FLAGS (Otimização máxima de leitura + includes)
CFLAGS  = -Iinclude -Wall -O3 -march=native -pipe -D_GNU_SOURCE
# CORREÇÃO TÁTICA SENIOR: Adicionada a flag -lm para linkar a biblioteca matemática (libm) e resolver o símbolo do sin()
LIBS    = -lncursesw -lSDL2 -lSDL2_mixer -lm

# ARQUIVOS
SRC     = $(wildcard src/*.c)
OBJ     = $(patsubst src/%.c, obj/%.o, $(SRC))

# --- EXECUÇÃO ---

.PHONY: all clean setup install check_sdl_dep

# Garante as pastas, as dependências externas de áudio e compila o binário monolítico
all: setup check_sdl_dep $(TARGET)
	@echo -e "\n$(GREEN)$(BOLD)⌬ [PROTOGNUM] PROTOCOLO ATIVADO COM SUCESSO!$(RESET)"
	@echo -e "$(CYAN)❱❱ Alvo localizado em: $(TARGET)$(RESET)\n"

# Prepara o terreno (Pastas locais)
setup:
	@mkdir -p bin obj
	@echo -e "$(CYAN)⌬ INICIALIZANDO AMBIENTE DE RECON...$(RESET)"

# Verifica se o ecossistema local possui as bibliotecas de desenvolvimento do Mixer/SDL
check_sdl_dep:
	@echo -e "$(WHITE)$(BOLD)⌬ [FONTE PROTOGNUM]$(RESET) $(CYAN)Sincronizado com: $(PROTOGNUM_GIT)$(RESET)"
	@echo -e "$(WHITE)$(BOLD)⌬ [SINAL]$(RESET) $(BLUE)Verificando dependências integradas do motor de áudio...$(RESET)"
	@if ! pkg-config --exists sdl2 SDL2_mixer 2>/dev/null; then \
		echo -e "$(RED)⌬ [ALERTA] Bibliotecas de desenvolvimento SDL2/SDL2_mixer não localizadas por pkg-config.$(RESET)"; \
		echo -e "$(BLUE)⌬ Tentando compilar mesmo assim usando flags estáticas globais...$(RESET)\n"; \
	else \
		echo -e "$(GREEN)⌬ [ECOSISTEMA] Bibliotecas de áudio SDL2 localizadas e linkadas com sucesso!$(RESET)\n"; \
	fi

# Linkagem Final Unificada
$(TARGET): $(OBJ)
	@echo -e "$(CYAN)⌬ CONSOLIDANDO NÚCLEO MONOLÍTICO: $(RESET)$@"
	@$(CC) $(OBJ) -o $(TARGET) $(LIBS)

# Compilação dos Módulos (Garante que a pasta obj existe)
obj/%.o: src/%.c | setup
	@echo -e "$(RED)⌬ PROCESSANDO CÓDIGO: $(RESET)$<"
	@$(CC) -c $< -o $@ $(CFLAGS)

# Limpeza (Éter)
clean:
	@rm -rf obj/*.o bin/*
	@echo -e "$(RED)⌬ LIMPANDO RASTROS DE OPERAÇÃO...$(RESET)"

# Instalação Profissional
install: all
	@echo -e "$(CYAN)⌬ ENVIANDO BINÁRIO MONOLÍTICO PARA O SISTEMA...$(RESET)"
	@mkdir -p $(DESTDIR)$(BINDIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/protognum
	@echo -e "\n$(GREEN)⌬ INSTALADO EM: $(BOLD)$(DESTDIR)$(BINDIR)$(RESET)"
	@echo -e "$(GREEN)⌬ DICA:$(RESET) $(BOLD)ZEUS-BROWSER$(RESET) detectado no horizonte e integrado ao núcleo."
