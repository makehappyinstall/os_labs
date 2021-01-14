%define MAKE_GDT_DESC(base, limit, access, flags) \
    (((base & 0x00FFFFFF) << 16) | \
    ((base & 0xFF000000) << 32) | \
    (limit & 0x0000FFFF) | \
    ((limit & 0x00FF0000) << 32) | \
    ((access & 0xFF) << 40) | \
    ((flags & 0x0F) << 52))

gdt_start:
    dq MAKE_GDT_DESC(0, 0, 0, 0); null descriptor

gdt_code:
    dq MAKE_GDT_DESC(0, 0x0Fffffff, 10011010b, 11001111b)
                                ; 32-bit code, 4kb gran, limit 0xffffffff bytes, base=0
gdt_data:
    dq MAKE_GDT_DESC(0, 0x0Fffffff, 10010010b, 11001111b)
                                ; 32-bit data, 4kb gran, limit 0xffffffff bytes, base=0
end_of_gdt:

gdt_descriptor:
    dw end_of_gdt - gdt_start - 1
                                ; limit (Size of GDT - 1)
    dd gdt_start                ; base of GDT

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start