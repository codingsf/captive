ENTRY(entry16);
SECTIONS
{
    . = 0;
    .text :
    {
        _text = .;
        *(.text);
        _text_end = .;
    }

    .data :
    {
        _data = .;
        *(.bss);
        *(.bss*);
        *(.data);
        *(.rodata*);
        *(COMMON)
        _data_end = .;
    }

    . = 0xfff0;
    .reset_vector :
    {
        *(.reset_vector);
    }

    /DISCARD/ :
    {
        *(.note*);
        *(.iplt*);
        *(.igot*);
        *(.rel*);
        *(.comment);
        *(.debug*);
    }
}
