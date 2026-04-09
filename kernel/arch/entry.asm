; NeuronOS Kernel Entry Point
; kernel/arch/entry.asm
; Multiboot-compliant entry — GRUB loads this in 32-bit protected mode.
; gdt_flush / tss_flush / idt_flush / isr_stub_handler live in arch_asm.asm.
; switch_context is declared here because it is the primary context-switch path.

BITS 32

; ── Multiboot header ─────────────────────────────────────────────────────────
MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ 0x00000003   ; bit0=align to 4K, bit1=provide memory map
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

KERNEL_STACK_SIZE  equ 16384       ; 16 KB initial kernel stack

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

; ── BSS (stack) ──────────────────────────────────────────────────────────────
section .bss
align 16
kernel_stack_bottom:
    resb KERNEL_STACK_SIZE
kernel_stack_top:

; ── Entry ────────────────────────────────────────────────────────────────────
section .text

global kernel_entry
extern kernel_main

kernel_entry:
    ; GRUB guarantees: EAX = 0x2BADB002, EBX = multiboot info ptr.
    ; Interrupts are disabled by GRUB.

    mov  esp, kernel_stack_top   ; install our kernel stack

    ; Pass Multiboot args as C function arguments (cdecl: pushed right-to-left)
    push ebx                    ; multiboot_info_t*  (arg2 — unused by kernel_main for now)
    push eax                    ; multiboot magic    (arg1)

    call kernel_main            ; never returns under normal operation

.hang:
    cli
    hlt
    jmp .hang

; ── Context switch ───────────────────────────────────────────────────────────
; void switch_context(cpu_context_t* old_ctx, cpu_context_t* new_ctx)
; Saves caller-saved registers into *old_ctx, restores from *new_ctx.
global switch_context
switch_context:
    mov  eax, [esp+4]       ; old_ctx  pointer
    mov  ecx, [esp+8]       ; new_ctx  pointer

    ; --- save old context ---
    mov  [eax+4],  ebx
    mov  [eax+12], edx
    mov  [eax+16], esi
    mov  [eax+20], edi
    mov  [eax+24], ebp
    mov  [eax+28], esp
    mov  ebx, [esp]         ; return address = saved EIP
    mov  [eax+32], ebx
    pushfd
    pop  dword [eax+36]

    ; --- restore new context ---
    mov  ebx, [ecx+4]
    mov  edx, [ecx+12]
    mov  esi, [ecx+16]
    mov  edi, [ecx+20]
    mov  ebp, [ecx+24]
    mov  esp, [ecx+28]
    push dword [ecx+36]
    popfd
    mov  eax, [ecx+32]      ; jump to saved EIP
    jmp  eax
