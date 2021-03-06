#ifndef STDIO_INTERNAL_H
#define STDIO_INTERNAL_H

#define DLL_EXPORTS
#define BUFFCAPACIT	4096
#define READ		0
#define READPLUS	1
#define WRITE		2
#define WRITEPLUS	3
#define APPEND		4
#define APPENDPLUS	5
#define LASTREAD	0
#define LASTWRITE	1
#define PIPE_READ	0
#define PIPE_WRITE	1
#define COMMSIZE	128

#include "stdio.h"
#include "stdlib.h"
#include "so_stdio.h"

#include <errno.h>

struct _so_file {
	HANDLE fd;
	HANDLE parent;
	HANDLE child;
	long pointer;
	int mode_type;
	unsigned char buffer[BUFFCAPACIT];
	DWORD buff_size;
	int last_op;
	BOOL found_eof;
	BOOL is_proc;
	int found_error;
	int buff_pos;
};

#endif /* STDIO_INTERNAL_H */
