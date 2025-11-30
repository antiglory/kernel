global pstart

global pml4_table
global pdpt_table
global pd_table
global pt_tables

[BITS 32]
; 0x1000
pstart:
    ; enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; enable PSE
    mov eax, cr4
    or eax, 1 << 4 
    mov cr4, eax

    ; clear CD and NW in CR0
    mov eax, cr0
    and eax, 0x9FFFFFFF
    mov cr0, eax

    call setup_paging

    ; set CR3 to point to the PML4 table
    mov eax, pml4_table
    mov cr3, eax

    ; enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; enable paging and load GDT
    lgdt [GDT64.descriptor]

    mov eax, cr0
    or eax, 1               ; PE
    or eax, 1 << 31         ; PG
    mov cr0, eax

    jmp GDT64.code_ptr:end

%include "source/Struct/gdt64.asm"

setup_paging:
    ; VA == PA; 0xffffffff80100000 -> 0x0000000000100000
    ; PML4 entries
    mov edi, pml4_table
    mov eax, pdpt_table
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [edi], eax                     ; entry 0 - lower addresses
    
    mov edi, pml4_table + (511 * 8)    ; entry 511 - high addresses
    mov eax, pdpt_table
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [edi], eax

    ; PDPT entries
    mov edi, pdpt_table                ; entry 0 - lower addresses
    mov eax, pd_table
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [edi], eax      

    mov edi, pdpt_table + (510 * 8)    ; entry 511 - high addresses
    mov eax, pd_table
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [edi], eax

    ; PD entries
    mov edi, pd_table
    mov eax, pt_tables
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [edi], eax      

    ; PT entries for kernel space
    mov edi, pt_tables
    mov eax, KERNEL_PHYSICAL_ENTRY
    mov ecx, 512        
.setup_pt:
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [edi], eax
    add edi, 8          
    add eax, PAGE_SIZE  
    loop .setup_pt

    ; PT entries for lower memory
    mov edi, pt_tables
    xor eax, eax        
    mov ecx, 256        
.setup_lower_pt:
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [edi], eax
    add edi, 8
    add eax, PAGE_SIZE
    loop .setup_lower_pt

    ret

KERNEL_PHYSICAL_ENTRY equ 0x100000
KERNEL_VIRTUAL_ENTRY  equ 0xFFFFFFFF80100000

KERNEL_BLOCK_START equ 8
KERNEL_BLOCK_COUNT equ 16

[BITS 64]
end:
    xor rax, rax

    mov ax, GDT64.data_ptr
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    call load_kernel

    mov rdi, [KERNEL_PHYSICAL_ENTRY]
    mov rsi, [KERNEL_VIRTUAL_ENTRY]

    cmp rdi, rsi
    jne .me_handler
    
    jmp KERNEL_VIRTUAL_ENTRY
.me_handler:
    ; if not equal -> mapping failed
    mov rsi, MAP_ERROR_MSG
    call pkprintnf

    hlt

load_kernel:
    mov rdi, KERNEL_VIRTUAL_ENTRY
    mov rax, KERNEL_BLOCK_START
    mov rcx, KERNEL_BLOCK_COUNT
.read_loop:
    push rcx
    push rax

    ; copy LBA to EBX and extract bits
    mov ebx, eax    ; EBX = current LBA (32 bits)

    ; set 0x1F6 (drive/head)
    mov dx, 0x1F6

    ; calculate LBA (EBX) bits 24-27 manually
    mov eax, ebx    ; copy LBA to EAX
    shr eax, 24     ; shift EAX in 24 bits to right (bits 24-27 in AL)
    and al, 0x0F    ; applies the 0x0F mask to isolate the 4 bits
    or al, 0xE0     ; combine with 0xE0 (drive master + LBA mode)
    out dx, al

    ; set 0x1F2 -> block count
    mov dx, 0x1F2
    mov al, 1       ; 1 block once
    out dx, al

    ; 0x1F3 -> LBA low: bits 0-7
    mov dx, 0x1F3
    mov al, bl      ; BL = bits 0-7 of LBA
    out dx, al

    ; set 0x1F4 -> LBA mid: bits 8-15
    mov dx, 0x1F4
    mov al, bh      ; BH = bits 8-15 of LBA
    out dx, al

    ; set 0x1F5 -> LBA high: bits 16-23
    mov dx, 0x1F5
    shr ebx, 16     ; EBX >> 16: bits 16-23 in BL
    mov al, bl
    out dx, al

    ; read
    mov dx, 0x1F7
    mov al, 0x20
    out dx, al

    mov ecx, 100000
.wait_disk:
    in al, dx

    test al, 0x80       ; check BSY bit (bit 7)
    jnz .wait_disk      ; wait while busy

    test al, 0x08       ; check DRQ bit (bit 3)
    jz .wait_disk       ; wait until data is ready
.ready:
    ; read 512 bytes (256 words) of the block
    mov ecx, 256
    mov dx, 0x1F0
    rep insw            ; read to [RDI]

    ; restore LBA and counter, increments LBA
    pop rax
    inc rax
    pop rcx

    loop .read_loop

    ret

; rdi: string address
pkprintnf:
    mov rsi, rdi
    mov rbx, 0xB8000    ; video mem address to text mode
    mov rcx, 0          ; char index (shift in video mem)
.l:
    lodsb               ; loads the next byte of the string in al (inc rsi)
    test al, al
    jz .e               ; if 0, end

    ; write char in video mem
    mov [rbx + rcx*2], al               ; char (each cell have 2 bytes for char and color)
    mov BYTE [rbx + rcx*2 + 1], 0x0F    ; white color, black back

    inc rcx     ; next position in video mem
    jmp .l      ; loops to the next char
.e:
    ret

DISK_ERROR_MSG   db "[ PANIC ] boot: disk read for kernel timeout!",   0x0
MAP_ERROR_MSG    db "[ PANIC ] boot: failed to setup kernel PML4!",    0x0

PAGE_SIZE             equ 4096
ENTRIES_PER_TABLE     equ 512
PTE_PRESENT           equ 1
PTE_WRITABLE          equ 2

align 4096
pml4_table resb PAGE_SIZE
pdpt_table resb PAGE_SIZE
pd_table   resb PAGE_SIZE
pt_tables  resb 512 * PAGE_SIZE ; tbh i think this is too much
