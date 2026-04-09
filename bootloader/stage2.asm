; NeuronOS Bootloader - Stage 2
; Enables A20, sets up GDT, switches to protected mode, loads kernel

BITS 16
ORG 0x7E00

stage2_start:
    mov si, msg_stage2_start
    call print_string
    
    ; Get memory map using E820
    call detect_memory
    
    ; Enable A20 line
    mov si, msg_a20
    call print_string
    call enable_a20
    
    ; Load kernel
    mov si, msg_kernel
    call print_string
    call load_kernel_stage2
    
    ; Switch to protected mode
    mov si, msg_protected
    call print_string
    cli
    lgdt [gdt_descriptor]
    
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to flush pipeline and enter 32-bit protected mode temporarily
    jmp CODE_SEG:protected_mode_start

;; Detect memory using BIOS E820 + Build System Profile for AI
detect_memory:
    xor ebx, ebx
    mov di, 0x8000            ; Store memory map at 0x8000
    mov edx, 0x534D4150       ; 'SMAP' signature
    xor cx, cx                ; Entry counter
.loop:
    mov eax, 0xE820
    push ecx
    mov ecx, 24
    int 0x15
    pop ecx
    jc .done
    
    inc cx                    ; Count entries
    add di, 24
    test ebx, ebx
    jnz .loop
.done:
    ; Store entry count for kernel
    mov word [0x7FFE], cx
    
    ; Build system profile at 0x7000 for AI engine
    call build_system_profile
    ret

;; Build system profile for AI-driven boot decisions
build_system_profile:
    mov di, 0x7000
    
    ; Total RAM size (from last E820 entry)
    mov eax, [0x8000 + 16]    ; Base address high
    mov [di], eax
    
    ; CPU features (CPUID)
    mov eax, 1
    cpuid
    mov [di + 4], edx         ; Feature flags
    
    ; Boot timestamp (simplified - tick count)
    xor eax, eax
    int 0x1A                  ; Get tick count
    mov [di + 8], edx
    
    ; Profile complete marker
    mov dword [di + 12], 0x41495359  ; 'AISY' (AI SYstem)
    
    ret

;; Enable A20 line (method: keyboard controller)
enable_a20:
    call a20_wait
    mov al, 0xAD
    out 0x64, al
    
    call a20_wait
    mov al, 0xD0
    out 0x64, al
    
    call a20_wait2
    in al, 0x60
    push ax
    
    call a20_wait
    mov al, 0xD1
    out 0x64, al
    
    call a20_wait
    pop ax
    or al, 2
    out 0x60, al
    
    call a20_wait
    mov al, 0xAE
    out 0x64, al
    
    call a20_wait
    ret

a20_wait:
    in al, 0x64
    test al, 2
    jnz a20_wait
    ret

a20_wait2:
    in al, 0x64
    test al, 1
    jz a20_wait2
    ret

;; Load kernel
load_kernel_stage2:
    mov ah, 0x02
    mov al, 64                ; Read 64 sectors (32KB kernel)
    mov ch, 0
    mov cl, 6
    mov dh, 0
    mov bx, KERNEL_OFFSET
    int 0x13
    jc disk_error_stage2
    ret

disk_error_stage2:
    mov si, msg_disk_error
    call print_string
    jmp $

; 16-bit helper functions
print_string:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop
.done:
    popa
    ret

; GDT (Global Descriptor Table) - 32 Bit for Transition
gdt_start:
    dq 0 ; Null
gdt_code:
    dw 0xFFFF, 0, 0x9A00, 0x00CF ; Code
gdt_data:
    dw 0xFFFF, 0, 0x9200, 0x00CF ; Data
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; GDT for 64-bit Long Mode
gdt64_start:
    dq 0 ; Null
gdt64_code:
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; Executable, User Segment, Present, 64-bit
gdt64_data:
    dq (1<<41) | (1<<44) | (1<<47)           ; Writable, User Segment, Present
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64_start - 1
    dq gdt64_start

; Protected mode code (32-bit)
BITS 32
protected_mode_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Set up 4-level paging (PML4) at 0x10000
    mov edi, 0x10000 
    mov cr3, edi 
    xor eax, eax
    mov ecx, 4096
    rep stosd
    mov edi, 0x10000
    
    ; PML4T[0] -> PDPT
    mov dword [edi], 0x11003
    ; PDPT[0] -> PDT
    mov dword [edi + 0x1000], 0x12003
    ; PDT[0] -> PT
    mov dword [edi + 0x2000], 0x13003
    
    ; Identity map first 2MB
    mov edi, 0x13000
    mov ebx, 0x00000003
    mov ecx, 512
.set_entry:
    mov dword [edi], ebx
    add ebx, 0x1000
    add edi, 8
    loop .set_entry

    ; Enable PAE Support
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    ; Enable Long Mode (LME) in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable Paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    ; Load 64-bit GDT
    lgdt [gdt64_ptr]
    
    ; Far jump to Long Mode
    jmp 0x08:long_mode_start

BITS 64
long_mode_start:
    ; Setup segments for 64-bit
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Jump to kernel entry point (assumed Long Mode compiled)
    mov rax, KERNEL_OFFSET
    jmp rax

;; Print string in protected mode (VGA text mode)
print_string_pm:
    pusha
    mov ah, 0x0F              ; White on black
.loop:
    lodsb
    test al, al
    jz .done
    mov [edi], ax
    add edi, 2
    jmp .loop
.done:
    popa
    ret

; Messages
msg_stage2_start db 'Stage 2 Bootloader', 13, 10, 0
msg_a20 db 'Enabling A20...', 13, 10, 0
msg_kernel db 'Loading kernel...', 13, 10, 0
msg_protected db 'Switching to protected mode...', 13, 10, 0
msg_disk_error db 'DISK ERROR IN STAGE 2!', 13, 10, 0
msg_protected_mode db 'Protected mode active. Jumping to kernel...', 0

KERNEL_OFFSET equ 0x1000

times 2048-($-$$) db 0        ; Pad to 2KB (4 sectors)
