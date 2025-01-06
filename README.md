# Operating-Systems-Projects
This repository contains the 5 Operating Systems projects I created in C!
## Custom Shell 🐢
- Implemented a basic shell supporting:
    - command execution
    - input/output redirection
    - pipelining
    - background processes
- Created child processes with fork() for process handling and used dup2() for pipe management
- Used waitpid() for foreground tasks and signal handling for background tasks
- Used open() and related calls to manage file descriptors and seamlessly integrate redirection in the stream
- Thoroughly tested shell features with various commands, pipelines, and combinations of redirection to validate functionality

## Custom Thread Library + Thread Synchronization 🧵
- Designed a user-level threading library based on POSIX Pthreads, incorporating:
  - thread creation/termination
  - round-robin scheduling
  - joining
  - mutex operations
- Developed TCB structure to handle thread-specific data such as CPU registers and stack, supporting up to 128 threads
- Implemented round-robin scheduling mechanism through signal and timer handlers to ensure proper thread switching and preemption
- Integrated synchronization through mutex up and down functions and constructed a queue to hold waiting threads

## Thread Local Storage 📖
- Developed a thread-local storage (TLS) library leveraging block-based memory management methods including
    - copy on write functionality (CoW)
    - read/write
    - cloning
- Utilized mmap for memory allocation with page-alignment and used mprotect to control read/write permissions for thread safety
- Incorporated page fault handling and signal management to ensure different types of segfaults are handled correctly
- Performed extensive debugging using GDB (GNU debugger)

## File System 📂
- Implemented a 16 MB file system with operations (create, delete, read, write) with custom FAT-based structure
- Engineered a superblock to hold location data of the FAT and File Directory
- Wrote functions for file size retrieval, file listing, seeking, and truncating
- Gained hands-on experience in low-level file system architecture and block-level memory management
