/* pipe_reader.c
 * cmd2: reads integers (one per line) from __pipe_buf__ and prints their squares.
 */
#include "syscall.h"

int readLine(OpenFileId fd, char* buf, int maxLen) {
    int i = 0;
    char c;
    int n;
    while (i < maxLen - 1) {
        n = Read(&c, 1, fd);
        if (n <= 0) break;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

int parseInt(char* s) {
    int val = 0;
    int i = 0;
    while (s[i] >= '0' && s[i] <= '9') {
        val = val * 10 + (s[i] - '0');
        i++;
    }
    return val;
}

int main() {
    OpenFileId fd;
    char line[32];
    int num, square;

    fd = Open("__pipe_buf__", 1);
    if (fd < 0) {
        PrintString("pipe_reader: cannot open pipe buffer\n");
        Exit(1);
    }

    PrintString("pipe_reader: reading and squaring each number\n");

    while (1) {
        int len = readLine(fd, line, 32);
        if (len == 0) break;

        num    = parseInt(line);
        square = num * num;

        PrintNum(num);
        PrintString("^2 = ");
        PrintNum(square);
        PrintString("\n");
    }

    Close(fd);
    Exit(0);
    return 0;
}
