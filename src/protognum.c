#include "protognum.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ncurses.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/statvfs.h>

#define COLOR_ORANGE 8
extern int esquema_atual;
extern int esquema_player;

// ==========================================
// --- 1. MOTOR DE CORES UNIFICADO ----------
// ==========================================
void init_colors() {
    start_color();
    use_default_colors(); 
    
    if (has_colors() && can_change_color()) {
        init_color(COLOR_ORANGE, 1000, 500, 0); 
    } else {
        #undef COLOR_ORANGE
        #define COLOR_ORANGE COLOR_YELLOW
    }

    init_pair(3, COLOR_CYAN, -1);   // ZEUS
    init_pair(5, COLOR_RED, -1);    // ALERTA

    // 🚀 RESOLVIDO: Trocamos o Amarelo por Verde Tático para eliminar qualquer repetição com o Laranja
    switch (esquema_atual) {
        case 0: // TEMA 1: Laranja Tático (Archon Padrão)
            init_pair(1, COLOR_ORANGE, -1); 
            init_pair(2, COLOR_WHITE, -1); 
            init_pair(4, COLOR_YELLOW, -1);
            break;
               case 1: // TEMA 2: Terminal Verde Neon Sinistrasso
            init_pair(1, COLOR_GREEN, -1); 
            init_pair(2, COLOR_GREEN, -1); // Deixa o texto sem o bloco preto, livre para brilhar
            init_pair(4, COLOR_CYAN, -1);
            break;

        case 2: // TEMA 3: Vermelho Alerta (Modo de Combate)
            init_pair(1, COLOR_RED, -1); 
            init_pair(2, COLOR_WHITE, -1); 
            init_pair(4, COLOR_YELLOW, -1);
            break;
        case 3: // TEMA 4: Neon Abyss (Ciano e Magenta - O sumido que perdeu a vergonha!)
        default:
            init_pair(1, COLOR_CYAN, -1); 
            init_pair(2, COLOR_WHITE, -1); 
            init_pair(4, COLOR_MAGENTA, -1);   
            break;
    }


    // Cores do Archonplayer isoladas em canais superiores para evitar conflitos
    switch (esquema_player) {
        case 0: // OUTPOST
            init_pair(11, COLOR_GREEN, -1);  init_pair(12, COLOR_RED, -1);    init_pair(13, COLOR_YELLOW, -1);
            break;
        case 1: // HAZARD
            init_pair(11, COLOR_YELLOW, -1); init_pair(12, COLOR_BLUE, -1);   init_pair(13, COLOR_GREEN, -1);
            break;
        case 2: // OVERWATCH
            init_pair(11, COLOR_CYAN, -1);   init_pair(12, COLOR_YELLOW, -1); init_pair(13, COLOR_GREEN, -1);
            break;
        case 3: // NEON PURPLE
        default:
            init_pair(11, COLOR_MAGENTA, -1);init_pair(12, COLOR_CYAN, -1);   init_pair(13, COLOR_YELLOW, -1);
            break;
    }
}

void alternar_cores() {
    esquema_atual = (esquema_atual + 1) % 4;
    init_colors();
}


// ==========================================
// --- 2. ANIMAÇÃO ZEUS ---------------------
// ==========================================
void zeus_loading_anim(int y, int x) {
    char *frames[] = { "[        ]", "[=       ]", "[==      ]", "[===     ]", 
                       "[====    ]", "[=====   ]", "[======  ]", "[======= ]", 
                       "[========]" };
    attron(COLOR_PAIR(3) | A_BOLD);
    for (int i = 0; i < 9; i++) {
        mvprintw(y, x, " ⚙ ZEUS_SYNC: %s ", frames[i]);
        refresh();
        usleep(35000); 
    }
    attroff(COLOR_PAIR(3) | A_BOLD);
}

// Utilitário rápido de checagem de extensão
int check_ext(const char *name, const char *ext) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    return (strcasecmp(dot, ext) == 0);
}

// ==========================================
// --- 3. MOTOR DE LISTAGEM (ARQ. OCULTOS) ---
// ==========================================
int list_files(const char *path, FileItem *items) {
    DIR *dir = opendir(path);
    if (!dir) return 0;
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < MAX_ITEMS) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        strncpy(items[count].name, entry->d_name, 255);
        items[count].name[255] = '\0'; 
        items[count].is_dir = (entry->d_type == DT_DIR);
        count++;
    }
    closedir(dir);
    return count;
}

// ==========================================
// --- 4. LANÇADOR DE APLICATIVOS -----------
// ==========================================
int scan_applications(FileItem *items) {
    int count = 0;
    DIR *dir;
    struct dirent *entry;
    char clean_name[256];

    dir = opendir("/usr/local/bin");
    if (dir) {
        while ((entry = readdir(dir)) != NULL && count < MAX_ITEMS) {
            if (entry->d_name[0] == '.') continue;
            
            if (strcmp(entry->d_name, "nano") == 0 || strcmp(entry->d_name, "htop") == 0 || 
                strcmp(entry->d_name, "vim") == 0 || strcmp(entry->d_name, "neofetch") == 0) {
                snprintf(items[count].name, 256, "⚙ %.240s", entry->d_name);
                items[count].is_dir = 1; 
            } else {
                snprintf(items[count].name, 256, "⌬ %.240s", entry->d_name);
                items[count].is_dir = 0;
            }
            count++;
        }
        closedir(dir);
    }

    dir = opendir("/usr/share/applications");
    if (dir) {
        while ((entry = readdir(dir)) != NULL && count < MAX_ITEMS) {
            if (strstr(entry->d_name, ".desktop")) {
                snprintf(clean_name, sizeof(clean_name), "%.250s", entry->d_name);
                char *dot = strstr(clean_name, ".desktop");
                if (dot) *dot = '\0';
                
                snprintf(items[count].name, 256, "⌬ %.240s", clean_name);
                items[count].is_dir = 0;
                count++;
            }
        }
        closedir(dir);
    }

    char *home = getenv("HOME");
    if (home) {
        char rota_local[512];
        snprintf(rota_local, sizeof(rota_local), "%s/.local/share/applications", home);
        dir = opendir(rota_local);
        if (dir) {
            while ((entry = readdir(dir)) != NULL && count < MAX_ITEMS) {
                if (strstr(entry->d_name, ".desktop")) {
                    snprintf(clean_name, sizeof(clean_name), "%.250s", entry->d_name);
                    char *dot = strstr(clean_name, ".desktop");
                    if (dot) *dot = '\0';
                    
                    snprintf(items[count].name, 256, "⌬ %.240s", clean_name);
                    items[count].is_dir = 0;
                    count++;
                }
            }
            closedir(dir);
        }
    }
    return count;
}

// ==========================================
// --- 5. MONITOR DE HARDWARE E CRONOS ------
// ==========================================
void draw_storage_info(int y, int x_start) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;
    
    int col1_w = max_x * 0.25;
    int col2_w = max_x * 0.35;
    int col3_x = col1_w + col2_w + 1;

    // Devolve o título PREVIEW_NODE cravado no início da terceira coluna
    attron(COLOR_PAIR(1));
    mvprintw(1, col3_x + 2, " PREVIEW_NODE ");
    attroff(COLOR_PAIR(1));

    // ─── MÓDULO CRONOS: RELÓGIO E DATA (NOVO) ───
    #include <time.h>
    time_t rawtime;
    struct tm *timeinfo;
    char str_relogio[16];
    char str_data[16];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Formata o relógio (Hora:Minuto:Segundo) e a Data (Dia/Mês/Ano)
    strftime(str_relogio, sizeof(str_relogio), "%H:%M:%S", timeinfo);
    strftime(str_data, sizeof(str_data), "%d/%m/%y", timeinfo);

    // Posicionamento estrito colado na esquerda do HD para não amassar o layout
    // Empurramos o bloco todo para acomodar o relógio com folga
    int pos_x = max_x - 88; 

    // Imprime a Data e o Relógio piscando em Neon no Topo
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(y, pos_x, " [%s ❱ %s] ", str_data, str_relogio);
    attroff(COLOR_PAIR(3) | A_BOLD);

    // Ajusta o avanço das próximas métricas (pos_x + 24)
    int hardware_x = pos_x + 26;

    // ─── 1. MONITOR DE ESPAÇO EM DISCO REAL (GIGABYTES) ───
    struct statvfs df;
    char *home = getenv("HOME");
    if (!home) home = "/";

    if (statvfs(home, &df) == 0) {
        unsigned long long bloco = df.f_frsize;
        double total = (double)(df.f_blocks * bloco) / (1024 * 1024 * 1024);
        double livre = (double)(df.f_bavail * bloco) / (1024 * 1024 * 1024);
        double usado = total - livre;
        int percentual = (int)((usado / total) * 100);

        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(y, hardware_x, " DISK: %.0fG/%.0fG (%d%%) ", usado, total, percentual);
        attroff(COLOR_PAIR(1) | A_BOLD);
    }

    // ─── 2. MONITOR DE TIPO DE HARDWARE (SSD / NVMe) ───
    attron(COLOR_PAIR(5) | A_BOLD);
    if (access("/sys/block/nvme0n1", F_OK) == 0) {
        mvprintw(y, hardware_x + 24, " [NVMe] ");
    } else if (access("/sys/block/sda/queue/rotational", F_OK) == 0) {
        FILE *rot = fopen("/sys/block/sda/queue/rotational", "r");
        if (rot) {
            int is_rotational = 1;
            if (fscanf(rot, "%d", &is_rotational) == 1 && is_rotational == 0) {
                mvprintw(y, hardware_x + 24, " [SSD] ");
            } else {
                mvprintw(y, hardware_x + 24, " [HDD] ");
            }
            fclose(rot);
        }
    }
    attroff(COLOR_PAIR(5) | A_BOLD);

    // ─── 3. MONITOR DE MEMÓRIA RAM ───
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        long total_mem = 0, available_mem = 0;
        char linha[256];
        while (fgets(linha, sizeof(linha), meminfo)) {
            if (sscanf(linha, "MemTotal: %ld kB", &total_mem) == 1) continue;
            if (sscanf(linha, "MemAvailable: %ld kB", &available_mem) == 1) break;
        }
        fclose(meminfo);

        if (total_mem > 0) {
            long usada_mem = total_mem - available_mem;
            int pct_ram = (int)((usada_mem * 100) / total_mem);
            
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(y, hardware_x + 34, " RAM: %d%% ", pct_ram);
            attroff(COLOR_PAIR(4) | A_BOLD);
        }
    }

    // ─── 4. MONITOR DE PROCESSADOR (CPU) ESTABILIZADO ───
    static int ticks = 0;
    static int ultimo_pct_cpu = 0;
    ticks++;

    if (ticks >= 15 || ultimo_pct_cpu == 0) {
        ticks = 0;
        FILE *stat = fopen("/proc/stat", "r");
        if (stat) {
            unsigned long long user, nice, system, idle;
            char cpu_label[32];
            if (fscanf(stat, "%s %llu %llu %llu %llu", cpu_label, &user, &nice, &system, &idle) == 5) {
                static unsigned long long prev_user = 0, prev_nice = 0, prev_system = 0, prev_idle = 0;
                unsigned long long total = user + nice + system + idle;
                unsigned long long prev_total = prev_user + prev_nice + prev_system + prev_idle;
                
                if (total > prev_total) {
                    unsigned long long diff_total = total - prev_total;
                    unsigned long long diff_idle = idle - prev_idle;
                    ultimo_pct_cpu = (int)((diff_total - diff_idle) * 100 / diff_total);
                }
                prev_user = user; prev_nice = nice; prev_system = system; prev_idle = idle;
            }
            fclose(stat);
        }
    }

    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(y, hardware_x + 46, " CPU: %d%% ", ultimo_pct_cpu);
    attroff(COLOR_PAIR(3) | A_BOLD);
}


// ==========================================
// --- 6. INTERFACE EM 3 COLUNAS MILLER -----
// ==========================================
void draw_interface(int sel_l, int count_l, FileItem *items_l, 
                    int sel_r, int count_r, FileItem *items_r, 
                    int active_pane, const char *cmd_ptr) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int col1_w = max_x * 0.25;
    int col2_w = max_x * 0.35;

    attron(COLOR_PAIR(1));
    box(stdscr, 0, 0); 
    mvhline(2, 0, ACS_HLINE, max_x); 
    
    mvvline(3, col1_w, ACS_VLINE, max_y - 4); 
    mvvline(3, col1_w + col2_w, ACS_VLINE, max_y - 4); 
    
         // 🚀 RESOLVIDO: Títulos unificados em sequência com barras e cor Ciano Sinistrasso
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(1, 2, " LEFT_NODE / CENTER_NODE / PREVIEW_NODE ");
    attroff(COLOR_PAIR(3) | A_BOLD);


    
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(0, (max_x / 2) - 8, " ⌬ PROTOGNUM ⌬ ");
    attroff(A_BOLD | COLOR_PAIR(3));

    draw_storage_info(1, max_x - 38);

    // 🚀 RESOLVIDO: Injeção do par Stealth e otimização de espaço para não trombar com o F10
    int pos = 1;
    char *btns[] = {
        "F1", "USB",   "F2", "HOME",   "F3", "APP",    "F4", "NANO", 
        "F5", "COR",   "F6", "PLAY",   "F7", "MIX",    "F8", "CAM", 
        "F9", "NET",   "G",  "ZEUS",   "-",  "V-",     "+",  "V+",
        "S",  "CAMUF"
    };

    // O loop calcula automaticamente o tamanho novo de 26 elementos (13 botões)
    int total_elementos = (int)(sizeof(btns) / sizeof(btns[0]));
    for(int i = 0; i < total_elementos; i += 2) { 
        attron(COLOR_PAIR(4) | A_BOLD); 
        mvprintw(max_y - 1, pos, " %s ", btns[i]);
        pos += strlen(btns[i]) + 2;
        attroff(COLOR_PAIR(4) | A_BOLD);
        
        attron(COLOR_PAIR(1) | A_REVERSE); 
        mvprintw(max_y - 1, pos, " %s ", btns[i+1]);
        pos += strlen(btns[i+1]) + 2; 
        attroff(COLOR_PAIR(1) | A_REVERSE);
    }

    
    attron(COLOR_PAIR(4) | A_BOLD);
    mvprintw(max_y - 2, 1, " ⌬ ARCHON: "); 
    attroff(COLOR_PAIR(4) | A_BOLD);
    printw("%s", cmd_ptr);
}
// ====================================================================
// 🚀 MÓDULO STEALTH: CAMUFLAGEM CIBERNÉTICA MATRÍCULAL (WAYNE/STARK)
// ====================================================================
void activar_modo_stealth(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;

    // Aloca arrays dinâmicos para controlar a queda das colunas Matrix
    int *intensidade = malloc(max_x * sizeof(int));
    int *pos_queda = malloc(max_x * sizeof(int));
    
    for (int i = 0; i < max_x; i++) {
        pos_queda[i] = -(rand() % 80); // Altura de partida aleatória
        intensidade[i] = 10 + (rand() % 50); // Comprimento da queda da boiada
    }

    // Caracteres táticos misturados (Código de segurança industrial Stark)
    const char *chars_taticos = "01$#@&%*X⌬⚙/\\?-+=<>![]";
    int num_chars = strlen(chars_taticos);

    nodelay(stdscr, TRUE); // Torna o getch não-bloqueante para rodar solto a 16ms
    curs_set(0);           // Some com o cursor para máxima camuflagem

    while (1) {
        // Se bater qualquer tecla, o protocolo desativa na marra e sai
        int ch_stop = getch();
        if (ch_stop != ERR) break;

        // Desenha a cascata digital direto no metal
        for (int x = 0; x < max_x; x++) {
            // Desenha a cabeça da linha com brilho máximo (Neon Sinistrasso)
            if (pos_queda[x] >= 0 && pos_queda[x] < max_y) {
                attron(COLOR_PAIR(1) | A_BOLD); // Usa o seu Verde Sinistrasso ativo
                mvaddch(pos_queda[x], x, chars_taticos[rand() % num_chars]);
                attroff(COLOR_PAIR(1) | A_BOLD);
            }
            
            // Desenha a cauda apagando o rastro antigo
            int cauda = pos_queda[x] - intensidade[x];
            if (cauda >= 0 && cauda < max_y) {
                mvaddch(cauda, x, ' ');
            }

            // Avança a queda da linha
            pos_queda[x]++;
            if (pos_queda[x] - intensidade[x] > max_y) {
                pos_queda[x] = 0;
                intensidade[x] = 10 + (rand() % 50);
            }
        }

        // Marca d'água de segurança Wayne/Stark piscando discretamente no centro
        attron(COLOR_PAIR(5) | A_BOLD | A_BLINK); // Vermelho Alerta piscante
        mvprintw(max_y / 2, (max_x / 2) - 16, " 🛑 [STEALTH PROTOCOL ACTIVE] ");
        attroff(COLOR_PAIR(5) | A_BOLD | A_BLINK);

        refresh();
        usleep(30000); // Sincronia de taxa de atualização limpa para efeito fluido
    }

     // Libera a memória para não dar vazamento no metal (0 caca)
    free(intensidade);
    free(pos_queda);
    
    // 🚀 RESOLVIDO: Restaura o timeout de 16ms interativo para destravar o espectro visual!
    nodelay(stdscr, FALSE); 
    timeout(16); 
}
