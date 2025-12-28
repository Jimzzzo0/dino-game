// 413262304 周汶宸 413262330 劉品禎
// Controls: SPACE = jump, r = restart, q = quit

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

//  x86 NASM functions
int asm_dec(int x); //移動障礙物的 X 座標(向左)
unsigned asm_add_u32(unsigned a, unsigned b); //遊戲計分
int asm_aabb_overlap(int dl, int dr, int dt, int db, int ol, int or_, int ot, int ob); //碰撞檢測
void *asm_memset(void *dst, int value, long count); //每一幀開始之前，快速將canvas全部填入空白字元
void asm_draw_ground(char *row, int ch, long n); //繪製地板，將一列陣列填滿底線

//終端機設定(用來保存原本的終端機設定，以便程式結束後恢復)
static struct termios g_old_term;
static int g_old_flags;

static void term_restore(void) // 程式結束時，讓終端機回到正常的輸入模式而且並顯示游標
{
    //離開 "Alternate Screen" （回到執行程式前的畫面）
    write(STDOUT_FILENO, "\033[?1049l", sizeof("\033[?1049l") - 1);
    // 恢復輸入
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
    fcntl(STDIN_FILENO, F_SETFL, g_old_flags);
    // 顯示游標
    (void)write(STDOUT_FILENO, "\033[?25h", 6); 
}

static void term_setup(void)//設定終端機為:遊戲模式
{
    tcgetattr(STDIN_FILENO, &g_old_term);
    struct termios t = g_old_term;
    t.c_lflag &= ~(ICANON | ECHO); // 關閉緩衝輸入與回顯
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    g_old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);// 設定Non-blocking IO
    fcntl(STDIN_FILENO, F_SETFL, g_old_flags | O_NONBLOCK);

    (void)write(STDOUT_FILENO, "\033[?25l", 6); // 隱藏游標
    // 開啟 "Alternate Screen" 緩衝區 (避免遊戲畫面洗版原本的終端機歷史紀錄)
    write(STDOUT_FILENO, "\033[?1049h\033[H", sizeof("\033[?1049h\033[H") - 1);

    atexit(term_restore);// 結束時自動呼叫 restore
}

static bool poll_key(char *out) // 檢查是否有鍵盤輸入，如果有按鍵，回傳 true 並將字元存入 *out
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

//遊戲參數(寬度/高度/地板所在的 Y 座標)
#define W 60  
#define H 25
#define GROUND_Y (H - 2)

//初始生命&受傷後的無敵時間
#define LIVES_INIT 3
#define INVINCIBLE_FRAMES 12 // 約 0.7 秒（起始 60ms/frame 時）

//圖案
static const char *DINO_SPR[] = {  // 恐龍圖案
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

static const char *CLOUD_SPR[] = { // 雲朵圖案
    "   .--.   ",
    " .(    ). ",
    "(___.__)__",
};
static const int CLOUD_H = 3;
static const int CLOUD_W = 10;

static const char *CACTUS_SPR_S[] = { // 小仙人掌
    " | ",
    "-+-",
    " | ",
};
static const int CACTUS_S_H = 3;
static const int CACTUS_S_W = 3;

static const char *CACTUS_SPR_B[] = { // 大仙人掌
    "  |  ",
    "--+--",
    "  |  ",
};
static const int CACTUS_B_H = 3;
static const int CACTUS_B_W = 5;

static const char *TRI_SPR[] = { // 三角錐障礙物
    "  ^  ",
    " / \\ ",
    "/___\\",
    "  |  ",
};
static const int TRI_H = 4;
static const int TRI_W = 5;

typedef struct // 方便傳遞圖案資料
{
    const char **spr;
    int w, h;
} Sprite;

static Sprite rand_cactus(void) // 隨機回傳大仙人掌或小仙人掌
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

// 將圖案繪製到Canvas上
// canvas: 整個畫面的二維陣列
// x0, y0: 圖案左上角座標
// spr: 圖案內容 (空白字元 ' ' 當作透明處理)
static void blit(char canvas[H][W], int x0, int y0, const char **spr, int sw, int sh)
{
    for (int y = 0; y < sh; y++)
    {
        for (int x = 0; x < sw; x++)
        {
            int cx = x0 + x;
            int cy = y0 + y;
            if (cx < 0 || cx >= W || cy < 0 || cy >= H) // 邊界檢查，超出畫面不畫
                continue;
            char p = spr[y][x];
            if (p != ' ') // 透明度處理：不是空白才畫上去
                canvas[cy][cx] = p;
        }
    }
}
//像素級碰撞檢測，計算兩個物件重疊的矩形區域，再逐格檢查是否雙方都有非空白字元
static bool pixel_collide(int ax, int ay, const char **aspr, int aw, int ah,
                          int bx, int by, const char **bspr, int bw, int bh) 
{
    // 計算重疊區域的邊界
    int left = (ax > bx) ? ax : bx;
    int right = ((ax + aw - 1) < (bx + bw - 1)) ? (ax + aw - 1) : (bx + bw - 1);
    int top = (ay > by) ? ay : by;
    int bottom = ((ay + ah - 1) < (by + bh - 1)) ? (ay + ah - 1) : (by + bh - 1);

    if (left > right || top > bottom)  // 沒有重疊
        return false;

    for (int y = top; y <= bottom; y++) // 掃描重疊區域內的每一格
    {
        for (int x = left; x <= right; x++)
        { 
            char ap = aspr[y - ay][x - ax]; // 取出物件 A 和 B 在該位置的字元
            char bp = bspr[y - by][x - bx];
            if (ap != ' ' && bp != ' ') // 兩者都不是空白才算撞到
                return true;
        }
    }
    return false;
}

static void print_lives(int lives) // 顯示生命值
{
    for (int i = 0; i < lives; i++)
        printf("♥");
}

// 繪圖函式：負責渲染每一幀畫面
static void draw(int dino_top_y,
                 int obs_x, Sprite obs,
                 int obs2_x, Sprite obs2, bool obs2_active,
                 int cloud_x, int cloud_y,
                 unsigned score, int lives, int inv_frames,
                 bool game_over) 
{
    write(STDOUT_FILENO, "\033[H", sizeof("\033[H") - 1); // 將游標移動到左上角 (0,0)，避免畫面閃爍

    char canvas[H][W];
    asm_memset(&canvas[0][0], ' ', (long)(H * W)); //清空整個畫布
    asm_draw_ground(&canvas[GROUND_Y][0], '_', (long)W); //畫出地板線條
    blit(canvas, cloud_x, cloud_y, CLOUD_SPR, CLOUD_W, CLOUD_H); // 畫雲

    for (int x = 0; x < W; x++) // 修正地板陣列資料 (要確保邏輯層與繪圖層一致)
        canvas[GROUND_Y][x] = '_';

    int obs_top_y = GROUND_Y - (obs.h - 1); // 畫障礙物仙人掌
    blit(canvas, obs_x, obs_top_y, obs.spr, obs.w, obs.h);

    if (obs2_active) // 畫障礙物三角錐(若已啟動)
    {
        int obs2_top_y = GROUND_Y - (obs2.h - 1);
        blit(canvas, obs2_x, obs2_top_y, obs2.spr, obs2.w, obs2.h);
    }

    bool draw_dino = true; // 畫恐龍 (處理受傷無敵時的閃爍效果)
    if (inv_frames > 0)
        draw_dino = (inv_frames % 2 == 0); // 偶數幀才畫，製造閃爍的效果

    if (draw_dino)
        blit(canvas, 6, dino_top_y, DINO_SPR, DINO_W, DINO_H);
    
    // 輸出最終畫面到終端機 // 上邊框
    putchar('+');
    for (int x = 0; x < W; x++)
        putchar('-');
    puts("+");
    for (int y = 0; y < H; y++) // 內容每一行
    {
        putchar('|');
        for (int x = 0; x < W; x++)
            putchar(canvas[y][x]);
        puts("|");
    }
    putchar('+'); // 下邊框
    for (int x = 0; x < W; x++)
        putchar('-');
    puts("+");

    // HUD ：分數與生命 // 先印出佔位符，再用 \r回到行首覆寫，確保寬度一致
    char hud[160];
    int n = snprintf(hud, sizeof(hud),
                     "Score: %u   Lives: ", score);

    int pos = n;
    for (int i = 0; i < lives && pos < (int)sizeof(hud) - 1; i++)
        hud[pos++] = '\x03'; // 先塞占位字元，等下直接用 printf 印 hearts

    hud[pos] = '\0';

    // 清空當前行並印出 HUD（回到行首 + 清到行尾）
    printf("\r\033[K");
    printf("Score: %u   Lives: ", score);
    print_lives(lives);

    if (inv_frames > 0)
        printf("   [INV %d]", inv_frames);

    printf("   (SPACE=jump, r=restart, q=quit)\n");

    if (game_over)
        puts("GAME OVER! Press 'r' to restart.");
    fflush(stdout); // 強制刷新輸出緩衝區，確保畫面立即顯示
}

int main(void)
{
    srand((unsigned)time(NULL));

    printf("\033[2J\033[H"); // 程式開始前先清空一次螢幕
    fflush(stdout);

    term_setup(); // 進入遊戲模式終端設定
     // 遊戲變數初始化
    int dino_top_y = GROUND_Y - (DINO_H - 1);
    int vy = 0; // Y軸速度
    bool jumping = false; // 是否在跳躍中

    int jumps_left = 3; // 三段式跳躍計數器

    int obs_x = W - 3; // 障礙物(仙人掌)初始化
    Sprite obs = rand_cactus();

    // 障礙物(三角錐) 初始化，分數 > 500 後才會出現
    int obs2_x = W + 20;
    Sprite obs2 = {TRI_SPR, TRI_W, TRI_H};
    bool obs2_active = false;

    // 雲朵
    int cloud_x = W - 12;
    int cloud_y = 1 + rand() % 3; // 1~3

    // 生命與狀態、分數
    int lives = LIVES_INIT;
    int inv_frames = 0;
    unsigned score = 0;
    bool game_over = false;

    int frame_us = 60000; // 每幀微秒數 (控制 FPS，初始約 16 FPS)
    int grav_tick = 0; // 用來控制重力加速度頻率

    while (1) // 遊戲主迴圈
    {
        bool jump_pressed = false;
        char c;

        while (poll_key(&c)) // 處理輸入
        {
            if (c == 'q') return 0; // 離開
            if (c == ' ') jump_pressed = true; // 跳躍

            if (c == 'r') // 重新開始
            {   // 重置所有變數
                dino_top_y = GROUND_Y - (DINO_H - 1); 
                vy = 0;
                jumping = false;
                grav_tick = 0;
                jumps_left = 3;
                obs_x = W - 3;
                obs = rand_cactus();
                obs2_x = W + 20;
                obs2_active = false;
                cloud_x = W - 12;
                cloud_y = 1 + rand() % 3;
                lives = LIVES_INIT;
                inv_frames = 0;
                score = 0;
                frame_us = 60000;
                game_over = false;
            }
        }

        if (!game_over)
        {
            // 更新無敵時間
            if (inv_frames > 0)
                inv_frames--;
            // 跳躍邏輯
            if (jump_pressed && jumps_left > 0)
            {
                jumping = true;

                if (jumps_left == 3) // 根據剩餘跳躍次數給予不同的初速度 (多段跳)
                    vy = -3; // 第一次
                else if (jumps_left == 2)
                    vy = -5; // 第二次
                else
                    vy = -7; // 第三次

                jumps_left--;
            }

            // 物理引擎 (重力與移動)
            if (jumping)
            {
                dino_top_y += vy;// 更新位置
                grav_tick++;
                if (grav_tick % 2 == 0) // 每 2 幀增加一次重力 (模擬重力加速度)
                    vy += 1;

                int dino_bottom_y = dino_top_y + (DINO_H - 1); // 地板碰撞檢測 (落地)
                if (dino_bottom_y >= GROUND_Y)
                {
                    dino_top_y = GROUND_Y - (DINO_H - 1); // 修正位置
                    vy = 0;
                    jumping = false;
                    grav_tick = 0;
                    jumps_left = 3; // 落地後補滿跳躍次數
                }
            }
            // 障礙物移動 (x86 ASM 函式) 
            obs_x = asm_dec(obs_x); // asm_dec(x) 等同於 x = x - 1
            if (obs_x <= 1) // 障礙物超出左邊界，重置到右邊
            {
                obs_x = W - 3;
                obs = rand_cactus();
            }
            // 啟動第二個障礙物
            if (!obs2_active && score > 500)
            {
                obs2_active = true;
                obs2_x = W + 20;
            }
            if (obs2_active)
            {
                obs2_x = asm_dec(obs2_x);

                if (obs2_x <= 1)
                {
                    int gap = 12 + rand() % 18; // 12~29
                    obs2_x = W + gap;
                }
            }
            // 背景雲朵移動 (速度較慢)
            if (score % 3 == 0)
                cloud_x -= 1;
            if (cloud_x + CLOUD_W < 0)
            {
                cloud_x = W;
                cloud_y = 1 + rand() % 3;
            }
            // 碰撞檢測 
            if (inv_frames == 0) // 無敵狀態不檢測
            {
                int dino_x = 6; //恐龍的位置
                int dino_y = dino_top_y;

                int obs1_x = obs_x; //障礙物的位置
                int obs1_y = GROUND_Y - (obs.h - 1);

                bool hit = false; //初始化碰撞狀態
                
                if (pixel_collide(dino_x, dino_y, DINO_SPR, DINO_W, DINO_H,
                                  obs1_x, obs1_y, obs.spr, obs.w, obs.h)) // 檢查障礙物 1
                {
                    hit = true;
                }
                else if (obs2_active)// 檢查障礙物 2
                {
                    int obs2_y = GROUND_Y - (obs2.h - 1);
                    if (pixel_collide(dino_x, dino_y, DINO_SPR, DINO_W, DINO_H,
                                      obs2_x, obs2_y, obs2.spr, obs2.w, obs2.h))
                    {
                        hit = true;
                    }
                }

                if (hit)
                {
                    lives--;
                    inv_frames = INVINCIBLE_FRAMES; // 給予短暫無敵
                    if (lives <= 0)
                        game_over = true;
                }
            }
            score = asm_add_u32(score, 1); // 分數更新 (x86 ASM 函式) 

            int speed_level = score / 100; // 難度調整 (隨分數增加加快遊戲速度) 
            frame_us = 60000 - speed_level * 3000;
            if (frame_us < 25000)
                frame_us = 25000; // 速度上限
        }
        // 呼叫繪圖
        draw(dino_top_y,
             obs_x, obs,
             obs2_x, obs2, obs2_active,
             cloud_x, cloud_y,
             score, lives, inv_frames,
             game_over);
        // 控制 FPS
        usleep(frame_us);
    }
}
