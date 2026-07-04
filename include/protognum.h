#ifndef PROTOGNUM_H
#define PROTOGNUM_H

#define MAX_ITEMS 512

typedef enum {
    MODO_FILE,
    MODO_ZEUS,
    MODO_LAUNCHER
} PaneMode;

typedef struct {
    char name[256];
    int is_dir;
} FileItem;

// Declarações das funções globais
void init_colors(void);
void alternar_cores(void);
void zeus_loading_anim(int y, int x);
int list_files(const char *path, FileItem *items);
void draw_interface(int sel_l, int count_l, FileItem *items_l, 
                    int sel_r, int count_r, FileItem *items_r, 
                    int active_pane, const char *cmd_ptr);
void execute_tactic_prompt(int max_y);
void execute_zeus_action(const char *query, int is_video);
int scan_applications(FileItem *items);

#endif
