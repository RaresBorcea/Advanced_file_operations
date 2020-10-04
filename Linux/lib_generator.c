#include "stdio_internal.h"
#include <string.h>

/* Allocates and returns a new SO_FILE structure
 * Reading and writing permission coresponding to mode string
 * Returns NULL in case of error
 */
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int fd = -1;
	int mode_type = -1;
	SO_FILE *file = NULL;
	long end_position = -1;

	if (strcmp(mode, "r") == 0) {
		fd = open(pathname, O_RDONLY);
		mode_type = READ;
	} else if (strcmp(mode, "r+") == 0) {
		fd = open(pathname, O_RDWR);
		mode_type = READPLUS;
	} else if (strcmp(mode, "w") == 0) {
		fd = open(pathname, O_WRONLY | O_TRUNC | O_CREAT, 0644);
		mode_type = WRITE;
	} else if (strcmp(mode, "w+") == 0) {
		fd = open(pathname, O_RDWR | O_TRUNC | O_CREAT, 0644);
		mode_type = WRITEPLUS;
	} else if (strcmp(mode, "a") == 0) {
		fd = open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0644);
		mode_type = APPEND;
	} else if (strcmp(mode, "a+") == 0) {
		fd = open(pathname, O_RDWR | O_APPEND | O_CREAT, 0644);
		mode_type = APPENDPLUS;
	}

	if (fd != -1) {
		file = malloc(sizeof(SO_FILE));
		if (file != NULL) {
			file->fd = fd;
			file->pointer = lseek(fd, 0, SEEK_CUR);
			file->mode_type = mode_type;
			memset(file->buffer, 0, sizeof(file->buffer));
			file->last_op = -1;

			end_position = lseek(fd, 0, SEEK_END);
			if (file->pointer == end_position)
				file->found_eof = true;
			else
				file->found_eof = false;
			lseek(fd, file->pointer, SEEK_SET);

			file->pid = -1;
			file->found_error = -1;
			file->buff_pos = 0;
			file->buff_size = 0;
		}
	}
	return file;
}

/* Frees memory for given SO_FILE and closes its file descr
 * The function flushes to file the remaining elements
 * in the buffer of the SO_FILE
 * Returns 0 at success, SO_EOF in case of error
 */
int so_fclose(SO_FILE *stream)
{
	int ret = 0;

	if (stream->last_op == LASTWRITE)
		ret = so_fflush(stream);
	ret |= close(stream->fd);
	free(stream);
	return ret;
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
	int bytes_written = 0;
	int bytes_written_now = 0;

	if (stream->last_op == LASTREAD) {
		memset(stream->buffer, 0, sizeof(stream->buffer));
		stream->buff_pos = 0;
		stream->buff_size = 0;
	} else if (stream->last_op == LASTWRITE) {
		while (bytes_written < stream->buff_size) {
			bytes_written_now = write(stream->fd,
				stream->buffer + bytes_written,
				stream->buff_size - bytes_written);
			stream->pointer += bytes_written_now;

			if (bytes_written_now <= 0) {
				stream->found_error = 1;
				return -1;
			}
			bytes_written += bytes_written_now;
		}
		memset(stream->buffer, 0, sizeof(stream->buffer));
		stream->buff_pos = 0;
		stream->buff_size = 0;
	}

	stream->pointer = lseek(stream->fd, offset, whence);
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
	int bytes_written = 0;
	int bytes_written_now = 0;

	if (stream->last_op != LASTWRITE) {
		stream->found_error = 1;
		return SO_EOF;
	}
	while (bytes_written < stream->buff_size) {
		bytes_written_now = write(stream->fd,
			stream->buffer + bytes_written,
			stream->buff_size - bytes_written);
		stream->pointer += bytes_written_now;

		if (bytes_written_now <= 0) {
			stream->found_error = 1;
			return SO_EOF;
		}
		bytes_written += bytes_written_now;
	}
	memset(stream->buffer, 0, sizeof(stream->buffer));
	stream->buff_pos = 0;
	stream->buff_size = 0;
	return 0;
}

/* Returns the file descr associated with this SO_FILE */
int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

/* Returns 1 if SO_FILE pointer is at EOF, 0 otherwise */
int so_feof(SO_FILE *stream)
{
	if (stream->found_eof == true)
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
	long bytes_read = 0;
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
		bytes_read = read(stream->fd, stream->buffer, BUFFCAPACIT);
		if (bytes_read == -1) {
			stream->found_error = 1;
			return SO_EOF;
		} else if (bytes_read == 0) {
			stream->found_eof = true;
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
		memcpy(ptr++, &c, sizeof(unsigned char));
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
	int bytes_written = 0;
	int bytes_written_now = 0;

	if (so_ferror(stream))
		return SO_EOF;

	if (stream->buff_size == BUFFCAPACIT) {
		while (bytes_written < stream->buff_size) {
			bytes_written_now = write(stream->fd,
				stream->buffer + bytes_written,
				stream->buff_size - bytes_written);
			stream->pointer += bytes_written_now;

			if (bytes_written_now <= 0) {
				stream->found_error = 1;
				return SO_EOF;
			}
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
		memcpy(&ch, (ptr + i++), sizeof(unsigned char));
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
	pid_t pid;
	int ret = 0;
	int fds[2];
	SO_FILE *file = NULL;
	int fd = -1;
	int mode_type = -1;

	if (strcmp(type, "r") != 0 && strcmp(type, "w") != 0)
		return file;

	ret = pipe(fds);
	if (ret != 0)
		return file;

	pid = fork();
	switch (pid) {
	case -1:
		close(fds[PIPE_READ]);
		close(fds[PIPE_WRITE]);
		return file;
	case 0:
		if (strcmp(type, "r") == 0) {
			ret = close(fds[PIPE_READ]);
			if (fds[PIPE_WRITE] != STDOUT_FILENO) {
				ret |= dup2(fds[PIPE_WRITE], STDOUT_FILENO);
				ret |= close(fds[PIPE_WRITE]);
			}
		} else {
			ret = close(fds[PIPE_WRITE]);
			if (fds[PIPE_READ] != STDIN_FILENO) {
				ret |= dup2(fds[PIPE_READ], STDIN_FILENO);
				ret |= close(fds[PIPE_READ]);
			}
		}
		if (ret < 0)
			exit(-1);
		execl("/bin/sh", "/bin/sh", "-c", command, NULL);
		exit(-1);
	}

	if (strcmp(type, "r") == 0) {
		fd = fds[PIPE_READ];
		ret = close(fds[PIPE_WRITE]);
		mode_type = READ;
	} else {
		fd = fds[PIPE_WRITE];
		ret = close(fds[PIPE_READ]);
		mode_type = WRITE;
	}
	if (ret < 0)
		return file;

	file = malloc(sizeof(SO_FILE));
	if (file != NULL) {
		file->fd = fd;
		file->pointer = 0;
		file->mode_type = mode_type;
		memset(file->buffer, 0, sizeof(file->buffer));
		file->last_op = -1;
		file->found_eof = false;
		file->pid = pid;
		file->found_error = -1;
		file->buff_pos = 0;
		file->buff_size = 0;
	}
	return file;
}

/* Waits for child process associated with to terminate
 * Frees memory for given SO_FILE and closes its file descr
 * The function flushes to file the remaining elements
 * in the buffer of the SO_FILE
 * Returns ExitCode for child process at success or
 * -1 in case of error
 */
int so_pclose(SO_FILE *stream)
{
	int pid = stream->pid;
	int backup_pid = pid;
	int status = 0;
	int ret = 0;

	if (pid < 0)
		return -1;

	if (stream->last_op == LASTWRITE)
		ret = so_fflush(stream);
	ret |= close(stream->fd);
	free(stream);

	do {
		pid = waitpid(backup_pid, &status, 0);
	} while (pid == -1 && errno == EINTR);

	return ((pid == -1 || ret == -1) ? -1 : status);
}
