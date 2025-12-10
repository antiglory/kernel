[BITS 16]
[ORG 0x7C00]
; 1 disk block = 512 bytes

PREKERNEL_ENTRY equ 0x1000

mov [BOOT_DISK], dl

_start:
    cli

    ; segment regs
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; stack (but i'll not use in this label)
    mov bp, 0x7C00
    mov sp, bp

    mov si, boot_msg
    call bprintnf

    call check_ata
    cmp byte [ATA_PRESENT], 1
    jne .no_ata_found

    ; loading prekernel into block 2
    mov bx, PREKERNEL_ENTRY
    mov dh, 4                  ; reading only 4 blocks
    mov ah, 2
    mov al, dh
    mov ch, 0
    mov dh, 0
    mov cl, 2
    mov dl, [BOOT_DISK]
    int 0x13

    mov ah, 0
    mov al, 3
    int 0x10

    ; setup GDT
    lgdt [GDT32.descriptor]

    ; enabling A20 line (to read more than 1MB of mem)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; switch to protected mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; far jump to protected mode
    jmp GDT32.code_ptr:protected_mode
.no_ata_found:
    mov si, no_ata_found_msg
    call bprintnf

    hlt

check_ata:
    mov dx, 0x1F7
    in al, dx

    cmp al, 0xFF       ; nenhum dispositivo responde -> linha flutuante
    je .no_ata

    cmp al, 0x00       ; alguns chipsets retornam 0 quando não existe
    je .no_ata

    ; força um comando NOP (0x00) —> ATA real sempre responde com ERR
    mov al, 0x00
    out dx, al

    ; delay de ~400ns exigido pelo ATA (4 reads consecutivas)
    in al, dx
    in al, dx
    in al, dx
    in al, dx

    in al, dx

    ; ERR bit (0x01) costuma setar com comando inválido
    test al, 0x01
    jnz .ata_ok

    ; se não mudou e 0x00 ou 0xFF -> não existe
    cmp al, 0xFF
    je .no_ata

    cmp al, 0x00
    je .no_ata

    ; se chegou aqui, o device tá respondendo
.ata_ok:
    mov byte [ATA_PRESENT], 1    
    ret
.no_ata:
    mov byte [ATA_PRESENT], 0
    ret

; si = pointer to string
bprintnf:
    ; AH = 0x0E (teletype)
    mov ah, 0x0E
    mov bh, 0x00        ; page
    mov bl, 0x07        ; attribute
.next_char:
    lodsb               ; AL = [SI], SI++
    test al, al         ; chegou no '\0'?
    jz .done

    int 0x10            ; print AL
    jmp .next_char
.done:
    ret

ATA_PRESENT: db 0

boot_msg db "boot image reached", 0dh, 0ah, 0
no_ata_found_msg db "[ PANIC ] boot: ATA PIO not present", 0dh, 0ah, 0

%include "source/Struct/gdt32.asm"

[BITS 32]
; 0x7c65
protected_mode:
    mov ax, GDT32.data_ptr
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov ebp, 0x1000
    mov esp, ebp

    cmp dword [PREKERNEL_ENTRY], 0xDEADBEEF
    jne .invalid_kernel

    mov eax, [PREKERNEL_ENTRY + 4]

    test eax, eax
    jz .invalid_kernel

    cmp eax, 0x1000
    jb .invalid_kernel

    jmp eax
.invalid_kernel:
    cli
    hlt

BOOT_DISK: db 0

times 510-($-$$) db 0
dw 0xAA55
