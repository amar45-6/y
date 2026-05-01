#include "syscall.h"

void low() {
    PrintString("LOW priority running\n");
    ThreadExit(0);
}

void medium() {
    PrintString("MEDIUM priority running\n");
    ThreadExit(0);
}

void high() {
    PrintString("HIGH priority running\n");
    ThreadExit(0);
}

int main() {
    // Fork in reverse order - low first, high last
    // Priority scheduler should still run high first
    ThreadFork(low);
    ThreadFork(medium);
    ThreadFork(high);

    ThreadYield();  // give up CPU so forked threads run
    PrintString("Main thread done\n");
    Halt();
}
