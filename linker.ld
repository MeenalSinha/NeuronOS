/* NeuronOS Linker Script — Multiboot/ELF32 */
/* Kernel is loaded by GRUB at 1MB physical (0x100000)  */

ENTRY(kernel_entry)

SECTIONS
{
    /* Load at 1MB — standard Multiboot load address */
    . = 0x100000;

    .text ALIGN(4096) :
    {
        /* Multiboot header must be in first 8KB */
        *(.multiboot)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4096) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Discard debug/exception table sections */
    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note.gnu.build-id)
    }
}
