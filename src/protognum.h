#ifndef PROTOGNUM_H
#define PROTOGNUM_H

#define MAX_ITEMS 512

typedef enum {
    MODO_FILE,
    MODO_LAUNCHER,
    MODO_ZEUS
} PaneMode;

typedef struct {
    char name[512]; 
    int is_dir;
} FileItem;

// Prototipação dos Motores Táticos
void init_colors(void);
void alternar_cores(void);
void alternar_cores_player(void); 
void zeus_loading_anim(int y, int x);
int check_ext(const char *name, const char *ext);
int list_files(const char *path, FileItem *items);
int scan_applications(FileItem *items);
// Prototipação do Modo Stealth
void activar_modo_stealth(void);


// 🚀 RESOLVIDO: Declara o monitor de hardware para o main.c poder enxergar!
void draw_storage_info(int y, int x_start);

void draw_interface(int sel_l, int count_l, FileItem *items_l, 
                    int sel_c, int count_c, FileItem *items_c, 
                    int active_pane, const char *cmd_ptr);

void launch_nano_fork(const char *filepath);

#endif
