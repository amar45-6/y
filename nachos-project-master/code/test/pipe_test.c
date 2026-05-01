/* pipe_test.c
 * Test harness: runs Pipe(pipe_writer, pipe_reader).
 *
 * Expected output:
 *   pipe_writer: writing numbers 3 7 12 5 20
 *   pipe_reader: reading and squaring each number
 *   3^2 = 9
 *   7^2 = 49
 *   12^2 = 144
 *   5^2 = 25
 *   20^2 = 400
 */
#include "syscall.h"

int main() {
    int result;
    PrintString("Running Pipe(pipe_writer, pipe_reader)...\n");
    result = Pipe("pipe_writer", "pipe_reader");
    if (result >= 0)
        PrintString("Pipe done.\n");
    else
        PrintString("Pipe failed.\n");
    Halt();
    return 0;
}
