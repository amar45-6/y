/* exec.c
 *	Simple program to test the exec system call.
 */

#include "syscall.h"

int main() {
    int pid;
    pid = Exec("halt");
    if (pid < 0) {
        PrintString("Exec failed\n");
    } else {
        Join(pid);
    }
    Exit(0);
}
