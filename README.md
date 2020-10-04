# Advanced_file_operations
Shared-library which recreates advanced file operations (fopen, fputc, popen etc.).
It includes buffering capabilities.

The repo contains two versions of the project:
- one for Windows, which, at build, creates the so_stdio.dll dynamic-link library;
- one for Linux, which, at build, creates the so_stdio.so shared object library.

The library recreates the following functions for files: fopen, fclose, fgetc, fputc,
fread, fwrite, fseek, ftell, fflush, feof, ferror.
It also allows for launching new processes with popen and pclose, via execl (Linux)/CreateProcess (WIN32).
