# Task 4) Pipe System Call Implementation in NachOS

## Overview
Implemented the `Pipe` system call in NachOS that connects two user programs via a temporary file buffer — running `cmd1` (producer) first, piping its output to `cmd2` (consumer), mimicking Unix-style pipe behavior inside the NachOS MIPS simulator.

---

## Files Modified
| File | Change |
|------|--------|
| `code/userprog/syscall.h` | Added SC_Pipe number and declaration |
| `code/userprog/ksyscall.h` | Added SysPipe kernel implementation |
| `code/userprog/exception.cc` | Added handler function and switch case |
| `code/test/start.S` | Replaced old stubs with Pipe assembly stub |
| `code/test/Makefile` | Added build rules for pipe_writer, pipe_reader, pipe_test |
| `code/test/pipe_writer.c` | Producer — writes integers into pipe buffer |
| `code/test/pipe_reader.c` | Consumer — reads integers and prints their squares |
| `code/test/pipe_test.c` | Test harness |

---

## Implementation

### 1. `code/userprog/syscall.h`
```c
#define SC_Pipe 58
int Pipe(char *cmd1, char *cmd2);
```

### 2. `code/userprog/ksyscall.h`
```c
int SysPipe(char* cmd1, char* cmd2) {
    if (!kernel->fileSystem->Create("__pipe_buf__")) {
        DEBUG(dbgSys, "\nPipe: Cannot create pipe buffer file.");
        return -1;
    }
    OpenFile* f1 = kernel->fileSystem->Open(cmd1);
    if (f1 == NULL) {
        DEBUG(dbgSys, "\nPipe: Cannot open cmd1 executable.");
        kernel->fileSystem->Remove("__pipe_buf__");
        return -1;
    }
    delete f1;
    int pid1 = kernel->pTab->ExecUpdate(cmd1);
    if (pid1 < 0) {
        DEBUG(dbgSys, "\nPipe: Cannot exec cmd1.");
        kernel->fileSystem->Remove("__pipe_buf__");
        return -1;
    }
    kernel->pTab->JoinUpdate(pid1);
    OpenFile* f2 = kernel->fileSystem->Open(cmd2);
    if (f2 == NULL) {
        DEBUG(dbgSys, "\nPipe: Cannot open cmd2 executable.");
        kernel->fileSystem->Remove("__pipe_buf__");
        return -1;
    }
    delete f2;
    int pid2 = kernel->pTab->ExecUpdate(cmd2);
    if (pid2 < 0) {
        DEBUG(dbgSys, "\nPipe: Cannot exec cmd2.");
        kernel->fileSystem->Remove("__pipe_buf__");
        return -1;
    }
    int result = kernel->pTab->JoinUpdate(pid2);
    kernel->fileSystem->Remove("__pipe_buf__");
    return result;
}
```

### 3. `code/userprog/exception.cc`
```cpp
void handle_SC_Pipe() {
    int virtAddr1 = kernel->machine->ReadRegister(4);
    int virtAddr2 = kernel->machine->ReadRegister(5);
    char* cmd1 = stringUser2System(virtAddr1);
    char* cmd2 = stringUser2System(virtAddr2);
    if (cmd1 == NULL || cmd2 == NULL) {
        DEBUG(dbgSys, "\nPipe: Not enough memory in System");
        kernel->machine->WriteRegister(2, -1);
        if (cmd1) delete[] cmd1;
        if (cmd2) delete[] cmd2;
        return move_program_counter();
    }
    kernel->machine->WriteRegister(2, SysPipe(cmd1, cmd2));
    return move_program_counter();
}
```

In the switch-case inside `ExceptionHandler()`:
```cpp
case SC_Pipe:
    return handle_SC_Pipe();
```

### 4. `code/test/start.S`
```asm
	.globl Pipe
	.ent	Pipe
Pipe:
	addiu $2,$0,SC_Pipe
	syscall
	j	$31
	.end Pipe
```

### 5. `code/test/Makefile`
Added `pipe_writer pipe_reader pipe_test` to `PROGRAMS`, then:
```makefile
pipe_writer.o: pipe_writer.c
	$(CC) $(CFLAGS) -c pipe_writer.c
pipe_writer: pipe_writer.o start.o
	$(LD) $(LDFLAGS) start.o pipe_writer.o -o pipe_writer.coff
	$(COFF2NOFF) pipe_writer.coff pipe_writer

pipe_reader.o: pipe_reader.c
	$(CC) $(CFLAGS) -c pipe_reader.c
pipe_reader: pipe_reader.o start.o
	$(LD) $(LDFLAGS) start.o pipe_reader.o -o pipe_reader.coff
	$(COFF2NOFF) pipe_reader.coff pipe_reader

pipe_test.o: pipe_test.c
	$(CC) $(CFLAGS) -c pipe_test.c
pipe_test: pipe_test.o start.o
	$(LD) $(LDFLAGS) start.o pipe_test.o -o pipe_test.coff
	$(COFF2NOFF) pipe_test.coff pipe_test
```

### 6. `code/test/pipe_writer.c`
```c
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
        while (tmp > 0) { rev[j++] = '0' + (tmp % 10); tmp /= 10; }
        while (j > 0)   { buf[i++] = rev[--j]; }
    }
    buf[i++] = '\n';
    Write(buf, i, fd);
}

int main() {
    OpenFileId fd;
    int nums[5];
    int i;
    nums[0]=3; nums[1]=7; nums[2]=12; nums[3]=5; nums[4]=20;

    fd = Open("__pipe_buf__", 0);
    if (fd < 0) { PrintString("pipe_writer: cannot open pipe buffer\n"); Exit(1); }

    PrintString("pipe_writer: writing numbers 3 7 12 5 20\n");
    for (i = 0; i < 5; i++) writeInt(fd, nums[i]);

    Close(fd);
    Exit(0);
    return 0;
}
```

### 7. `code/test/pipe_reader.c`
```c
#include "syscall.h"

int readLine(OpenFileId fd, char* buf, int maxLen) {
    int i = 0; char c; int n;
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
    int val = 0, i = 0;
    while (s[i] >= '0' && s[i] <= '9') { val = val * 10 + (s[i] - '0'); i++; }
    return val;
}

int main() {
    OpenFileId fd;
    char line[32];
    int num, square;

    fd = Open("__pipe_buf__", 1);
    if (fd < 0) { PrintString("pipe_reader: cannot open pipe buffer\n"); Exit(1); }

    PrintString("pipe_reader: reading and squaring each number\n");
    while (1) {
        int len = readLine(fd, line, 32);
        if (len == 0) break;
        num = parseInt(line);
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
```

### 8. `code/test/pipe_test.c`
```c
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
```

> **Note:** `pipe_writer` and `pipe_reader` use `Exit(0)` — not `Halt()`. Using `Halt()` in a child process shuts down the entire NachOS machine immediately. `Exit(0)` terminates only the current process and signals the parent via the join semaphore.

---

## How It Works

```
User: Pipe("pipe_writer", "pipe_reader")
  → SC_Pipe trap (syscall number 58)
  → handle_SC_Pipe() reads R4 = addr(cmd1), R5 = addr(cmd2)
  → stringUser2System() copies both strings from user space to kernel
  → SysPipe("pipe_writer", "pipe_reader")
  → fileSystem->Create("__pipe_buf__") creates temp buffer file
  → ExecUpdate("pipe_writer") spawns producer as new NachOS process
  → JoinUpdate(pid1) blocks pipe_test until pipe_writer exits
  → pipe_writer opens __pipe_buf__, writes "3\n7\n12\n5\n20\n", calls Exit(0)
  → ExecUpdate("pipe_reader") spawns consumer as new NachOS process
  → JoinUpdate(pid2) blocks pipe_test until pipe_reader exits
  → pipe_reader opens __pipe_buf__, reads each number, squares it, prints result
  → fileSystem->Remove("__pipe_buf__") cleans up temp file
  → returns exit code of pipe_reader back to pipe_test
```

1. User calls `Pipe("pipe_writer", "pipe_reader")` — triggers syscall trap with `SC_Pipe = 58`
2. `handle_SC_Pipe()` reads virtual addresses of `cmd1` and `cmd2` from registers R4 and R5, copies both strings from user space into kernel memory using `stringUser2System()`
3. `SysPipe()` calls `fileSystem->Create("__pipe_buf__")` — creates a temporary file that acts as the pipe buffer
4. `ExecUpdate("pipe_writer")` spawns `pipe_writer` as a child process, then `JoinUpdate(pid1)` blocks `pipe_test` until the producer finishes
5. `pipe_writer` runs — opens `__pipe_buf__`, writes the integers `3 7 12 5 20` one per line as ASCII, closes the file, and calls `Exit(0)`
6. `ExecUpdate("pipe_reader")` spawns `pipe_reader` as a child process, then `JoinUpdate(pid2)` blocks `pipe_test` until the consumer finishes
7. `pipe_reader` runs — opens `__pipe_buf__`, reads each line, parses the integer, computes its square, prints `n^2 = result` using `PrintNum()`, and calls `Exit(0)`
8. `fileSystem->Remove("__pipe_buf__")` deletes the temporary buffer, and `SysPipe()` returns the exit code of `pipe_reader`

---

## How to Run
```bash
# Build test programs
cd nachos-project-master/code/test
make pipe_writer pipe_reader pipe_test

# Build NachOS kernel
cd ../build.linux
make

# Copy binaries to working directory (nachos looks here at runtime)
cp ../test/pipe_writer ../test/pipe_reader .

# Run
./nachos -x ../test/pipe_test
```

## Output
```
Running Pipe(pipe_writer, pipe_reader)...
pipe_writer: writing numbers 3 7 12 5 20
pipe_reader: reading and squaring each number
3^2 = 9
7^2 = 49
12^2 = 144
5^2 = 25
20^2 = 400
Pipe done.
Machine halting!
```
<img width="1138" height="403" alt="image" src="https://github.com/user-attachments/assets/0631c55c-5351-4440-ab21-bf99f1963a1c" />


## Output Explanation

**Line 1** — `Running Pipe(pipe_writer, pipe_reader)...`
Printed by `pipe_test.c` before the `Pipe()` syscall. Confirms the test program started.

**Lines 2–8** — producer and consumer output
`pipe_writer` prints its status line then writes the integers into `__pipe_buf__`. After it exits, `pipe_reader` reads each integer back, squares it, and prints the result — proving `cmd2` is genuinely processing the data produced by `cmd1`, not just echoing it.

**Line 9** — `Pipe done.`
Printed by `pipe_test.c` after `Pipe()` returns. Confirms both child processes finished and control returned to the parent.

**Line 10** — `Machine halting!`
Printed by the NachOS kernel when `Halt()` is called in `pipe_test.c`. Simulation ended cleanly.

## Verification

The output is correct because:
- The integers `3 7 12 5 20` were written by `pipe_writer` into `__pipe_buf__` as ASCII text, and `pipe_reader` successfully read, parsed, and processed each one — proving data passed through the pipe buffer and was further computed upon.
- The correct squares (`9, 49, 144, 25, 400`) confirm `pipe_reader`'s integer parsing and arithmetic are working correctly.
- `Pipe done.` appearing after all squared values confirms sequential execution order — `pipe_writer` finished before `pipe_reader` ran, and `pipe_reader` finished before `pipe_test` continued.
- `Machine halting!` with no error confirms all processes exited cleanly via `Exit(0)` and only the final `Halt()` shut down the machine.
