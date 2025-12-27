; src/asm_funcs.asm
; 目的：提供可被 C 呼叫的 x86-64 組語函式（NASM）
; 平台：Linux x86-64 System V ABI
;
; 函式列表：
;   int asm_dec(int x);                         // x-1
;   unsigned asm_add_u32(unsigned a, unsigned b); // a+b
;   int asm_aabb_overlap(int dl,int dr,int dt,int db,
;                        int ol,int or,int ot,int ob); // 1=overlap,0=no

global asm_dec
global asm_add_u32
global asm_aabb_overlap

section .text

; int asm_dec(int x);
; x 在 edi，回傳在 eax
asm_dec:
    mov eax, edi
    sub eax, 1
    ret

; unsigned asm_add_u32(unsigned a, unsigned b);
; a 在 edi，b 在 esi，回傳在 eax
asm_add_u32:
    mov eax, edi
    add eax, esi
    ret

; int asm_aabb_overlap(int dl,int dr,int dt,int db,
;                      int ol,int or,int ot,int ob);
;
; 參數（System V）：
;   dl=rdi, dr=rsi, dt=rdx, db=rcx, ol=r8, or=r9
;   第7、第8個參數（ot, ob）會在 stack 上
; 回傳：eax = 1 (overlap) 或 0 (no)
asm_aabb_overlap:
    ; 取 stack 上的第7、第8個參數
    ; 進入函式時 rsp 指向 return address
    ; [rsp+8]  = ot
    ; [rsp+16] = ob
    mov r10d, dword [rsp + 8]    ; ot
    mov r11d, dword [rsp + 16]   ; ob

    ; overlap_x = !(dr < ol || dl > or)
    ; if (dr < ol) return 0
    cmp esi, r8d                 ; dr ? ol
    jl .no

    ; if (dl > or) return 0
    cmp edi, r9d                 ; dl ? or
    jg .no

    ; overlap_y = !(db < ot || dt > ob)
    ; if (db < ot) return 0
    cmp ecx, r10d                ; db ? ot
    jl .no

    ; if (dt > ob) return 0
    cmp edx, r11d                ; dt ? ob
    jg .no

    mov eax, 1
    ret

.no:
    xor eax, eax
    ret
