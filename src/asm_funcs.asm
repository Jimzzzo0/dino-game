;數值運算、遊戲邏輯、碰撞判定
; src/asm_funcs.asm
; NASM x86-64 (System V AMD64 ABI), ELF64

default rel
section .text

; 匯出函式名稱，讓 C 語言Linker能找到這些函式 
global asm_dec
global asm_add_u32
global asm_aabb_overlap

global asm_memset
global asm_draw_ground

; ------------------------------------------------------------
; int asm_dec(int x) 
; return x - 1 將輸入整數減 1 (x - 1)
; args: x in edi 參數
; ret : eax 回傳
; ------------------------------------------------------------
asm_dec:
    mov eax, edi ; 將參數 x (在 edi) 複製到回傳暫存器 eax
    sub eax, 1 ; eax = eax - 1 (減法運算)
    ret ; 返回，結果存於 eax

; ------------------------------------------------------------
; unsigned asm_add_u32(unsigned a, unsigned b)
; return a + b (32-bit) 兩數相加
; args: a in edi, b in esi 參數
; ret : eax 回傳
; ------------------------------------------------------------
asm_add_u32:
    mov eax, edi ; 將第一個參數 a (edi) 放入 eax
    add eax, esi ; eax = eax + b (第二個參數在 esi)
    ret ; 返回

; ------------------------------------------------------------
; int asm_aabb_overlap(dl,dr,dt,db, ol,or,ot,ob)
; return 1 if overlap else 0
; SysV args:
;   1: edi=dl(Dino Left), 2: esi=dr, 3: edx=dt, 4: ecx=db
;   5: r8d=ol, 6: r9d=or
;   7: [rsp+8]=ot(Obs Top), 8: [rsp+16]=ob (暫存器用完了，剩下放堆疊)
; 採用「排除法」，只要有一個方向分離，就算沒撞到。
; 如果 (Rect1 右邊 < Rect2 左邊) 或 (Rect1 左邊 > Rect2 右邊)... 則沒撞到
; ------------------------------------------------------------
asm_aabb_overlap:
    ; 1. 從堆疊讀取第 7, 8 個參數
    mov r10d, dword [rsp + 8]    ; ot
    mov r11d, dword [rsp + 16]   ; ob

    ; 2. 檢查 X 軸是否分離 (不重疊)
    ; 條件 A: Dino右邊 < Obs左邊 ?
    cmp esi, r8d      ; 比較 dr (esi) 與 ol (r8d)
    jl .no            ; 若 dr < ol (Jump if Less)，代表 Dino 在 Obs 左邊沒撞到 -> 跳去 .no

    ; 條件 B: Dino左邊 > Obs右邊 ?
    cmp edi, r9d      ; 比較 dl (edi) 與 or (r9d)
    jg .no            ; 若 dl > or (Jump if Greater)，代表 Dino 在 Obs 右邊沒撞到 -> 跳去 .no

    ; 3. 檢查 Y 軸是否分離 (不重疊)
    ; 條件 C: Dino底部 < Obs頂部 ? (這裡假設數值越小越上面)
    cmp ecx, r10d     ; 比較 db (ecx) 與 ot (r10d)
    jl .no            ; 若 db < ot，代表兩者垂直分離 -> 跳去 .no

    ; 條件 D: Dino頂部 > Obs底部 ?
    cmp edx, r11d     ; 比較 dt (edx) 與 ob (r11d)
    jg .no            ; 若 dt > ob，代表兩者垂直分離 -> 跳去 .no

    4. 判定重疊，如果上述 4 個分離條件都不成立，代表兩個矩形有交集
    mov eax, 1        ; 設定回傳值為 1 (True)
    ret               ; 返回
.no:
    ; 5. 判定未重疊
    xor eax, eax      ; eax = 0 (False)
    ret               ; 返回

; ------------------------------------------------------------
; void* asm_memset(void* dst, int value, long count)
; 功能: 快速記憶體填充
; 將 dst 指向的記憶體區塊，填入 count 個 value 值
; args: rdi=dst(目的地位址), esi=value(要填入的值，只取低 8 bit), rdx=count(填寫數量) 
; ret : rax=dst 回傳
; 用於每一幀開始前，將整個Canvas清空為空白
; ------------------------------------------------------------
asm_memset:
    mov rax, rdi        ; ; 準備回傳值 rax = dst
    mov rcx, rdx        ; 設定計數器 RCX = count
    mov al, sil         ; 設定填入值 AL = value 的低 8 bit
    rep stosb
    ; rep stosb 重複執行 RCX 次: 
    ; 1. 將 AL 的值寫入 [RDI]
    ; 2. RDI = RDI + 1
    ; 3. RCX = RCX - 1
    ret                  ; 返回

; ------------------------------------------------------------
; void asm_draw_ground(char* row, int ch, long n)
;  專門用來畫地板的函式，將 row 這一列的 n 個字元填滿 ch
; args: rdi=row(該列的起始位址), esi=ch(要畫的字元), rdx=n(長度/寬度)
; ret : none
; ------------------------------------------------------------
asm_draw_ground:
    mov rcx, rdx         ; 設定計數器 RCX = n (長度)
    mov al, sil          ; 設定填入字元 AL = ch
    rep stosb            ; 使用 rep stosb 快速填滿整列(執行填入動作，直到長度填滿)
    ret                  ; 返回
