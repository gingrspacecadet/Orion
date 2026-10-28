SECTIONS
{
    .tests ALIGN(8) :
    {
        __tests_start = .;
        KEEP(*(tests))
        __tests_end = .;
    }
}
INSERT AFTER .rodata;