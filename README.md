# BSDSF24A032-OS-A03 — myshell

A simple UNIX shell written in C for the Operating Systems course (PA-03).

## Build and run

```bash
make
./bin/myshell
```

`make clean` removes `obj/` and `bin/`.

## Structure

- `src/main.c` — main loop (read → tokenize → execute)
- `src/shell.c` — reading and tokenizing the command line
- `src/execute.c` — fork / exec / wait
- `include/shell.h` — shared header
