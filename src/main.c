// 413262304 周汶宸 413262330 劉品禎
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
int asm_aabb_overlap(int dl, int dr, int dt, int db, int ol, int or_, int ot, int ob); // 保留：展示用

// ---------------- Terminal raw + nonblocking ----------------
static struct termios g_old_term;
static int g_old_flags;

static void term_restore(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
    fcntl(STDIN_FILENO, F_SETFL, g_old_flags);
    // show cursor
    (void)write(STDOUT_FILENO, "\033[?25h", 6);
}

static void term_setup(void)
{
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

static bool poll_key(char *out)
{
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1)
    {
        *out = c;
        return true;
    }
    return false;
}

// ---------------- Simple game state ----------------
#define W 60
#define H 25
#define GROUND_Y (H - 2)

// ---------- Dino Sprite (Original style, smaller) ----------
static const char *DINO_SPR[] = {
    "#####     ",
    "## ###    ",
    "#####     ",
    "#####___  ",
    "#####     ",
    "#####@#   ",
    "@######   ",
    "#####     ",
    "##  ##    ",
};
static const int DINO_H = 9;
static const int DINO_W = 10;

// ---------- Cloud Sprite (background only) ----------
static const char *CLOUD_SPR[] = {
    "   .--.   ",
    " .(    ). ",
    "(___.__)__",
};
static const int CLOUD_H = 3;
static const int CLOUD_W = 10;

// small cactus 3x3
static const char *CACTUS_SPR_S[] = {
    " | ",
    "-+-",
    " | ",
};
static const int CACTUS_S_H = 3;
static const int CACTUS_S_W = 3;

// big cactus 3x5
static const char *CACTUS_SPR_B[] = {
    "  |  ",
    "--+--",
    "  |  ",
};
static const int CACTUS_B_H = 3;
static const int CACTUS_B_W = 5;

// triangle cone 4x5
static const char *TRI_SPR[] = {
    "  ^  ",
    " / \\ ",
    "/___\\",
    "  |  ",
};
static const int TRI_H = 4;
static const int TRI_W = 5;

typedef struct
{
    const char **spr;
    int w, h;
} Sprite;

static Sprite rand_cactus(void)
{
    if (rand() % 2 == 0)
    {
        Sprite s = {CACTUS_SPR_S, CACTUS_S_W, CACTUS_S_H};
        return s;
    }
    else
    {
        Sprite s = {CACTUS_SPR_B, CACTUS_B_W, CACTUS_B_H};
        return s;
    }
}

// Put a sprite onto canvas; ' ' in sprite is transparent
static void blit(char canvas[H][W], int x0, int y0, const char **spr, int sw, int sh)
{
    for (int y = 0; y < sh; y++)
    {
        for (int x = 0; x < sw; x++)
        {
            int cx = x0 + x;
            int cy = y0 + y;
            if (cx < 0 || cx >= W || cy < 0 || cy >= H)
                continue;
            char p = spr[y][x];
            if (p != ' ')
                canvas[cy][cx] = p;
        }
    }
}

// ✅ 像素級碰撞：只有「兩個 sprite 的非空白字元」真的疊到才算碰撞
static bool pixel_collide(int ax, int ay, const char **aspr, int aw, int ah,
                          int bx, int by, const char **bspr, int bw, int bh)
{
    int left = (ax > bx) ? ax : bx;
    int right = ((ax + aw - 1) < (bx + bw - 1)) ? (ax + aw - 1) : (bx + bw - 1);
    int top = (ay > by) ? ay : by;
    int bottom = ((ay + ah - 1) < (by + bh - 1)) ? (ay + ah - 1) : (by + bh - 1);

    if (left > right || top > bottom)
        return false;

    for (int y = top; y <= bottom; y++)
    {
        for (int x = left; x <= right; x++)
        {
            char ap = aspr[y - ay][x - ax];
            char bp = bspr[y - by][x - bx];
            if (ap != ' ' && bp != ' ')
                return true; // 真正重疊
        }
    }
    return false;
}

static void draw(int dino_top_y,
                 int obs_x, Sprite obs,
                 int obs2_x, Sprite obs2, bool obs2_active,
                 int cloud_x, int cloud_y,
                 unsigned score, bool game_over)
{
    printf("\033[H");

    char canvas[H][W];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            canvas[y][x] = ' ';

    // cloud (background)
    blit(canvas, cloud_x, cloud_y, CLOUD_SPR, CLOUD_W, CLOUD_H);

    // ground
    for (int x = 0; x < W; x++)
        canvas[GROUND_Y][x] = '_';

    // obstacle 1
    int obs_top_y = GROUND_Y - (obs.h - 1);
    blit(canvas, obs_x, obs_top_y, obs.spr, obs.w, obs.h);

    // obstacle 2 (triangle)
    if (obs2_active)
    {
        int obs2_top_y = GROUND_Y - (obs2.h - 1);
        blit(canvas, obs2_x, obs2_top_y, obs2.spr, obs2.w, obs2.h);
    }

    // dino
    blit(canvas, 6, dino_top_y, DINO_SPR, DINO_W, DINO_H);

    // border + print
    putchar('+');
    for (int x = 0; x < W; x++)
        putchar('-');
    puts("+");

    for (int y = 0; y < H; y++)
    {
        putchar('|');
        for (int x = 0; x < W; x++)
            putchar(canvas[y][x]);
        puts("|");
    }

    putchar('+');
    for (int x = 0; x < W; x++)
        putchar('-');
    puts("+");

    printf("Score: %u   (SPACE=jump, r=restart, q=quit)\n", score);
    if (game_over)
        puts("GAME OVER! Press 'r' to restart.");
    fflush(stdout);
}

int main(void)
{
    srand((unsigned)time(NULL));

    printf("\033[2J\033[H");
    fflush(stdout);

    term_setup();

    // Dino
    int dino_top_y = GROUND_Y - (DINO_H - 1);
    int vy = 0;
    bool jumping = false;

    // ✅ 三段式跳躍：最多 3 次
    int jumps_left = 3;

    // Obstacle 1
    int obs_x = W - 3;
    Sprite obs = rand_cactus();

    // Obstacle 2 (triangle), activates after score > 500
    int obs2_x = W + 20;
    Sprite obs2 = {TRI_SPR, TRI_W, TRI_H};
    bool obs2_active = false;

    // Cloud
    int cloud_x = W - 12;
    int cloud_y = 1 + rand() % 3; // 1~3

    unsigned score = 0;
    bool game_over = false;

    int frame_us = 60000; // initial speed
    int grav_tick = 0;

    while (1)
    {
        bool jump_pressed = false;
        char c;

        while (poll_key(&c))
        {
            if (c == 'q')
                return 0;
            if (c == ' ')
                jump_pressed = true;

            if (c == 'r')
            {
                cloud_x = W - 12;
                cloud_y = 1 + rand() % 3;

                dino_top_y = GROUND_Y - (DINO_H - 1);
                vy = 0;
                jumping = false;

                obs_x = W - 3;
                obs = rand_cactus();

                score = 0;
                game_over = false;

                jumps_left = 3;
                obs2_x = W + 20;
                obs2_active = false;

                grav_tick = 0;
                frame_us = 60000;
            }
        }

        if (!game_over)
        {
            // ✅ 三段式跳躍（不改你原本的重力模型）
            if (jump_pressed && jumps_left > 0)
            {
                jumping = true;

                if (jumps_left == 3)
                {
                    // 第一次跳：保留你原本的力道
                    vy = -3;
                }
                else if (jumps_left == 2)
                {
                    // 第二次跳：保留你原本的力道
                    vy = -5;
                }
                else
                {
                    // 第三次跳：新增一段（更強一點）
                    vy = -7; // 只新增這段，不動你原本的參數
                }

                jumps_left--;
            }

            // physics — 保留你原本參數：grav_tick%2 才加重力
            if (jumping)
            {
                dino_top_y += vy;
                grav_tick++;
                if (grav_tick % 2 == 0)
                    vy += 1;

                int dino_bottom_y = dino_top_y + (DINO_H - 1);
                if (dino_bottom_y >= GROUND_Y)
                {
                    dino_top_y = GROUND_Y - (DINO_H - 1);
                    vy = 0;
                    jumping = false;
                    grav_tick = 0;

                    // ✅ 落地後回滿 3 段跳
                    jumps_left = 3;
                }
            }

            // obstacle 1 move (x86 asm_dec)
            obs_x = asm_dec(obs_x);
            if (obs_x <= 1)
            {
                obs_x = W - 3;
                obs = rand_cactus();
            }

            // activate 2nd obstacle after score > 500
            if (!obs2_active && score > 500)
            {
                obs2_active = true;
                obs2_x = W + 20;
            }

            // obstacle 2 move
            if (obs2_active)
            {
                obs2_x = asm_dec(obs2_x);
                if (obs2_x <= 1)
                {
                    int gap = 12 + rand() % 18;
                    obs2_x = W + gap;
                }
            }

            // cloud move (slower)
            if (score % 3 == 0)
                cloud_x -= 1;

            if (cloud_x + CLOUD_W < 0)
            {
                cloud_x = W;
                cloud_y = 1 + rand() % 3;
            }

            // collision: pixel-level (真正重疊才死)
            int dino_x = 6;
            int dino_y = dino_top_y;

            int obs1_x = obs_x;
            int obs1_y = GROUND_Y - (obs.h - 1);

            if (pixel_collide(dino_x, dino_y, DINO_SPR, DINO_W, DINO_H,
                              obs1_x, obs1_y, obs.spr, obs.w, obs.h))
            {
                game_over = true;
            }

            if (!game_over && obs2_active)
            {
                int obs2_y = GROUND_Y - (obs2.h - 1);
                if (pixel_collide(dino_x, dino_y, DINO_SPR, DINO_W, DINO_H,
                                  obs2_x, obs2_y, obs2.spr, obs2.w, obs2.h))
                {
                    game_over = true;
                }
            }

            // score += 1 (x86 asm_add_u32)
            score = asm_add_u32(score, 1);

            // speed up as score increases
            int speed_level = score / 100;
            frame_us = 60000 - speed_level * 3000;
            if (frame_us < 25000)
                frame_us = 25000;
        }

        draw(dino_top_y,
             obs_x, obs,
             obs2_x, obs2, obs2_active,
             cloud_x, cloud_y,
             score, game_over);

        usleep(frame_us);
    }
}
