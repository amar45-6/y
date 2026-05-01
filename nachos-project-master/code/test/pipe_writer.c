/* pipe_writer.c
 * cmd1: writes a series of integers (one per line) into __pipe_buf__
 * cmd2 (pipe_reader) will read them and compute their squares.
 */
#include "syscall.h"

void writeInt(OpenFileId fd, int n) {
    char buf[12];
    int i = 0;
    int tmp = n;

    if (tmp == 0) {
        buf[i++] = '0';
    } else {
        char rev[12];
        int j = 0;
        while (tmp > 0) {
            rev[j++] = '0' + (tmp % 10);
            tmp /= 10;
        }
        while (j > 0) {
            buf[i++] = rev[--j];
        }
    }
    buf[i++] = '\n';
    Write(buf, i, fd);
}

int main() {
    OpenFileId fd;
    int nums[5];
    int i;

    nums[0] = 3;
    nums[1] = 7;
    nums[2] = 12;
    nums[3] = 5;
    nums[4] = 20;

    fd = Open("__pipe_buf__", 0);
    if (fd < 0) {
        PrintString("pipe_writer: cannot open pipe buffer\n");
        Exit(1);
    }

    PrintString("pipe_writer: writing numbers 3 7 12 5 20\n");
    for (i = 0; i < 5; i++) {
        writeInt(fd, nums[i]);
    }

    Close(fd);
    Exit(0);
    return 0;
}
