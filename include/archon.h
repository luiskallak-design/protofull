#ifndef ARCHON_H
#define ARCHON_H

#include <ncurses.h>
#include <stdbool.h>

// Estados do Player
typedef enum {
    STOPPED,
    PLAYING,
    PAUSED
} PlayerState;

// Estrutura para os itens da lista (Arquivos de música)
typedef struct {
    char name[256];
    bool is_dir;
} ArchonFile;

// Estrutura Principal do Archon (DNA Estilo Cmus)
typedef struct {
    ArchonFile items[200]; // Suporta até 200 arquivos na pasta
    int total_items;
    int selection;         // Onde o cursor está na lista
    int current_index;     // Qual música está tocando agora
    PlayerState state;
    int volume;
} ArchonCore;

// Funções de Operação
void init_colors();
void draw_interface(ArchonCore *core);
int scan_directory(ArchonCore *core, const char *path);

#endif
