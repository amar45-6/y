#include "syscall.h"

int main() {
    int before, after, slept;

    before = GetTicks();
    Sleep(1000);
    after = GetTicks();

    slept = after - before;

    PrintString("Requested ticks : 1000\n");
    PrintString("Slept for ticks : ");
    PrintNum(slept);
    PrintString("\n");

    Halt();
}
