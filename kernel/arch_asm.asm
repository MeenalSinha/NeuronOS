; NeuronOS Architecture-Specific Assembly Rules
; kernel/arch_asm.asm
global gdt_flush
global tss_flush
global idt_flush
global isr_stub_handler

section .text

; void gdt_flush(uint32_t pointer)
gdt_flush:
    mov eax, [esp+4]    ; Get the pointer to the GDT
    lgdt [eax]          ; Load the new GDT
    
    mov ax, 0x10        ; 0x10 is the offset in the GDT to our kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    jmp 0x08:.flush     ; 0x08 is the offset to our code segment
.flush:
    ret

; void tss_flush(void)
tss_flush:
    mov ax, 0x2B        ; 0x2B is the offset in the GDT to our TSS (+3 for RPL)
    ltr ax              ; Load Task Register
    ret

; void idt_flush(uint32_t pointer)
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

; Generic ISR stub — Halt the CPU on any exception for now
isr_stub_handler:
    cli
.hang:
    hlt
    jmp .hang
