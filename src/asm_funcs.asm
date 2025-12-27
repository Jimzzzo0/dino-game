;數值運算、遊戲邏輯、碰撞判定
; src/asm_funcs.asm
; NASM x86-64 (System V AMD64 ABI), ELF64

default rel
section .text

global asm_dec
global asm_add_u32
global asm_aabb_overlap

global asm_memset
global asm_draw_ground

; ------------------------------------------------------------
; int asm_dec(int x)
; return x - 1
; args: x in edi
; ret : eax
; ------------------------------------------------------------
asm_dec:
    mov eax, edi
    sub eax, 1
    ret

; ------------------------------------------------------------
; unsigned asm_add_u32(unsigned a, unsigned b)
; return a + b (32-bit)
; args: a in edi, b in esi
; ret : eax
; ------------------------------------------------------------
asm_add_u32:
    mov eax, edi
    add eax, esi
    ret

; ------------------------------------------------------------
; int asm_aabb_overlap(dl,dr,dt,db, ol,or,ot,ob)
; return 1 if overlap else 0
; SysV args:
;   1: edi=dl, 2: esi=dr, 3: edx=dt, 4: ecx=db
;   5: r8d=ol, 6: r9d=or
;   7: [rsp+8]=ot, 8: [rsp+16]=ob   (after call return address)
; ------------------------------------------------------------
asm_aabb_overlap:
    ; load stack args ot, ob
    mov r10d, dword [rsp + 8]    ; ot
    mov r11d, dword [rsp + 16]   ; ob

    ; overlap_x = !(dr < ol || dl > or)
    ; if (dr < ol) -> no overlap
    cmp esi, r8d
    jl .no

    ; if (dl > or) -> no overlap
    cmp edi, r9d
    jg .no

    ; overlap_y = !(db < ot || dt > ob)
    cmp ecx, r10d
    jl .no

    cmp edx, r11d
    jg .no

    mov eax, 1
    ret
.no:
    xor eax, eax
    ret

; ------------------------------------------------------------
; void* asm_memset(void* dst, int value, long count)
; Fill count bytes at dst with (unsigned char)value
; args: rdi=dst, esi=value, rdx=count
; ret : rax=dst
; uses: rep stosb
; ------------------------------------------------------------
asm_memset:
    mov rax, rdi        ; return dst
    mov rcx, rdx        ; rcx = count
    mov al, sil         ; al = (unsigned char)value
    rep stosb
    ret

; ------------------------------------------------------------
; void asm_draw_ground(char* row, int ch, long n)
; Fill n bytes of row with (unsigned char)ch
; args: rdi=row, esi=ch, rdx=n
; ret : none
; ------------------------------------------------------------
asm_draw_ground:
    mov rcx, rdx
    mov al, sil
    rep stosb
    ret
