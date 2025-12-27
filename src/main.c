//413262304 周汶宸 413262330 劉品禎
// Dino Game v0+ (terminal, Linux/WSL) - ASCII sprites
// Controls: SPACE = jump, r = restart, q = quit
// Build: make
// Run:   make run

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

// ---------------- x86 NASM functions (linked by GCC+NASM) ----------------
// 下面這些函式「定義在 src/asm_funcs.asm」，C 這裡是宣告給 compiler 知道介面
int asm_dec(int x);
unsigned asm_add_u32(unsigned a, unsigned b);
int asm_aabb_overlap(int dl,int dr,int dt,int db, int ol,int or_,int ot,int ob);


// ---------------- Terminal raw + nonblocking ----------------
static struct termios g_old_term;
static int g_old_flags;

static void term_restore(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
    fcntl(STDIN_FILENO, F_SETFL, g_old_flags);
    // show cursor
    (void)write(STDOUT_FILENO, "\033[?25h", 6);
}

static void term_setup(void) {
    tcgetattr(STDIN_FILENO, &g_old_term);
    struct termios t = g_old_term;
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    g_old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, g_old_flags | O_NONBLOCK);

    // hide cursor
    (void)write(STDOUT_FILENO, "\033[?25l", 6);

    atexit(term_restore);
}

static bool poll_key(char *out) {
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1) { *out = c; return true; }
    return false;
}

// ---------------- Simple game state ----------------
#define W 60
#define H 25
#define GROUND_Y (H-2)

// ---------- Dino Sprite (Large ASCII, monospace safe) ----------
static const char* DINO_SPR[] = {
    "########      ",
    "### ######    ",
    "###########   ",
    "###########   ",
    "#######___    ",
    "#######       ",
    "########      ",
    "#########@@#  ",
    "##########    ",
    "@############ ",
    "############  ",
    "#########     ",
    "##   ##       ",
    "#_  #__       ",
};
static const int DINO_H = 14;
static const int DINO_W = 14;




// small cactus 3x3
static const char* CACTUS_SPR_S[] = {
    " | ",
    "-+-",
    " | ",
};
static const int CACTUS_S_H = 3;
static const int CACTUS_S_W = 3;

// big cactus 3x5
static const char* CACTUS_SPR_B[] = {
    "  |  ",
    "--+--",
    "  |  ",
};
static const int CACTUS_B_H = 3;
static const int CACTUS_B_W = 5;

typedef struct {
    const char** spr;
    int w, h;
} Sprite;

static Sprite rand_cactus(void) {
    if (rand() % 2 == 0) {
        Sprite s = { CACTUS_SPR_S, CACTUS_S_W, CACTUS_S_H };
        return s;
    } else {
        Sprite s = { CACTUS_SPR_B, CACTUS_B_W, CACTUS_B_H };
        return s;
    }
}

// Put a sprite onto canvas; ' ' in sprite is transparent
static void blit(char canvas[H][W], int x0, int y0, const char** spr, int sw, int sh) {
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            int cx = x0 + x;
            int cy = y0 + y;
            if (cx < 0 || cx >= W || cy < 0 || cy >= H) continue;
            char p = spr[y][x];
            if (p != ' ') canvas[cy][cx] = p;
        }
    }
}

static void draw(int dino_top_y, int obs_x, Sprite obs, unsigned score, bool game_over) {
    // move cursor to top-left
    printf("\033[H");

    // 1) build canvas
    char canvas[H][W];
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) canvas[y][x] = ' ';
    }

    // 2) ground
    for (int x = 0; x < W; x++) canvas[GROUND_Y][x] = '_';

    // 3) obstacle: bottom on ground
    int obs_top_y = GROUND_Y - (obs.h - 1);
    blit(canvas, obs_x, obs_top_y, obs.spr, obs.w, obs.h);

    // 4) dino: fixed x, top-left at (6, dino_top_y)
    blit(canvas, 6, dino_top_y, DINO_SPR, DINO_W, DINO_H);

    // 5) print with border
    putchar('+');
    for (int x = 0; x < W; x++) putchar('-');
    puts("+");

    for (int y = 0; y < H; y++) {
        putchar('|');
        for (int x = 0; x < W; x++) putchar(canvas[y][x]);
        puts("|");
    }

    putchar('+');
    for (int x = 0; x < W; x++) putchar('-');
    puts("+");

    printf("Score: %u   (SPACE=jump, r=restart, q=quit)\n", score);
    if (game_over) puts("GAME OVER! Press 'r' to restart.");
    fflush(stdout);
}

int main(void) {
    srand((unsigned)time(NULL));

    // clear screen once
    printf("\033[2J\033[H");
    fflush(stdout);

    term_setup();

    // Dino: store top-left y for sprite
    int dino_top_y = GROUND_Y - (DINO_H - 1);
    int vy = 0;
    bool jumping = false;

    // Obstacle
    int obs_x = W - 3;
    Sprite obs = rand_cactus();

    unsigned score = 0;
    bool game_over = false;

    const int frame_us = 60000; // ~16 FPS

    while (1) {
        bool jump_pressed = false;
        char c;

        while (poll_key(&c)) {
            if (c == 'q') return 0;
            if (c == ' ') jump_pressed = true;

            if (c == 'r') {
                dino_top_y = GROUND_Y - (DINO_H - 1);
                vy = 0;
                jumping = false;

                obs_x = W - 3;
                obs = rand_cactus();

                score = 0;
                game_over = false;
            }
        }

        if (!game_over) {
            // jump
            if (jump_pressed && !jumping) {
                jumping = true;
                vy = -5;
            }

            // physics
            if (jumping) {
                dino_top_y += vy;
                vy += 0.7; // gravity

                int dino_bottom_y = dino_top_y + (DINO_H - 1);
                if (dino_bottom_y >= GROUND_Y) {
                    dino_top_y = GROUND_Y - (DINO_H - 1);
                    vy = 0;
                    jumping = false;
                }
            }

            // obstacle move
            // obstacle move (x86 asm_dec)
            obs_x = asm_dec(obs_x);

            if (obs_x <= 1) {
                obs_x = W - 3;
                obs = rand_cactus();
            }

            // AABB collision (dino sprite vs obstacle sprite)
            int dino_left = 6, dino_right = 6 + DINO_W - 1;
            int dino_top = dino_top_y, dino_bottom = dino_top_y + DINO_H - 1;

            int obs_left = obs_x, obs_right = obs_x + obs.w - 1;
            int obs_top = GROUND_Y - (obs.h - 1);
            int obs_bottom = GROUND_Y;

            // AABB collision moved to x86 asm (asm_aabb_overlap returns 1 if overlap)
            if (asm_aabb_overlap(
            dino_left, dino_right, dino_top, dino_bottom,
            obs_left,  obs_right,  obs_top,  obs_bottom
            )) {
            game_over = true;
            }


            // score += 1 (x86 asm_add_u32)
            score = asm_add_u32(score, 1);

        }

        draw(dino_top_y, obs_x, obs, score, game_over);
        usleep(frame_us);
    }
}
