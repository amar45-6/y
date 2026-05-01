#include "syscall.h"

int main() {
    int result = Abs(-42);
    PrintNum(result);   // should print 42
    Halt();
}
