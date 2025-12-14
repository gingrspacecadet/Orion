int main() {
    asm("    mov r0, #0x10\n    mov r1, r0");
    asm("\l");
    return 0;
}