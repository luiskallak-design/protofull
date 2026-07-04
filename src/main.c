#include "protognum.h"
#include <stdio.h>
#include <locale.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <ncurses.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <time.h>
// Controle de comandos, paginação e caminhos das colunas independentes
char cmd_buffer[1024] = "";

int offset_l = 0;
int offset_c = 0;

int esquema_atual = 0;
int esquema_player = 0;


/* ==========================================
   MOTOR DO ESPECTRO
   ========================================== */

int spectrum_data[256] = {0};
int peak_data[4096] = {0};

int frame_counter = 0;


/* ==========================================
   PLAYER
   ========================================== */

int playing_idx = -1;

char playlist_path[512] = "";
char current_playing_file[512] = "";

double current_prog = 0;

Mix_Music *global_music = NULL;


/* ==========================================
   MODOS
   ========================================== */

PaneMode modo_esquerda = MODO_FILE;
PaneMode modo_centro = MODO_FILE;

int app_mode_active = 0;


/* ==========================================
   CALLBACK DE ÁUDIO (CALIBRADO PARA ALTA OSCILAÇÃO)
   ========================================== */

void audio_callback(void *udata, Uint8 *stream, int len)
{
    if(!stream || len<=0)
        return;

    Sint16 *samples = (Sint16*)stream;

    int total_samples = len / sizeof(Sint16);
    int samples_per_band = total_samples / 256;

    if(samples_per_band < 1)
        samples_per_band = 1;

    for(int i=0;i<256;i++)
    {
        long energy = 0;
        int start = i * samples_per_band;
        int end = start + samples_per_band;

        if(end > total_samples)
            end = total_samples;

        /* energia média */
        for(int j=start;j<end;j++)
        {
            int s = abs(samples[j]);
            energy += s;
        }

        energy /= (end-start);

                  /* curva logarítmica otimizada */
        int val = (int)(log10f(energy + 1.0f) * 35.0f);

        /* ==========================================================
           🔊 INTEGRAÇÃO DE VOLUME DINÂMICO (PROTOGNUM SYNC)
           ========================================================== */
        // Captura o volume atual da SDL2 (0 a 128)
        int volume_atual = Mix_VolumeMusic(-1);
        if (volume_atual < 0) volume_atual = 0;
        
        // Multiplica o valor da barra pela proporção real do volume.
        // Se o volume for 0, o espectro zera. Se for 128 (máximo), mantém 100% da força.
        val = (val * volume_atual) / 128;

      

                /* 
           reforço de graves real (IMPACTO MÁXIMO)
           💎 CALIBRAÇÃO AGRESSIVA: Elevado de 1.35f para 1.75f. 
           Isso vai fazer as frequências de bumbo e subwoofers (i < 30) darem um soco na tela.
        */
        if(i < 30)
        {
            val = (int)(val * 1.75f);
        }

        /* reforço médio/vocal (suavizado) */
        if(i > 60 && i < 130)
        {
            val = (int)(val * 1.20f);
        }

        if(val > 255) val = 255;
        if(val < 0)   val = 0;

        /* ==========================================================
           🔥 SINAL ULTRA-PULSANTE: QUEDA LIVRE + ATAQUE CHICOTE
           ========================================================== */
        if(val > spectrum_data[i])
        {
            // 🚀 ATAQUE CHICOTE BRUTO: Remove a suavização (*1 + *7). 
            // Agora a barra absorve 100% da energia imediatamente e salta pro topo no mesmo frame.
            spectrum_data[i] = val; 
        }
        else
        {
            // 🏎️ QUEDA LIVRE ACELERADA: Alterado o peso de (4+4) para (2*anterior + 6*val).
            // Na ausência de batida, a barra despenca feito uma bigorna. 
            // Isso cria o maior contraste elástico possível (efeito mola violento).
            spectrum_data[i] = (spectrum_data[i] * 2 + val * 6) / 8;
        }
    }
}



void spawn_external_tactic(const char *cmd, const char *arg, int open_in_terminal) {
    pid_t pid1 = fork();
    if (pid1 == 0) {
        setsid();
        pid_t pid2 = fork();
        if (pid2 == 0) {
            int dn = open("/dev/null", O_RDWR);
            dup2(dn, 0); dup2(dn, 1); dup2(dn, 2); 
            close(dn);
            sync();
            if (open_in_terminal) {
                execlp("qterminal", "qterminal", "-e", cmd, arg, (char *)NULL);
            } else {
                execlp(cmd, cmd, arg, (char *)NULL);
            }
            _exit(1); 
        }
        _exit(0); 
    }
    waitpid(pid1, NULL, 0);
}

                // 1. Atualize a assinatura para receber 'const char *zeus_buf' no final
void draw_preview_panel(int max_y, int max_x, const char *dir_pai, FileItem *focused_item, int zeus_active, const char *zeus_buf) {
    int col1_w = max_x * 0.30;
    int col2_w = max_x * 0.35;
    int col3_x = col1_w + col2_w + 1;
    int col3_w = max_x - col3_x - 1;
    int horizon_y = max_y - 6;

    // Limpa a região inteira do Preview
    for (int row = 3; row < horizon_y - 1; row++) {
        mvhline(row, col3_x, ' ', col3_w);
    }

    /* ====================================================================
       🛰️ COMPONENTE VISUAL FIXO: ZEUS_RADAR / SNIPER (REPOSITÓRIO SUPERIOR)
       ==================================================================== */
    int target_y = horizon_y - 10; 

    attron(COLOR_PAIR(1) | A_DIM);
    mvhline(target_y - 2, col3_x + 1, ACS_HLINE, col3_w - 2);
    attroff(COLOR_PAIR(1) | A_DIM);

    attron(COLOR_PAIR(4) | A_BOLD);
    mvprintw(target_y - 1, col3_x + 2, " SITES ❱ [1:Google | 2:Duck | 3:YT] ");
    attroff(COLOR_PAIR(4) | A_BOLD);

    // Prompt de Busca fixo (Agora lendo com segurança a variável 'zeus_buf' repassada)
    if (zeus_active) {
        attron(COLOR_PAIR(3) | A_BOLD);
        // 👈 FIX: Agora usa o parâmetro local 'zeus_buf' recebido da main
        mvprintw(target_y, col3_x + 2, "❱ ZEUS_RADAR ❯ %s_", zeus_buf);
        attroff(COLOR_PAIR(3) | A_BOLD);
    } else {
        attron(COLOR_PAIR(3) | A_DIM);
        mvprintw(target_y, col3_x + 2, "❱ [G] ACESSAR ZEUS_RADAR ❯ [ESPERA]");
        attroff(COLOR_PAIR(3) | A_DIM);
    }

    int limite_texto = target_y - 3;
    
    // ... (O restante da sua rotina de leitura de arquivos com fgets continua igual abaixo daqui)


    if (!focused_item) {
        attron(COLOR_PAIR(1) | A_DIM);
        mvprintw(5, col3_x + 2, "[ VAZIO ]");
        attroff(COLOR_PAIR(1) | A_DIM);
        return;
    }

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(3, col3_x, "📂 PREVIEW: %.20s", focused_item->name);
    mvhline(4, col3_x, ACS_HLINE, col3_w);
    attroff(COLOR_PAIR(2) | A_BOLD);

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_pai, focused_item->name);

    if (check_ext(focused_item->name, ".txt") || check_ext(focused_item->name, ".sh") || 
        check_ext(focused_item->name, ".conf") || check_ext(focused_item->name, ".h") || 
        check_ext(focused_item->name, ".c") || check_ext(focused_item->name, ".toml") ||
        check_ext(focused_item->name, ".json") || check_ext(focused_item->name, ".md")) {
        
        FILE *f = fopen(full_path, "r");
        if (f) {
            char linha[256];
            int r_idx = 5;
            attron(COLOR_PAIR(2));
            // Substituído horizon_y - 2 por limite_texto para proteger a caixinha do radar
            while (fgets(linha, sizeof(linha), f) && r_idx < limite_texto) {
                linha[strcspn(linha, "\n")] = '\0';
                mvprintw(r_idx++, col3_x, "  %-.*s", col3_w - 2, linha);
            }
            attroff(COLOR_PAIR(2));
            fclose(f);
        }
    } else if (check_ext(focused_item->name, ".jpg") || check_ext(focused_item->name, ".png") || check_ext(focused_item->name, ".gif")) {
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(6, col3_x + 2, "⌬ IMAGEM: %.15s", focused_item->name);
        attron(COLOR_PAIR(1));
        mvprintw(8, col3_x + 2, "❯ [ENTER] EXEC SWWW/FEH");
        attroff(COLOR_PAIR(1) | COLOR_PAIR(3) | A_BOLD);
    } else {
        struct stat st;
        if (stat(full_path, &st) == 0) {
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(6, col3_x + 2, "⌬ METADADOS");
            attroff(A_BOLD);
            attron(COLOR_PAIR(1));
            mvprintw(8, col3_x + 2, "  Size: %.2f KB", (double)st.st_size / 1024.0);
            mvprintw(9, col3_x + 2, "  Perms: %o", st.st_mode & 0777);
            attroff(COLOR_PAIR(1) | COLOR_PAIR(4));
        }
    }
}



void draw_global_archonplayer(int max_y, int max_x)
{
    const int horizon_y = max_y - 6;
    const int center_screen = max_x / 2;

    // Expansão radical para ocupar todo o rodapé de ponta a ponta
    int player_start_x = 1; 
    int player_render_w = max_x - 2;

    /* ==========================
       HEADER
       ========================== */
    attron(COLOR_PAIR(1));
    mvhline(horizon_y - 6, player_start_x, ACS_HLINE, player_render_w);
    attroff(COLOR_PAIR(1));

    attron(COLOR_PAIR(13) | A_BOLD);
    mvprintw(horizon_y - 6, center_screen - 4, " PLAYER ");
    attroff(COLOR_PAIR(13) | A_BOLD);

    /* ==========================
       IDLE STATE
       ========================== */
    if(!global_music)
    {
        attron(COLOR_PAIR(11) | A_DIM);
        mvprintw(horizon_y, center_screen - 8, "[ PLAYER IDLE ]");
        attroff(COLOR_PAIR(11) | A_DIM);
        
        attron(COLOR_PAIR(11));
        mvhline(horizon_y, player_start_x, ACS_HLINE, player_render_w);
        attroff(COLOR_PAIR(11));
    }
    else
    {
        /* ==========================
           INFO DA MÚSICA
           ========================== */
        double music_pos = Mix_GetMusicPosition(global_music);
        double music_dur = Mix_MusicDuration(global_music);

        current_prog = (music_dur > 0) ? (music_pos / music_dur) : 0;
        int horizon_fill = (int)(current_prog * (player_render_w - 2));

        attron(COLOR_PAIR(12) | A_BOLD);
        mvprintw(horizon_y - 6, max_x - 24, "[ %02d:%02d ❱ %02d:%02d ]", 
                 (int)music_pos / 60, (int)music_pos % 60, 
                 (int)music_dur / 60, (int)music_dur % 60);
        attroff(COLOR_PAIR(12) | A_BOLD);

        /* ==========================
           LIMPA REGIÃO DO PLAYER (CIRÚRGICO ANTI-FLICKER)
           ========================== */
        for(int k = 1; k <= 10; k++) {
            mvhline(horizon_y - k, player_start_x, ' ', player_render_w);
            mvhline(horizon_y + k, player_start_x, ' ', player_render_w);
        }

        /* ====================================================================
           ⚡ MOTOR SPECTER TOTAL (CORREÇÃO DE PULSAÇÃO E FILTRO DE QUEDA)
           ==================================================================== */
        for (int i = player_start_x; i < max_x - 1; i++) 
        {
            // Centro absoluto do terminal para espalhar os graves a partir do meio da tela
            int distance_from_center = abs(i - center_screen); 

            // Mapeamento proporcional exato para as 256 bandas em tela cheia
            int d_idx = 255 - ((distance_from_center * 255) / center_screen);

            if (d_idx >= 256) d_idx = 255;
            if (d_idx < 0) d_idx = 0;

            // Coleta a energia calculada pelo audio_callback
            int val = spectrum_data[d_idx];
            
            if (d_idx > 0 && d_idx < 255) {
                val = (spectrum_data[d_idx-1] + spectrum_data[d_idx] + spectrum_data[d_idx+1]) / 3;
            }

            // Calibragem de ganho para evitar o paredão travado
            int h = val / 26; 
            if (h > 4) h = 4; 
            if (h == 0 && val > 16) h = 1;
 
            /* ==========================
               PEAK SYSTEM (CÁLCULO)
               ========================== */
            if (h > peak_data[i]) {
                peak_data[i] = h;         
            }
            else if (peak_data[i] > 0 && frame_counter % 2 == 0) {
                peak_data[i]--;
            }

            // Acompanha a gravidade da queda na ausência de som
            if (h < peak_data[i] && frame_counter % 2 == 0) {
                h = peak_data[i];
            }

            // 💎 DEFINIÇÃO DE VARIÁVEIS (Movidas para DENTRO do laço para sanar o erro)
            int is_passed = (i <= horizon_fill);
            int col = is_passed ? 12 : 11;

            // 1. Desenha os Filetes Verticais Dinâmicos (ACS_VLINE)
            for (int j = 1; j <= h; j++) {
                attron(COLOR_PAIR(col) | (is_passed ? A_BOLD : A_DIM));
                mvaddch(horizon_y - j, i, ACS_VLINE);
                mvaddch(horizon_y + j, i, ACS_VLINE);
                attroff(A_BOLD | A_DIM);
            }

            // 2. Renderização dos Marcadores de Pico em Diamante (ACS_DIAMOND)
            if (peak_data[i] > 0) {
                attron(COLOR_PAIR(4) | A_BOLD);
                mvaddch(horizon_y - peak_data[i], i, ACS_DIAMOND);
                mvaddch(horizon_y + peak_data[i], i, ACS_DIAMOND);
                attroff(A_BOLD | COLOR_PAIR(4));
            }

            // 3. Desenha a linha de Horizonte com o marcador de progresso 'O'
            attron(COLOR_PAIR(col) | (is_passed ? A_BOLD : A_DIM));
            mvaddch(horizon_y, i, (i == horizon_fill) ? 'O' : ACS_HLINE);
            attroff(A_BOLD | A_DIM);

        } // Fecha o loop for
    } // Fecha o bloco else do global_music
} // Fecha a função draw_global_archonplayer




void tocar_musica_idx(const char *pasta, FileItem *items, int idx, int count) {
    if (idx < 0 || idx >= count || items[idx].is_dir) return;
    if (!check_ext(items[idx].name, ".mp3")) return;

    if (global_music) { Mix_HaltMusic(); Mix_FreeMusic(global_music); }
    char fpath[1024]; 
    snprintf(fpath, sizeof(fpath), "%s/%s", pasta, items[idx].name);
    global_music = Mix_LoadMUS(fpath);
    if (global_music) {
        Mix_PlayMusic(global_music, 0);
        Mix_SetPostMix(audio_callback, NULL);
        playing_idx = idx;
        snprintf(playlist_path, sizeof(playlist_path), "%s", pasta);
        snprintf(current_playing_file, sizeof(current_playing_file), "%s", items[idx].name);
    }
}

int main()
{
    setlocale(LC_ALL, "");

    SDL_Init(SDL_INIT_AUDIO);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512);

    initscr();
    start_color();
    noecho();

    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(16);

    init_colors();

    srand(time(NULL));

    FileItem items_l[MAX_ITEMS], items_c[MAX_ITEMS];
    char path_l[512], path_c[512];

    int sel_l = 0, sel_c = 0;
    int count_l = 0, count_c = 0;
    int active_pane = 0;
    int zeus_active = 0; 
    char zeus_buffer[256] = ""; // 👈 Adicione este buffer global para a busca
    int zeus_idx = 0;            // 👈 Índice do caractere atual

      //int volume_player = 128; 

    getcwd(path_l, 512);
    strcpy(path_c, path_l);

    count_l = list_files(path_l, items_l);
    count_c = list_files(path_c, items_c);

       while(1)
    {
        frame_counter++;

        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);

        int col1_w = max_x * 0.30;
        int col2_w = max_x * 0.35;

        int max_view = max_y - 14;

        if(global_music &&
           Mix_PlayingMusic() == 0 &&
           Mix_PausedMusic() == 0)
        {
            int proximo = (playing_idx + 1) % count_l;
            int seguranca = 0;

            while(items_l[proximo].is_dir &&
                  seguranca < count_l)
            {
                proximo = (proximo + 1) % count_l;
                seguranca++;
            }

            if(!items_l[proximo].is_dir &&
               check_ext(items_l[proximo].name, ".mp3"))
            {
                if(global_music)
                {
                    Mix_HaltMusic();
                    Mix_FreeMusic(global_music);
                }

                char fpath[1024];

                snprintf(
                    fpath,
                    sizeof(fpath),
                    "%s/%s",
                    path_l,
                    items_l[proximo].name
                );

                global_music = Mix_LoadMUS(fpath);

                if(global_music)
                {
                    Mix_PlayMusic(global_music, 0);
                    Mix_SetPostMix(audio_callback, NULL);
                    playing_idx = proximo;

                    snprintf(
                        current_playing_file,
                        sizeof(current_playing_file),
                        "%s",
                        items_l[proximo].name
                    );
                }
            }
        }

        erase(); // Limpa o frame anterior

        
                 /* ==========================================
           1. ESTRUTURA BASE DA TELA EM ABAS/PAINÉIS
           ========================================== */
        attron(COLOR_PAIR(1));
        box(stdscr, 0, 0);
        mvhline(2, 0, ACS_HLINE, max_x);

        mvvline(3, col1_w, ACS_VLINE, max_y - 12);
        mvvline(3, col1_w + col2_w, ACS_VLINE, max_y - 12);
        attroff(COLOR_PAIR(1)); // Fecha o par padrão aqui para não vazar cor

        // 🚀 RESOLVIDO: Desenha os títulos em sequência com barras e injeta o Ciano Sinistrasso!
        attron(COLOR_PAIR(3) | A_BOLD);
        if (app_mode_active) {
            mvprintw(1, 2, " LEFT_NODE / APPS_LAUNCHER / PREVIEW_NODE ");
        } else {
            mvprintw(1, 2, " LEFT_NODE / CENTER_NODE / PREVIEW_NODE ");
        }
        attroff(COLOR_PAIR(3) | A_BOLD);

        /* ==========================================
           2. PAINEL DE PREVIEW (DIREITA)
           ========================================== */
        // 💎 RECONEXÃO TÁTICA: O Zeus_Radar agora é desenhado aqui dentro de forma 
        // embutida e segura. Não precisa de códigos soltos abaixo deste bloco!
        FileItem *item_focado = NULL;
        char *pasta_focada = NULL;

        if(active_pane == 0 && count_l > 0)
        {
            if(sel_l >= 0 && sel_l < count_l)
                item_focado = &items_l[sel_l];

            pasta_focada = path_l;
        }
        else if(count_c > 0)
        {
            if(sel_c >= 0 && sel_c < count_c)
                item_focado = &items_c[sel_c];

            pasta_focada = path_c;
        }

               /* ==========================
           2. PAINEL DE PREVIEW (DIREITA)
           ========================== */
        if(pasta_focada)
        {
            draw_preview_panel(
                max_y,
                max_x,
                pasta_focada,
                item_focado,
                zeus_active,
                zeus_buffer 
            );
        }


        /* ==========================================================
           ⚠️ 3. CAMADA INFERIOR: MOTORES DO PLAYER EM LARGURA TOTAL
           ========================================================= */
        draw_global_archonplayer(
            max_y,
            max_x
        );


        /* ==========================================================
           💎 4. SOBREPOSIÇÃO: DESENHO DA LISTA ESQUERDA (LEFT_NODE)
           ========================================================== */
        // Imprime por cima do espectro, garantindo leitura perfeita das fontes
        for(int i = offset_l; i < offset_l + max_view && i < count_l; i++)
        {
            int pair = (i == sel_l && active_pane == 0) ? 4 : (items_l[i].is_dir ? 1 : 2);
            int row = i - offset_l + 3;

            if(row < max_y - 12)
            {
                if(active_pane == 0 && i == sel_l)
                {
                    attron(COLOR_PAIR(pair) | A_BOLD);
                    mvprintw(row, 2, "> %-.*s", col1_w - 3, items_l[i].name);
                    attroff(COLOR_PAIR(pair) | A_BOLD);
                }
                else
                {
                    attron(COLOR_PAIR(pair));
                    mvprintw(row, 3, "%-.*s", col1_w - 3, items_l[i].name);
                    attroff(COLOR_PAIR(pair));
                }
            }
        }

        /* ==========================================================
           💎 5. SOBREPOSIÇÃO: DESENHO DA LISTA CENTRAL (CENTER_NODE)
           ========================================================== */
        // Imprime por cima do espectro no painel do meio
        for(int i = offset_c; i < offset_c + max_view && i < count_c; i++)
        {
            int pair = (i == sel_c && active_pane == 1) ? 4 : (items_c[i].is_dir ? 1 : 2);
            int row = i - offset_c + 3;

            if(row < max_y - 12)
            {
                if(active_pane == 1 && i == sel_c)
                {
                    attron(COLOR_PAIR(pair) | A_BOLD);
                    mvprintw(row, col1_w + 1, "> %-.*s", col2_w - 3, items_c[i].name);
                    attroff(COLOR_PAIR(pair) | A_BOLD);
                }
                else
                {
                    attron(COLOR_PAIR(pair));
                    mvprintw(row, col1_w + 2, "%-.*s", col2_w - 3, items_c[i].name);
                    attroff(COLOR_PAIR(pair));
                }
            }
        }

              /* ==========================================================
           💎 LISTA DO PAINEL ESQUERDO (LEFT_NODE) - MANTIDO
           ========================================================== */
        for(int i = offset_l; i < offset_l + max_view && i < count_l; i++)
        {
            int pair = (i == sel_l && active_pane == 0) ? 4 : (items_l[i].is_dir ? 1 : 2);
            int row = i - offset_l + 3;

            if(row < max_y - 12)
            {
                if(active_pane == 0 && i == sel_l)
                {
                    attron(COLOR_PAIR(pair) | A_BOLD);
                    mvprintw(row, 2, "> %-.*s", col1_w - 3, items_l[i].name);
                    attroff(COLOR_PAIR(pair) | A_BOLD);
                }
                else
                {
                    attron(COLOR_PAIR(pair));
                    mvprintw(row, 3, "%-.*s", col1_w - 3, items_l[i].name);
                    attroff(COLOR_PAIR(pair));
                }
            }
        }

        /* ==========================================================
           ⚡ RESTAURAÇÃO: LISTA DO PAINEL CENTRAL (CENTER_NODE)
           ========================================================== */
        // Este é o bloco que estava faltando e trouxe a sua coluna central de volta!
        for(int i = offset_c; i < offset_c + max_view && i < count_c; i++)
        {
            int pair = (i == sel_c && active_pane == 1) ? 4 : (items_c[i].is_dir ? 1 : 2);
            int row = i - offset_c + 3;

            if(row < max_y - 12)
            {
                if(active_pane == 1 && i == sel_c)
                {
                    attron(COLOR_PAIR(pair) | A_BOLD);
                    mvprintw(row, col1_w + 1, "> %-.*s", col2_w - 3, items_c[i].name);
                    attroff(COLOR_PAIR(pair) | A_BOLD);
                }
                else
                {
                    attron(COLOR_PAIR(pair));
                    mvprintw(row, col1_w + 2, "%-.*s", col2_w - 3, items_c[i].name);
                    attroff(COLOR_PAIR(pair));
                }
            }
        }



               /* ====================================================================
           1. NOVO RODAPÉ DE ATALHOS EM CAMADAS CONTÍNUAS
           ==================================================================== */
                   int pos = 1;
        char *btns[] = {
            "F1", "USB",   "F2", "HOME",   "F3", "APP",    "F4", "NANO", 
            "F5", "COR",   "F6", "PLAY",   "F7", "MIX",    "F8", "CAM", 
            "F9", "NET",   "G",  "ZEUS",   "-",  "V-",     "+",  "V+", 
            "S",  "CAMUF"
        };


        for(int i = 0; i < 20; i += 2) {
            attron(COLOR_PAIR(4) | A_BOLD); 
            mvprintw(max_y - 1, pos, "%s", btns[i]); 
            pos += strlen(btns[i]) + 1; 
            attroff(COLOR_PAIR(4) | A_BOLD);

            attron(COLOR_PAIR(1) | A_REVERSE); 
            mvprintw(max_y - 1, pos, "%s", btns[i+1]); 
            pos += strlen(btns[i+1]) + 2; 
            attroff(COLOR_PAIR(1) | A_REVERSE);
        }

               // 🚀 RESOLVIDO: Renderiza o F10 com fundo vermelho destacado e o EXIT na sequência
        init_pair(99, COLOR_WHITE, COLOR_RED); // Cria o par vermelho dinamicamente para o gatilho
        
        attron(COLOR_PAIR(99) | A_BOLD);
        mvprintw(max_y - 1, 96, " F10 "); // O quadrado vermelho tático
        attroff(COLOR_PAIR(99) | A_BOLD);
        
        attron(COLOR_PAIR(1));
        printw(" EXIT "); // O texto corrido logo à frente
        attroff(COLOR_PAIR(1));
        
                       // 🚀 PASSO 1: Força o motor do protognum.c a estampar a telemetria (HD, NVMe, RAM, CPU)
        draw_storage_info(1, max_x);

        // 🚀 PASSO 2: Injeção Direta do Botão Secreto STEALTH (Coluna 82) antes do F10
        attron(COLOR_PAIR(4) | A_BOLD); 
        mvprintw(max_y - 1, 82, " S ");
        attroff(COLOR_PAIR(4) | A_BOLD);
        
        attron(COLOR_PAIR(1) | A_REVERSE); 
        printw(" CAMUF ");
        attroff(COLOR_PAIR(1) | A_REVERSE);

        // 🚀 PASSO 3: Força o desenho do F10 com fundo vermelho destacado na coluna 96
        init_pair(99, COLOR_WHITE, COLOR_RED); 
        attron(COLOR_PAIR(99) | A_BOLD);
        mvprintw(max_y - 1, 96, " F10 "); 
        attroff(COLOR_PAIR(99) | A_BOLD);
        
        attron(COLOR_PAIR(1));
        printw(" EXIT "); 
        attroff(COLOR_PAIR(1));

        refresh(); 
        int ch = getch();


        // Encerramento seguro do sistema Protognum
        if (ch == KEY_F(10)) {
            if (global_music) Mix_FreeMusic(global_music);
            Mix_CloseAudio(); 
            SDL_Quit(); 
            endwin(); 
            return 0;
        }


       
        if (ch == '\t' || ch == '9') active_pane = !active_pane;

        int *c_sel = active_pane ? &sel_c : &sel_l;
        int *c_co  = active_pane ? &count_c : &count_l;
        int *c_off = active_pane ? &offset_c : &offset_l;
        char *c_path = active_pane ? path_c : path_l;
        FileItem *c_items = active_pane ? items_c : items_l;

                  /* ====================================================================
           ⚡ INTERCEPTAÇÃO CONTÍNUA DO ZEUS_RADAR (FLUXO 16ms INTERATIVO)
           ==================================================================== */
        if (!zeus_active && (ch == 'g' || ch == 'G')) {
            zeus_active = 1;
            zeus_idx = 0;
            zeus_buffer[0] = '\0';
            
            // 🚀 RESOLVIDO 1: O menu e o helpzinho abrem NA HORA que você aperta o 'G'!
            int col3_x = col1_w + col2_w + 1;
            int horizon_y = max_y - 6;
            int target_y = horizon_y - 10;
            
            mvhline(target_y - 1, col3_x + 1, ' ', max_x - col3_x - 2);
            attron(COLOR_PAIR(1) | A_REVERSE);
            mvprintw(target_y - 1, col3_x + 2, " SITES ❱ [1:Google | 2:Duck | 3:YT] (Aperte o número) ");
            attroff(COLOR_PAIR(1) | A_REVERSE);
            
            if (pasta_focada) {
                draw_preview_panel(max_y, max_x, pasta_focada, item_focado, zeus_active, zeus_buffer);
            }

            // Posiciona o cursor piscando na caixa de texto do input do radar
            move(target_y, col3_x + 16 + zeus_idx);
            curs_set(1); 
            refresh();
        }
        else if (zeus_active && ch != ERR) {
            // 🚀 SNIPER INSTANTÂNEO: Dispara direto no número, sem precisar apertar ENTER
            if (ch == '1' || ch == '2' || ch == '3') {
                const char *navegador_nativo = "xdg-open";
                if (access("/usr/bin/brave", X_OK) == 0) navegador_nativo = "brave";
                else if (access("/usr/bin/chromium", X_OK) == 0) navegador_nativo = "chromium";
                else if (access("/usr/bin/google-chrome", X_OK) == 0) navegador_nativo = "google-chrome";

                char url_final[1024];
                
                const char *dominio_duck = "https://duckduckgo.com";
                const char *dominio_yt   = "https://youtube.com";
                const char *dominio_goog = "https://google.com";

                // Abre a home do site direto baseado no número digitado
                if (ch == '2') {
                    snprintf(url_final, sizeof(url_final), "%s", dominio_duck);
                } else if (ch == '3') {
                    snprintf(url_final, sizeof(url_final), "%s", dominio_yt);
                } else {
                    // Se for '1', vai pro Google
                    snprintf(url_final, sizeof(url_final), "%s", dominio_goog);
                }

                // Dispara o navegador de forma tática
                if (strcmp(navegador_nativo, "xdg-open") != 0) {
                    char flag_app[2048];
                    snprintf(flag_app, sizeof(flag_app), "--app=%s", url_final);
                    spawn_external_tactic(navegador_nativo, flag_app, 0);
                } else {
                    spawn_external_tactic(navegador_nativo, url_final, 0);
                }
                
                // Desativa o Zeus imediatamente após o clique do número
                zeus_active = 0;
                zeus_buffer[0] = '\0';
                zeus_idx = 0;
                curs_set(0); 
                clear(); // Limpa e força o redesenho da tela limpa
            }
            // Se o usuário apertar ESC (27) ou 'g' de novo, ele cancela o radar sem abrir nada
            else if (ch == 27 || ch == 'g' || ch == 'G') {
                zeus_active = 0;
                zeus_buffer[0] = '\0';
                zeus_idx = 0;
                curs_set(0);
                clear();
            }
        } // 👈 Fecha o bloco do Zeus de forma limpa, permitindo que o loop principal continue


        /* ====================================================================
           🎛️ GERENCIADOR DE ATALHOS OPERACIONAIS (SISTEMA DE TECLAS)
           ==================================================================== */
        else if (ch == KEY_UP && *c_sel > 0) {

            (*c_sel)--; if (*c_sel < *c_off) (*c_off)--;
        }
        else if (ch == KEY_DOWN && *c_sel < *c_co - 1) {
            (*c_sel)++; if (*c_sel >= *c_off + max_view) (*c_off)++;
        }
                     else if (ch == ' ') {
            if (global_music) { 
                static int volume_salvo = 100; // Guarda o volume na memória do loop
                
                if (Mix_PausedMusic()) { 
                    // 🚀 PASSO 1: Devolve o volume exato que estava antes da pausa
                    Mix_VolumeMusic(volume_salvo); 
                    Mix_ResumeMusic(); 
                } else { 
                    // 🚀 PASSO 2: Salva o volume atual antes de zerar
                    int vol_atual = Mix_VolumeMusic(-1);
                    if (vol_atual > 0) volume_salvo = vol_atual; // Só salva se não estivesse zerado
                    
                    Mix_PauseMusic(); 
                    Mix_VolumeMusic(0); // Cala o mugido limpando o buffer residual
                } 
            }
        }

        else if (ch == KEY_RIGHT) { active_pane = 1; }
        else if (ch == KEY_LEFT) {
            if (active_pane == 1) { active_pane = 0; } 
            else {
                char caminho_pai[1024]; snprintf(caminho_pai, sizeof(caminho_pai), "%s/..", c_path);
                chdir(caminho_pai); getcwd(c_path, 512); 
                *c_co = list_files(c_path, c_items); *c_sel = 0; *c_off = 0;
            }
        }

        /* ====================================================================
           🎵 MECANISMO INTERNO: AUTO-NEXT (AUTO-AVANÇO DE MÚSICA)
           ==================================================================== */
        // Se a música acabou de tocar e o app não está pausado, caça a próxima da lista
        if (global_music && !Mix_PlayingMusic() && !Mix_PausedMusic()) {
            int checou_todas = 0;
            int proximo_idx = (*c_sel) + 1; // Tenta pegar o arquivo logo abaixo

            while (proximo_idx < *c_co && !checou_todas) {
                char checa_fname[1024];
                snprintf(checa_fname, sizeof(checa_fname), "%s/%s", c_path, c_items[proximo_idx].name);
                
                // Se o arquivo abaixo for um áudio válido, carrega e solta o som!
                if (check_ext(checa_fname, ".mp3") || check_ext(checa_fname, ".wav")) {
                    Mix_HaltMusic(); 
                    Mix_FreeMusic(global_music);
                    
                    global_music = Mix_LoadMUS(checa_fname);
                    if (global_music) {
                        // Restaura o volume padrão se o mute da pausa antiga ficou preso
                        if (Mix_VolumeMusic(-1) == 0) Mix_VolumeMusic(100); 
                        
                        Mix_PlayMusic(global_music, 0); 
                        Mix_SetPostMix(audio_callback, NULL); 
                        
                        *c_sel = proximo_idx; // Move a seleção visual da TUI para a música nova
                        playing_idx = proximo_idx;
                        snprintf(current_playing_file, sizeof(current_playing_file), "%s", c_items[proximo_idx].name);
                        
                        // Ajusta a rolagem da tela (offset) se a música nova sumir no rodapé
                        if (*c_sel >= *c_off + max_view) {
                            *c_off = *c_sel - max_view + 1;
                        }
                        break;
                    }
                }
                proximo_idx++;
                if (proximo_idx >= *c_co) checou_todas = 1; // Parada de segurança se chegar ao fim da pasta
            }
            clear();
        }

        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (active_pane == 0) {
                char caminho_pai[1024]; snprintf(caminho_pai, sizeof(caminho_pai), "%s/..", path_l);
                chdir(caminho_pai); getcwd(path_l, 512); count_l = list_files(path_l, items_l); sel_l = 0; offset_l = 0; 
            }
        }
        else if (ch == '-' || ch == '_') {
            int vol_atual = Mix_VolumeMusic(-1);
            vol_atual -= 8; if (vol_atual < 0) vol_atual = 0;
            Mix_VolumeMusic(vol_atual);
        }
        else if (ch == '+' || ch == '=') {
            int vol_atual = Mix_VolumeMusic(-1);
            vol_atual += 8; if (vol_atual > 128) vol_atual = 128;
            Mix_VolumeMusic(vol_atual);
        }
        else if (ch == KEY_F(1)) {
            spawn_external_tactic("udisksctl", "mount -b /dev/sdb1", 0);
            zeus_loading_anim(max_y - 5, max_x / 2 - 10);
            *c_co = list_files(c_path, c_items); clear();
        }
        else if (ch == KEY_F(2)) {
            chdir(getenv("HOME")); getcwd(path_l, 512); getcwd(path_c, 512);
            count_l = list_files(path_l, items_l); count_c = list_files(path_c, items_c);
            sel_l = 0; offset_l = 0; sel_c = 0; offset_c = 0; app_mode_active = 0;
        }
        else if (ch == KEY_F(3)) {
            app_mode_active = !app_mode_active;
            if(app_mode_active) {
                count_c = scan_applications(items_c); active_pane = 1;
            } else {
                count_c = list_files(path_c, items_c);
            }
            sel_c = 0; offset_c = 0;
        }
        else if (ch == KEY_F(4)) {
            if (*c_co > 0 && !c_items[*c_sel].is_dir) {
                char target[1024]; snprintf(target, sizeof(target), "%s/%s", c_path, c_items[*c_sel].name);
                spawn_external_tactic("nano", target, 1); clear();
            }
         }
                else if (ch == KEY_F(5)) { alternar_cores(); }
        else if (ch == KEY_F(6)) { esquema_player = (esquema_player + 1) % 4; init_colors(); }
        // ====================================================================
        // 🚀 RESOLVIDO: DISPAROS EM SEGUNDO PLANO COM DOUBLE FORK (TUI LIVRE)
        // ====================================================================
               else if (ch == KEY_F(7)) { 
            // F7: Controle de Volume (Alsamixer no Kitty)
            if (fork() == 0) {
                if (fork() == 0) {
                    setsid();
                    setenv("KITTY_DISABLE_VA", "1", 1);
                    execlp("kitty", "kitty", "--class", "ARCHON_MIX", "-o", "enable_audio_bell=no", "-e", "alsamixer", "-c", "0", (char *)NULL);
                    _exit(1);
                }
                _exit(0); 
            }
            wait(NULL);
            clear(); 
        }
        else if (ch == KEY_F(8)) { 
            // F8: Webcam (MPV de baixa latência desvinculado)
            if (fork() == 0) {
                if (fork() == 0) {
                    setsid();
                    int devnull = open("/dev/null", O_WRONLY);
                    if (devnull != -1) { dup2(devnull, 1); dup2(devnull, 2); close(devnull); }
                    execlp("sh", "sh", "-c", "pkill -15 mpv; mpv av://v4l2:/dev/video0 --profile=low-latency --ontop --no-border --geometry=320x240-10-80", (char *)NULL);
                    _exit(1);
                }
                _exit(0);
            }
            wait(NULL);
            clear(); 
        }
        else if (ch == KEY_F(9)) { 
            // F9: Menu de Rede (nmcli interativo no Kitty)
            if (fork() == 0) {
                if (fork() == 0) {
                    setsid();
                    execlp("kitty", "kitty", "--class", "ARCHON_NET", "-e", "bash", "-c", 
                           "PS3='Selecione uma opcao: '; "
                           "select opt in 'Rede Cabeada' 'Wi-Fi' 'Sair'; do "
                           "  case $opt in "
                           "    'Rede Cabeada') nmcli connection up $(nmcli -t -f NAME,TYPE connection show | grep ethernet | head -n1 | cut -d: -f1) 2>/dev/null; break ;; "
                           "    'Wi-Fi') clear; nmcli device wifi list && nmcli --ask device wifi connect; break ;; "
                           "    *) break ;; "
                           "  esac; "
                           "done", (char *)NULL);
                    _exit(1);
                }
                _exit(0);
            }
            wait(NULL);
            clear(); 
        }
        else if (ch == 10 || ch == KEY_ENTER) {
            if (*c_co > 0) {
                if (app_mode_active && active_pane == 1) {
                    char target_app[512];
                    if (strncmp(items_c[sel_c].name, "⚙ ", 4) == 0 || strncmp(items_c[sel_c].name, "⌬ ", 4) == 0) {
                        snprintf(target_app, sizeof(target_app), "%s", items_c[sel_c].name + 4);
                    } else {
                        snprintf(target_app, sizeof(target_app), "%s", items_c[sel_c].name);
                    }
                    
                    int rodar_no_terminal = items_c[sel_c].is_dir;
                    if (strcasecmp(target_app, "nano") == 0 || strcasecmp(target_app, "cfdisk") == 0 || 
                        strcasecmp(target_app, "nmtui") == 0 || strcasecmp(target_app, "htop") == 0) {
                        rodar_no_terminal = 1;
                    }
                    spawn_external_tactic(target_app, "", rodar_no_terminal); clear();
                }
                else if (c_items[*c_sel].is_dir) {
                    char novo_caminho[1024]; snprintf(novo_caminho, sizeof(novo_caminho), "%s/%s", c_path, c_items[*c_sel].name);
                    chdir(novo_caminho); getcwd(c_path, 512); 
                    *c_co = list_files(c_path, c_items); *c_sel = 0; *c_off = 0;
                } else {
                    char fname[1024]; snprintf(fname, sizeof(fname), "%s/%s", c_path, c_items[*c_sel].name);
                    if (check_ext(fname, ".mp3") || check_ext(fname, ".wav")) {
                        if (global_music) { Mix_HaltMusic(); Mix_FreeMusic(global_music); }
                        global_music = Mix_LoadMUS(fname);
                        if (global_music) {
                            Mix_PlayMusic(global_music, 0); Mix_SetPostMix(audio_callback, NULL); playing_idx = *c_sel;
                            snprintf(current_playing_file, sizeof(current_playing_file), "%s", c_items[*c_sel].name);
                        }
                    } else if (check_ext(fname, ".mp4")) { spawn_external_tactic("mpv", fname, 0); clear(); }
                    else if (check_ext(fname, ".jpg") || check_ext(fname, ".png") || check_ext(fname, ".gif")) {
                        if (getenv("WAYLAND_DISPLAY") != NULL) spawn_external_tactic("swww", fname, 0); else spawn_external_tactic("feh", fname, 0);
                        clear();
                    } else { spawn_external_tactic("nano", fname, 1); clear(); }
                }
            }
        }
                else if (ch == KEY_F(10)) {
            // F10 EXIT: Encerramento seguro da aplicação Archia / Protognum
            if (global_music) Mix_FreeMusic(global_music);
            Mix_CloseAudio(); 
            SDL_Quit(); 
            endwin(); 
            return 0; 
        }
               else if (ch == '-') {
            // VOL-: Reduz o volume interno e dispara o amixer de forma assíncrona (0% de travamento)
            int vol_atual = Mix_VolumeMusic(-1);
            vol_atual -= 8; if (vol_atual < 0) vol_atual = 0;
            Mix_VolumeMusic(vol_atual);
            
            if (fork() == 0) {
                setsid();
                // Redireciona saídas para não engasgar o descritor de arquivos do terminal
                int devnull = open("/dev/null", O_WRONLY);
                if (devnull != -1) { dup2(devnull, 1); dup2(devnull, 2); close(devnull); }
                execlp("amixer", "amixer", "-q", "set", "Master", "5%-", (char *)NULL);
                _exit(1);
            }
        }
        else if (ch == '+') {
            // VOL+: Aumenta o volume interno e dispara o amixer de forma assíncrona (0% de travamento)
            int vol_atual = Mix_VolumeMusic(-1);
            vol_atual += 8; if (vol_atual > 128) vol_atual = 128;
            Mix_VolumeMusic(vol_atual);
            
            if (fork() == 0) {
                setsid();
                int devnull = open("/dev/null", O_WRONLY);
                if (devnull != -1) { dup2(devnull, 1); dup2(devnull, 2); close(devnull); }
                execlp("amixer", "amixer", "-q", "set", "Master", "5%+", (char *)NULL);
                _exit(1);
            }
        }

        else if (ch == 's' || ch == 'S') {
            // 🚀 PROTOCOLO STEALTH ACIONADO: Encaixado com precisão dentro do loop!
            activar_modo_stealth();
            clear(); // Limpa o rastro da Matrix e devolve a TUI brilhando
        }
    } // 👈 AGORA SIM O LOOP PRINCIPAL 'while(1)' FECHA DO JEITO CERTO

    if (global_music) Mix_FreeMusic(global_music);
    Mix_CloseAudio(); 
    SDL_Quit(); 
    endwin(); 
    return 0;
} // 👈 FECHA A FUNÇÃO 'main' COM SUCESSO DO COMPILADOR
