# Task 1) Abs System Call Implementation in NachOS

## Overview
Implemented the `Abs` system call in NachOS that computes the absolute value of an integer passed from a user program running inside the NachOS MIPS simulator.

---

## Files Modified

| File | Change |
|------|--------|
| `code/userprog/syscall.h` | Added SC_Abs number and declaration |
| `code/userprog/ksyscall.h` | Added SysAbs kernel implementation |
| `code/userprog/exception.cc` | Added handler function and switch case |
| `code/test/start.S` | Added MIPS assembly stub |
| `code/test/Makefile` | Added build rules for abs |
| `code/test/abs.c` | New test program |

---

## Implementation

### 1. `code/userprog/syscall.h`
```c
#define SC_Abs 55

int Abs(int val);
```

### 2. `code/userprog/ksyscall.h`
```c
#include <stdint.h>   // added to fix INT32_MIN error

int SysAbs(int val) { return val < 0 ? -val : val; }
```

### 3. `code/userprog/exception.cc`
```cpp
void handle_SC_Abs() {
    int val = (int)kernel->machine->ReadRegister(4);
    int result = SysAbs(val);
    kernel->machine->WriteRegister(2, result);
    return move_program_counter();
}
```
In the switch-case inside `ExceptionHandler()`:
```cpp
case SC_Abs:
    return handle_SC_Abs();
```

### 4. `code/test/start.S`
```asm
	.globl Abs
	.ent	Abs
Abs:
	addiu $2,$0,SC_Abs
	syscall
	j	$31
	.end Abs
```

### 5. `code/test/Makefile`
Added `abs` to `PROGRAMS`, then:
```makefile
abs.o: abs.c
	$(CC) $(CFLAGS) -c abs.c

abs: abs.o start.o
	$(LD) $(LDFLAGS) start.o abs.o -o abs.coff
	$(COFF2NOFF) abs.coff abs
```

### 6. `code/test/abs.c`
```c
#include "syscall.h"

int main() {
    int result = Abs(-42);
    PrintNum(result);
    Halt();
}
```

---

## Setup Issues & Fixes

- **Missing MIPS compiler symlinks** — Makefile expected `mips-decstation-ultrix-*` but binaries were named `decstation-ultrix-*`. Fixed with:
```bash
cd /home/ssl56/oslab/usr/local/nachos/bin
ln -s decstation-ultrix-gcc    mips-decstation-ultrix-gcc
ln -s decstation-ultrix-ld     mips-decstation-ultrix-ld
ln -s decstation-ultrix-as     mips-decstation-ultrix-as
ln -s decstation-ultrix-ar     mips-decstation-ultrix-ar
ln -s decstation-ultrix-ranlib mips-decstation-ultrix-ranlib
```

- **Missing coff2noff binary** — Built from source:
```bash
cd nachos-project-master/coff2noff
make
mkdir -p ../code/coff2noff
cp coff2noff.x86Linux ../code/coff2noff/
```

- **`INT32_MIN` undeclared** — Added `#include <stdint.h>` to `ksyscall.h`.

---

## How to Run
```bash
# Build test program
cd nachos-project-master/code/test
make abs

# Build NachOS kernel
cd ../build.linux
make

# Run
./nachos -x ../test/abs
```

## Output
```
42Machine halting!
```
`Abs(-42)` returns `42` — system call works end-to-end.<img width="766" height="270" alt="Screenshot from 2026-02-25 16-19-48" src="https://github.com/user-attachments/assets/4b7531cb-8537-461a-9e2b-65ea97187bbe" />


# Task2) Priority Scheduler Implementation in NachOS

## Overview
Replaced the default FIFO scheduler with a priority-based scheduler.
Threads with higher priority values run first.

---

## Files Modified

| File | Change |
|------|--------|
| `code/threads/thread.h` | Added `priority` field, `getPriority()`, `setPriority()` |
| `code/threads/thread.cc` | Initialized `priority = 0` in constructor |
| `code/threads/scheduler.h` | Changed `List` to `SortedList` |
| `code/threads/scheduler.cc` | Added `PriorityCompare`, used `SortedList`, changed `Append` to `Insert` |
| `code/threads/kernel.cc` | Added `PriorityTestThread` and test in `ThreadSelfTest()` |

---

## Implementation

### 1. `code/threads/thread.h`
```cpp
// in private section
int priority;

// in public section
void setPriority(int p) { priority = p; }
int getPriority() { return priority; }
```

### 2. `code/threads/thread.cc`
```cpp
// in constructor
priority = 0;  // default priority
```

### 3. `code/threads/scheduler.h`
```cpp
// replace:
List<Thread*>* readyList;
// with:
SortedList<Thread*>* readyList;
```

### 4. `code/threads/scheduler.cc`
```cpp
// comparator - higher priority goes to front of ready list
static int PriorityCompare(Thread* a, Thread* b) {
    if (a->getPriority() > b->getPriority()) return -1;
    if (a->getPriority() < b->getPriority()) return 1;
    return 0;
}

// constructor
Scheduler::Scheduler() {
    readyList = new SortedList<Thread*>(PriorityCompare);
    toBeDestroyed = NULL;
}

// ReadyToRun - Insert instead of Append
readyList->Insert(thread);
```

### 5. `code/threads/kernel.cc`
```cpp
// add above ThreadSelfTest()
static void PriorityTestThread(int which) {
    printf("*** thread priority %d running\n", which);
}

// add at end of ThreadSelfTest()
printf("\n--- Priority Scheduler Test ---\n");

IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);

Thread* t1 = new Thread("low-priority");
t1->setPriority(1);
t1->Fork((VoidFunctionPtr)PriorityTestThread, (void*)1);

Thread* t2 = new Thread("medium-priority");
t2->setPriority(5);
t2->Fork((VoidFunctionPtr)PriorityTestThread, (void*)5);

Thread* t3 = new Thread("high-priority");
t3->setPriority(10);
t3->Fork((VoidFunctionPtr)PriorityTestThread, (void*)10);

kernel->interrupt->SetLevel(oldLevel);
currentThread->Yield();
printf("--- Priority Scheduler Test Done ---\n");
```

---

## How It Works
- Every `Thread` has a `priority` field (default 0).
- `readyList` is a `SortedList` sorted by `PriorityCompare` — higher priority threads sit at the front.
- `FindNextToRun()` calls `RemoveFront()` which always returns the highest priority thread.
- Interrupts are disabled while forking all test threads so all 3 enter the ready list before any runs — this ensures the sorted order is respected.

---

## Build and Run
```bash
cd nachos-project-master/code/build.linux
make
./nachos -K
```

## Output
```
--- Priority Scheduler Test ---
*** thread priority 10 running
*** thread priority 5 running
*** thread priority 1 running
--- Priority Scheduler Test Done ---
```
<img width="878" height="545" alt="image" src="https://github.com/user-attachments/assets/d5c50227-bd2c-45bf-9dcf-1b5d713e6650" />

# Task 3: Sleep System Call Implementation in NachOS

## Overview
Implemented a `Sleep(int ticks)` system call that suspends the calling thread for a specified number of timer ticks. The thread truly blocks (releases the CPU) and is woken up by the timer interrupt handler once the requested time has elapsed.

---

## Files Modified

| File | Change |
|------|--------|
| `code/userprog/syscall.h` | Added `SC_Sleep 56`, `SC_GetTicks 57`, declarations `void Sleep(int)`, `int GetTicks()` |
| `code/threads/alarm.h` | Added `SleepEntry` struct and `List<SleepEntry*>* sleepList` member |
| `code/threads/alarm.cc` | Implemented `WaitUntil()` and updated `CallBack()` to wake sleeping threads |
| `code/userprog/ksyscall.h` | Added `SysSleep()` and `SysGetTicks()` kernel functions |
| `code/userprog/exception.cc` | Added `handle_SC_Sleep()`, `handle_SC_GetTicks()`, and cases in the switch |
| `code/test/start.S` | Added MIPS assembly stubs for `Sleep` and `GetTicks` |
| `code/test/Makefile` | Added build rules for `sleep_test` |
| `code/test/sleep_test.c` | New test program (created) |

---

## Implementation

### 1. `code/userprog/syscall.h`

Assign syscall numbers and declare user-facing functions:

```c
#define SC_Sleep    56
#define SC_GetTicks 57

void Sleep(int when);  // suspend for 'when' ticks
int  GetTicks();       // return current tick count
```

---

### 2. `code/threads/alarm.h`

Add a struct to track each sleeping thread and a list to hold them:

```cpp
struct SleepEntry {
    Thread *thread;  // thread to wake up
    int wakeTime;    // absolute tick when it should wake
};

class Alarm : public CallBackObj {
    ...
private:
    Timer *timer;
    List<SleepEntry *> *sleepList;  // ADD THIS
    void CallBack();
};
```

---

### 3. `code/threads/alarm.cc`

**Constructor** — initialise the sleep list:

```cpp
Alarm::Alarm(bool doRandom) {
    timer = new Timer(doRandom, this);
    sleepList = new List<SleepEntry *>();  // ADD THIS LINE
}
```

**WaitUntil()** — block the current thread for `x` ticks:

```cpp
void Alarm::WaitUntil(int x) {
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);

    SleepEntry *entry = new SleepEntry();
    entry->thread   = kernel->currentThread;
    entry->wakeTime = kernel->stats->totalTicks + x;
    sleepList->Append(entry);

    kernel->currentThread->Sleep(false);  // block the thread

    kernel->interrupt->SetLevel(oldLevel);
}
```

**CallBack()** — on every timer interrupt, wake threads whose time has come:

```cpp
void Alarm::CallBack() {
    Interrupt *interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();
    int now = kernel->stats->totalTicks;

    List<SleepEntry *> *remaining = new List<SleepEntry *>();
    while (!sleepList->IsEmpty()) {
        SleepEntry *entry = sleepList->RemoveFront();
        if (entry->wakeTime <= now) {
            kernel->scheduler->ReadyToRun(entry->thread);  // wake it
            delete entry;
        } else {
            remaining->Append(entry);
        }
    }
    while (!remaining->IsEmpty())
        sleepList->Append(remaining->RemoveFront());
    delete remaining;

    if (status != IdleMode)
        interrupt->YieldOnReturn();
}
```

---

### 4. `code/userprog/ksyscall.h`

Add kernel-side implementations:

```cpp
void SysSleep(int ticks) {
    if (ticks <= 0) {
        kernel->currentThread->Yield();
        return;
    }
    kernel->alarm->WaitUntil(ticks);
}

int SysGetTicks() {
    return kernel->stats->totalTicks;
}
```

---

### 5. `code/userprog/exception.cc`

Add handler functions before `ExceptionHandler`:

```cpp
void handle_SC_Sleep() {
    int ticks = (int)kernel->machine->ReadRegister(4);
    DEBUG(dbgSys, "Sleep called with ticks=" << ticks << "\n");
    SysSleep(ticks);
    kernel->machine->WriteRegister(2, 0);
    return move_program_counter();
}

void handle_SC_GetTicks() {
    kernel->machine->WriteRegister(2, SysGetTicks());
    return move_program_counter();
}
```

Add cases inside the switch in `ExceptionHandler`:

```cpp
case SC_Sleep:
    return handle_SC_Sleep();
case SC_GetTicks:
    return handle_SC_GetTicks();
```

---

### 6. `code/test/start.S`

Add MIPS assembly stubs **before the `__main` block**:

```asm
    .globl Sleep
    .ent   Sleep
Sleep:
    addiu $2,$0,SC_Sleep
    syscall
    j     $31
    .end  Sleep

    .globl GetTicks
    .ent   GetTicks
GetTicks:
    addiu $2,$0,SC_GetTicks
    syscall
    j     $31
    .end  GetTicks
```



---

### 7. `code/test/Makefile`

Add `sleep_test` to the PROGRAMS list:

```makefile
PROGRAMS = ... main sleep_test
```

Add build rules:

```makefile
sleep_test.o: sleep_test.c
	$(CC) $(CFLAGS) -c sleep_test.c
sleep_test: sleep_test.o start.o
	$(LD) $(LDFLAGS) start.o sleep_test.o -o sleep_test.coff
	$(COFF2NOFF) sleep_test.coff sleep_test
```

---

### 8. `code/test/sleep_test.c` (new file)

```c
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
```

---

## How It Works

```
User: Sleep(1000)
  → SC_Sleep trap (syscall number 56)
  → handle_SC_Sleep() reads R4 = 1000
  → SysSleep(1000)
  → alarm->WaitUntil(1000)
  → thread added to sleepList, Thread::Sleep(false) blocks it
  → CPU runs other threads or goes idle
  → timer fires every 100 ticks → Alarm::CallBack() runs
  → totalTicks >= wakeTime → scheduler->ReadyToRun(thread)
  → thread resumes, returns to user space
```

1. User calls `Sleep(1000)` — triggers syscall trap with `SC_Sleep = 56`
2. `handle_SC_Sleep()` reads the tick count from register R4
3. `SysSleep()` calls `alarm->WaitUntil(1000)`
4. `WaitUntil()` disables interrupts, computes `wakeTime = now + 1000`, appends to `sleepList`, then calls `Thread::Sleep(false)` to block
5. CPU is free — runs other threads or goes idle
6. Timer fires every 100 ticks, triggering `Alarm::CallBack()`
7. `CallBack()` scans `sleepList` — when `totalTicks >= wakeTime`, calls `scheduler->ReadyToRun(thread)`
8. Thread resumes and returns to user space

---

## Build and Run

**Build the kernel:**

```bash
cd nachos-project-master/code/build.linux
make
```

**Build the test program:**

```bash
cd nachos-project-master/code/test
make sleep_test
```

**Run:**

```bash
cd nachos-project-master/code/build.linux
./nachos -x ../test/sleep_test
```

---

## Output

<img width="1011" height="660" alt="image" src="https://github.com/user-attachments/assets/9c1d3ed7-0e6b-4bc1-b78c-683924046066" />

---

## Verification

| Metric | Value | Meaning |
|--------|-------|---------|
| Requested sleep | 1000 ticks | Argument passed to `Sleep()` |
| Actual sleep | 1074 ticks | Woke at next timer interrupt after 1000 |
| Overshoot | 74 ticks | Always < 100 (one timer interval) ✅ |
| Idle ticks | 5641 / 7286 total | CPU was free while thread slept ✅ |

The overshoot is always between **0 and 100 ticks** because NachOS timer interrupts fire every 100 ticks — the thread wakes at the first interrupt after the requested duration.

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
