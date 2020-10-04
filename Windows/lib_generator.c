#include "stdio_internal.h"
#include <string.h>

/* Allocates and returns a new SO_FILE structure
 * Reading and writing permission coresponding to mode string
 * Returns NULL in case of error
 */
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	HANDLE hFile = NULL;
	int mode_type = -1;
	SO_FILE *file = NULL;
	long end_position = -1;

	if (strcmp(mode, "r") == 0) {
		hFile = CreateFile(pathname,
			GENERIC_READ,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		mode_type = READ;
	} else if (strcmp(mode, "r+") == 0) {
		hFile = CreateFile(pathname,
			GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		mode_type = READPLUS;
	} else if (strcmp(mode, "w") == 0) {
		hFile = CreateFile(pathname,
			GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		mode_type = WRITE;
	} else if (strcmp(mode, "w+") == 0) {
		hFile = CreateFile(pathname,
			GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		mode_type = WRITEPLUS;
	} else if (strcmp(mode, "a") == 0) {
		hFile = CreateFile(pathname,
			FILE_APPEND_DATA,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		mode_type = APPEND;
	} else if (strcmp(mode, "a+") == 0) {
		hFile = CreateFile(pathname,
			FILE_APPEND_DATA|FILE_READ_DATA,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		mode_type = APPENDPLUS;
	} else
		return NULL;

	if (hFile != INVALID_HANDLE_VALUE) {
		file = malloc(sizeof(SO_FILE));
		if (file != NULL) {
			file->fd = hFile;
			file->pointer = SetFilePointer(hFile, 0,
				NULL, FILE_CURRENT);
			file->mode_type = mode_type;
			memset(file->buffer, 0, sizeof(file->buffer));
			file->last_op = -1;
			file->found_eof = FALSE;
			file->is_proc = FALSE;
			file->found_error = -1;
			file->buff_pos = 0;
			file->buff_size = 0;
			file->parent = NULL;
			return file;
		}
		CloseHandle(hFile);
	}
	return NULL;
}

/* Frees memory for given SO_FILE and closes its handle
 * The function flushes to file the remaining elements
 * in the buffer of the SO_FILE
 * Returns 0 at success, SO_EOF in case of error
 */
int so_fclose(SO_FILE *stream)
{
	int ret = 0;
	BOOL bRet;

	if (stream->last_op == LASTWRITE)
		ret = so_fflush(stream);
	bRet = CloseHandle(stream->fd);
	free(stream);
	if (bRet == FALSE || ret < 0)
		return -1;
	else
		return 0;
}

/* Moves SO_FILE internal pointer, based on sum of
 * offset and whence
 * Offset could be a negative number
 * In case of a previous read operation, its content
 * is invalidated
 * A previous write operation will determine the content
 * of the buffer to be written to the file
 * Returns 0 at succes, -1 in case of error
 */
int so_fseek(SO_FILE *stream, long offset, int whence)
{
	DWORD bytes_written = 0;
	DWORD bytes_written_now = 0;
	BOOL bRet;

	if (stream->last_op == LASTREAD) {
		memset(stream->buffer, 0, sizeof(stream->buffer));
		stream->buff_pos = 0;
		stream->buff_size = 0;
	} else if (stream->last_op == LASTWRITE) {
		while (bytes_written < stream->buff_size) {
			bRet = WriteFile(stream->fd,
				stream->buffer + bytes_written,
				stream->buff_size - bytes_written,
				&bytes_written_now,
				NULL);

			if (bRet == FALSE) {
				stream->found_error = 1;
				return -1;
			}
			stream->pointer += bytes_written_now;
			bytes_written += bytes_written_now;
		}
		memset(stream->buffer, 0, sizeof(stream->buffer));
		stream->buff_pos = 0;
		stream->buff_size = 0;
	}

	stream->pointer = SetFilePointer(stream->fd,
					offset,
					NULL,
					whence);
	if (stream->pointer == -1) {
		stream->found_error = 1;
		return -1;
	} else {
		return 0;
	}
}

/* Returns the current position of the SO_FILE internal
 * pointer
 */
long so_ftell(SO_FILE *stream)
{
	if (stream->last_op == LASTWRITE)
		return (stream->pointer + stream->buff_pos);
	return stream->pointer;
}

/* Available only for a previous write operation, it
 * flushes the content of the SO_FILE buffer to the file
 * Returns 0 at succes, SO_EOF in case of error
 */
int so_fflush(SO_FILE *stream)
{
	DWORD bytes_written = 0;
	DWORD bytes_written_now = 0;
	BOOL bRet;

	if (stream->last_op != LASTWRITE) {
		stream->found_error = 1;
		return SO_EOF;
	}
	while (bytes_written < stream->buff_size) {
		bRet = WriteFile(stream->fd,
			stream->buffer + bytes_written,
			stream->buff_size - bytes_written,
			&bytes_written_now,
			NULL);

		if (bRet == FALSE) {
			stream->found_error = 1;
			return SO_EOF;
		}
		stream->pointer += bytes_written_now;
		bytes_written += bytes_written_now;
	}
	memset(stream->buffer, 0, sizeof(stream->buffer));
	stream->buff_pos = 0;
	stream->buff_size = 0;
	return 0;
}

/* Returns the handle associated with this SO_FILE */
HANDLE so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

/* Returns 1 if SO_FILE pointer is at EOF, 0 otherwise */
int so_feof(SO_FILE *stream)
{
	if (stream->found_eof == TRUE)
		return 1;
	else
		return 0;
}

/* Returns 1 if an operation on this SO_FILE determined
 * error, 0 otherwise
 */
int so_ferror(SO_FILE *stream)
{
	if (stream->found_error == -1)
		return 0;
	else
		return stream->found_error;
}

/* Reads a character from file
 * Uses the internal buffer to prefetch data
 * If there is no new data in the buffer, it reads as
 * much as BUFFCAPACIT more characters from file to buffer
 * Returns character at succes, SO_EOF in case of error
 * or if EOF found
 */
int so_fgetc(SO_FILE *stream)
{
	DWORD bytes_read = 0;
	DWORD last_error = 0;
	BOOL bRet;
	int c;

	if (so_ferror(stream) || so_feof(stream))
		return SO_EOF;

	if (stream->buff_size == 0 ||
		stream->buff_pos == stream->buff_size) {
		if (stream->buff_size != 0) {
			memset(stream->buffer, 0, sizeof(stream->buffer));
			stream->buff_pos = 0;
			stream->buff_size = 0;
		}
		bRet = ReadFile(stream->fd,
			stream->buffer,
			BUFFCAPACIT,
			&bytes_read,
			NULL);
		if (bRet == FALSE) {
			if (stream->is_proc == TRUE) {
				last_error = GetLastError();
				if (last_error == ERROR_BROKEN_PIPE) {
					stream->found_eof = TRUE;
					return SO_EOF;
				}
			}
			stream->found_error = 1;
			return SO_EOF;
		} else if (bytes_read == 0 && stream->is_proc == FALSE) {
			stream->found_eof = TRUE;
			return SO_EOF;
		}
		stream->buff_size = bytes_read;
	}

	stream->last_op = LASTREAD;
	c = stream->buffer[stream->buff_pos++];
	stream->pointer++;
	return c;
}

/* Reads size * nmemb bytes from the SO_FILE
 * Uses the internal buffer to prefetch data
 * If there is no new data in the buffer, it reads as much
 * as BUFFCAPACIT more characters from file to the buffer
 * Reads as much as the requested bytes at memory address pointed
 * by ptr at succes, returning the number of elements read
 * Returns 0 in case of error or if EOF found
 */
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t count = size * nmemb;
	int ret = 0;
	unsigned char c = 0;

	while (count > 0) {
		ret = so_fgetc(stream);
		if (so_ferror(stream))
			return 0;
		if (ret == SO_EOF)
			break;
		c = (unsigned char)ret;
		memcpy(((char *)ptr)++, &c, sizeof(unsigned char));
		count--;
	}

	stream->last_op = LASTREAD;
	return ((size * nmemb - count) / size);
}

/* Writes a character to file
 * Uses the internal buffer for buffering
 * Writes the content of the buffer to the file only
 * when buffer is full
 * Returns character at succes, SO_EOF in case of error
 */
int so_fputc(int c, SO_FILE *stream)
{
	unsigned char ch = (unsigned char) c;
	DWORD bytes_written = 0;
	DWORD bytes_written_now = 0;
	BOOL bRet;

	if (so_ferror(stream))
		return SO_EOF;

	if (stream->buff_size == BUFFCAPACIT) {
		while (bytes_written < stream->buff_size) {
			bRet = WriteFile(stream->fd,
				stream->buffer + bytes_written,
				stream->buff_size - bytes_written,
				&bytes_written_now,
				NULL);

			if (bRet == FALSE) {
				stream->found_error = 1;
				return SO_EOF;
			}
			stream->pointer += bytes_written_now;
			bytes_written += bytes_written_now;
		}
		memset(stream->buffer, 0, sizeof(stream->buffer));
		stream->buff_pos = 0;
		stream->buff_size = 0;
	}

	stream->last_op = LASTWRITE;
	stream->buffer[stream->buff_pos++] = ch;
	stream->buff_size++;
	return c;
}

/* Writes size * nmemb bytes to the SO_FILE
 * Uses the internal buffer for buffering
 * Writes the content of the buffer to the file only
 * when buffer is full
 * Writes as much as the requested bytes from memory address pointed
 * by ptr at succes, returning the number of elements written
 * Returns 0 in case of error
 */
size_t so_fwrite(const void *ptr, size_t size,
	size_t nmemb, SO_FILE *stream)
{
	size_t count = size * nmemb;
	int ret = 0;
	int i = 0;
	int c = 0;
	unsigned char ch = 0;

	while (count > 0) {
		memcpy(&ch, (((char *)ptr) + i++), sizeof(unsigned char));
		count--;
		c = (int)ch;
		ret = so_fputc(c, stream);
		if (ret == SO_EOF)
			return 0;
	}

	stream->last_op = LASTWRITE;
	return ((size * nmemb - count) / size);
}

/* Allocates and returns a new SO_FILE structure, creating
 * a new child process which runs the given command in terminal
 * Input OR output of child is redirected through a pipe
 * from/to the main process, based on type parameter
 * Returns NULL in case of error
 */
SO_FILE *so_popen(const char *command, const char *type)
{
	int mode_type = -1;
	SO_FILE *file = NULL;
	HANDLE child_in_read = NULL;
	HANDLE child_in_write = NULL;
	HANDLE child_out_read = NULL;
	HANDLE child_out_write = NULL;
	SECURITY_ATTRIBUTES sa;
	BOOL bRet;
	PROCESS_INFORMATION pi;
	STARTUPINFO psi;
	char comm[COMMSIZE];

	if (strcmp(type, "r") != 0)
		return file;
	else if (strcmp(type, "r") == 0)
		mode_type = READ;
	else
		mode_type = WRITE;

	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	if (mode_type == READ) {
		bRet = CreatePipe(&child_out_read, &child_out_write, &sa, 0);
		if (bRet == FALSE)
			return file;
		bRet = SetHandleInformation(child_out_read,
			HANDLE_FLAG_INHERIT, 0);
		if (bRet == FALSE)
			return file;
	} else {
		bRet = CreatePipe(&child_in_read, &child_in_write, &sa, 0);
		if (bRet == FALSE)
			return file;
		bRet = SetHandleInformation(child_in_write,
			HANDLE_FLAG_INHERIT, 0);
		if (bRet == FALSE)
			return file;
	}

	ZeroMemory(&pi, sizeof(pi));

	ZeroMemory(&psi, sizeof(psi));
	psi.cb = sizeof(psi);
	psi.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	psi.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	psi.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	psi.dwFlags |= STARTF_USESTDHANDLES;

	if (mode_type == READ)
		psi.hStdOutput = child_out_write;
	else
		psi.hStdInput = child_in_read;

	memset(comm, 0, sizeof(comm));
	strcat(comm, "cmd /C ");
	strcat(comm, command);

	bRet = CreateProcess(NULL,
		comm,
		NULL,
		NULL,
		TRUE,
		0,
		NULL,
		NULL,
		&psi,
		&pi);
	if (bRet == FALSE)
		return file;

	if (mode_type == READ)
		bRet = CloseHandle(child_out_write);
	else
		bRet = CloseHandle(child_in_read);
	if (bRet == FALSE)
		return file;

	file = malloc(sizeof(SO_FILE));
	if (file != NULL) {
		if (mode_type == READ)
			file->fd = child_out_read;
		else
			file->fd = child_in_write;
		file->pointer = 0;
		file->mode_type = mode_type;
		memset(file->buffer, 0, sizeof(file->buffer));
		file->last_op = -1;
		file->found_eof = FALSE;
		file->is_proc = TRUE;
		file->found_error = -1;
		file->buff_pos = 0;
		file->buff_size = 0;
		file->parent = pi.hThread;
		file->child = pi.hProcess;
	}
	return file;
}

/* Waits for child process associated with to terminate
 * Frees memory for given SO_FILE and closes its handle
 * The function flushes to file the remaining elements
 * in the buffer of the SO_FILE
 * Returns ExitCode for child process at success or
 * -1 in case of error
 */
int so_pclose(SO_FILE *stream)
{
	DWORD dwRet = 0;
	DWORD exitCode = 0;
	BOOL bRet;
	int ret = 0;

	if (stream->is_proc == FALSE)
		return -1;

	dwRet = WaitForSingleObject(stream->child, INFINITE);
	if (dwRet == WAIT_FAILED) {
		free(stream);
		return -1;
	}

	bRet = GetExitCodeProcess(stream->child, &exitCode);
	if (bRet == FALSE) {
		free(stream);
		return -1;
	}

	if (stream->last_op == LASTWRITE)
		ret = so_fflush(stream);

	bRet = CloseHandle(stream->parent);
	if (bRet == FALSE)
		return -1;
	bRet = CloseHandle(stream->child);
	free(stream);

	return ((bRet == FALSE || ret == -1) ? -1 : exitCode);
}
