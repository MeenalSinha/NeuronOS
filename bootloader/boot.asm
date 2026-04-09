; NeuronOS Bootloader - Stage 1
; 512-byte boot sector loaded at 0x7C00
; Switches to protected mode and loads kernel

BITS 16
ORG 0x7C00

; Constants
KERNEL_OFFSET equ 0x1000      ; Load kernel at 4KB
STAGE2_OFFSET equ 0x7E00      ; Stage 2 loader

start:
    ; Setup segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00            ; Stack grows downward from boot sector
    
    ; Clear screen and print boot message
    call clear_screen
    mov si, msg_boot
    call print_string
    
    ; Load stage 2 bootloader
    mov si, msg_stage2
    call print_string
    call load_stage2
    
    ; Jump to stage 2
    jmp STAGE2_OFFSET

;; Load Stage 2 bootloader from disk
load_stage2:
    mov ah, 0x02              ; BIOS read sectors
    mov al, 4                 ; Read 4 sectors (2KB for stage 2)
    mov ch, 0                 ; Cylinder 0
    mov cl, 2                 ; Sector 2 (sector 1 is boot sector)
    mov dh, 0                 ; Head 0
    mov bx, STAGE2_OFFSET     ; Destination
    int 0x13                  ; Call BIOS
    
    jc disk_error
    ret

;; Load kernel from disk
load_kernel:
    mov ah, 0x02
    mov al, 32                ; Read 32 sectors (~16KB kernel)
    mov ch, 0
    mov cl, 6                 ; Start after stage 2
    mov dh, 0
    mov bx, KERNEL_OFFSET
    int 0x13
    
    jc disk_error
    ret

;; Clear screen
clear_screen:
    mov ah, 0x00
    mov al, 0x03
    int 0x10
    ret

;; Print string (SI points to null-terminated string)
print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    popa
    ret

disk_error:
    mov si, msg_disk_error
    call print_string
    jmp $                     ; Hang

; Messages
msg_boot db 'NeuronOS Bootloader v1.0', 13, 10, 0
msg_stage2 db 'Loading Stage 2...', 13, 10, 0
msg_disk_error db 'DISK ERROR!', 13, 10, 0

; Boot sector signature
times 510-($-$$) db 0
dw 0xAA55
