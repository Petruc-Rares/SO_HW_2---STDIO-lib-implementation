#include "so_stdio.h"
#include "stdio.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


#define BUFFER_SIZE 4096
#define NO_MODES_OPEN 3
#define MODE_READ 0
#define MODE_WRITE 1
#define MODE_APPEND 2
#define DEFAULT_PERMISSIONS 0644
#define LAST_OPERATION_SEEK 2
#define LAST_OPERATION_WRITE 1
#define LAST_OPERATION_READ 0
#define LAST_OPERATION_OTHER -1

struct _so_file {
	char buffer[BUFFER_SIZE];

	// have two possible cursors
	// one for reading, and other
	// for writing (e.g: they differ for "a+" mode)
	int cursor_read;
	int cursor_write;

	// set to 1 if '+' exists at mode
	// of opening of the file
	// otherwise 0
	char update_mode;

	// -1, if last operation is open/close
	// 0, if last operation is read
	// 1, if last operation is writing
	char last_operation;

	// number of the file descriptor
	int fd;

	// number of bytes read from the file
	int bytes_read;

	// number of bytes written into the file
	int bytes_written;

	// read, write and append
	char mode_opened[NO_MODES_OPEN];

	char eof_reached;

	char error_encountered;

	int child_pid;
};

SO_FILE *so_fopen(const char *pathname, const char *mode) {
	SO_FILE *so_file = (SO_FILE *) malloc(1 * sizeof(SO_FILE));

	if (so_file == NULL) {
		//printf("Error at allocating structure of so_file\n");
		return NULL;
	}

	if (strcmp(mode, "r") == 0) {
		so_file->fd = open(pathname, O_RDONLY);

		if (so_file->fd < 0) {
			//printf("Can't open file with pathname: %s\n", pathname);
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 0;
		so_file->mode_opened[MODE_READ] = 1;
		so_file->mode_opened[MODE_WRITE] = 0;
		so_file->mode_opened[MODE_APPEND] = 0;
	} else if (strcmp(mode, "r+") == 0) {
		so_file->fd = open(pathname, O_RDWR);

		if (so_file->fd < 0) {
			//printf("Can't open file with pathname: %s\n", pathname);
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 1;
		so_file->mode_opened[MODE_READ] = 1;
		so_file->mode_opened[MODE_WRITE] = 1;
		so_file->mode_opened[MODE_APPEND] = 0;
	} else if (strcmp(mode, "w") == 0) {
		so_file->fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, DEFAULT_PERMISSIONS);

		if (so_file->fd < 0) {
			//printf("Can't open file with pathname: %s\n", pathname);
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 0;
		so_file->mode_opened[MODE_READ] = 0;
		so_file->mode_opened[MODE_WRITE] = 1;
		so_file->mode_opened[MODE_APPEND] = 0;
	} else if (strcmp(mode, "w+") == 0) {
		so_file->fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, DEFAULT_PERMISSIONS);

		if (so_file->fd < 0) {
			//printf("Can't open file with pathname: %s\n", pathname);
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 1;
		so_file->mode_opened[MODE_READ] = 1;
		so_file->mode_opened[MODE_WRITE] = 1;
		so_file->mode_opened[MODE_APPEND] = 0;
	} else if (strcmp(mode, "a") == 0) {
		so_file->fd = open(pathname, O_APPEND | O_CREAT | O_WRONLY, DEFAULT_PERMISSIONS);

		if (so_file->fd < 0) {
			//printf("Can't open file with pathname: %s\n", pathname);
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 0;
		so_file->mode_opened[MODE_READ] = 0;
		so_file->mode_opened[MODE_WRITE] = 0;
		so_file->mode_opened[MODE_APPEND] = 1;
	} else if (strcmp(mode, "a+") == 0) {
		so_file->fd = open(pathname, O_APPEND | O_CREAT | O_RDWR, DEFAULT_PERMISSIONS);

		if (so_file->fd < 0) {
			//printf("Can't open file with pathname: %s\n", pathname);
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 1;
		so_file->mode_opened[MODE_READ] = 1;
		so_file->mode_opened[MODE_WRITE] = 0;
		so_file->mode_opened[MODE_APPEND] = 1;
	} else {
		free(so_file);
		return NULL;
	}

	memset(so_file->buffer, 0, BUFFER_SIZE);
	so_file->cursor_read = 0;
	so_file->cursor_write = 0;
	so_file->last_operation = LAST_OPERATION_OTHER;
	so_file->bytes_read = 0;
	so_file->bytes_written = 0;
	so_file->eof_reached = 0;
	so_file->error_encountered = 0;
	so_file->child_pid = 0;

	return so_file;
}

int so_fgetc(SO_FILE *stream)
{
	if ((stream == NULL) || (stream->fd < 0))
		//printf("Nothing to read, invalid stream\n");
		return SO_EOF;

	int bytes_read = 0;

	if (((stream->cursor_read % BUFFER_SIZE != 0) && ((stream->cursor_read % BUFFER_SIZE) >= stream->bytes_read)) ||
		(stream->cursor_read % BUFFER_SIZE == 0)) {
		if (stream->bytes_read >= BUFFER_SIZE)
			stream->bytes_read %= BUFFER_SIZE;

		// read how much you can from stream->fd into stream->buffer
		bytes_read = read(stream->fd, stream->buffer + (stream->cursor_read % BUFFER_SIZE), BUFFER_SIZE - (stream->cursor_read % BUFFER_SIZE));

		//printf("bytes_read: %d\n", bytes_read);
		if (bytes_read < 0) {
			//printf("Can't read from file\n");
			stream->error_encountered = 1;
			return SO_EOF;
		} else if (bytes_read == 0) {
			stream->eof_reached = 1;
			stream->cursor_read++;
			//printf("Read everything possible from the file\n");
			return SO_EOF;
		}
	}

	stream->bytes_read = stream->cursor_read % BUFFER_SIZE + bytes_read;
	return stream->buffer[(stream->cursor_read++) % BUFFER_SIZE];
}

int so_fputc(int c, SO_FILE *stream)
{
	if ((stream == NULL) || (stream->fd < 0))
		return SO_EOF;

	if (stream->bytes_written >= BUFFER_SIZE)
		so_fflush(stream);

	memcpy(stream->buffer + stream->bytes_written, &c, 1);
	stream->bytes_written += 1;
	stream->last_operation = LAST_OPERATION_WRITE;

	return c;
}

int so_feof(SO_FILE *stream)
{
	return stream->eof_reached;
}

int so_ferror(SO_FILE *stream)
{
	return stream->error_encountered;
}

int so_fflush(SO_FILE *stream)
{
	if ((stream == NULL) || (stream->fd < 0))
		return SO_EOF;

	int no_bytes_written = 0;

	while (no_bytes_written < stream->bytes_written) {
		int no_bytes_write = write(stream->fd, stream->buffer + no_bytes_written, stream->bytes_written - no_bytes_written);

		if (no_bytes_write < 0) {
			stream->error_encountered = 1;
			return -1;
		}
		no_bytes_written += no_bytes_write;
		stream->cursor_write += no_bytes_written;
	}

	stream->bytes_written = 0;
	memset(stream->buffer, 0, BUFFER_SIZE);
	return (no_bytes_written >= 0) ? 0 : -1;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream->last_operation == LAST_OPERATION_WRITE) {
		// write content of buffer to fle
		so_fflush(stream);
		stream->bytes_written = 0;
	} else if (stream->last_operation == LAST_OPERATION_READ) {
		// invalidate buffer data
		memset(stream->buffer, 0, BUFFER_SIZE);
		stream->bytes_read = 0;
	}

	int ret_value = lseek(stream->fd, offset, whence);
	//printf("value after move: %d\n", lseek(stream->fd, 0, SEEK_CUR));
	if (ret_value < 0)
		return -1;

	stream->cursor_write = ret_value;

	if (!((stream->mode_opened[MODE_APPEND]) && (stream->update_mode)))
		stream->cursor_read  = ret_value;

	stream->last_operation = LAST_OPERATION_SEEK;

	return 0;
}

long so_ftell(SO_FILE *stream)
{
	if ((stream == NULL) || (stream->fd < 0))
		return -1;

	if (stream->bytes_written != 0)
		so_fflush(stream);

	if (stream->mode_opened[MODE_READ])
		return stream->cursor_read;
	else if (stream->mode_opened[MODE_WRITE])
		return stream->cursor_write;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	// TODO: check buffer empty
	// or not enough data available (size * nmemb)
	// for reading
	//stream
	if ((stream == NULL) || (stream->fd < 0))
		return 0;

	int it;

	for (it = 0; it < size * nmemb; it++) {
		int character_read = so_fgetc(stream);
		// TODO - THIS MIGHT BE DELETED

		if (so_feof(stream) || (stream->error_encountered)) {
			stream->last_operation = LAST_OPERATION_READ;
			return it / size;
		}
		memcpy(ptr + it, &character_read, 1);
	}

	stream->last_operation = LAST_OPERATION_READ;
	return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int it;

	for (it = 0; it < size * nmemb; it++) {
		int ret_value = so_fputc(((const char *)ptr)[it], stream);
		// TODO - this check might be useless
		if ((unsigned char) ret_value == SO_EOF) {
			stream->last_operation = LAST_OPERATION_WRITE;
			return it / size;
		}
	}

	stream->last_operation = LAST_OPERATION_WRITE;
	return nmemb;
}

int so_fclose(SO_FILE *stream)
{
	if ((stream == NULL) || (stream->fd < 0)) {
		//printf("nothing to close\n");
		return SO_EOF;
	}

	// check if there is something
	// to be written
	if (stream->bytes_written != 0)
		so_fflush(stream);
		//printf("Nu s-a terminat flush-ul\n");

	int error_encountered = stream->error_encountered;
	int mode_write = stream->mode_opened[MODE_WRITE];

	int ret_value;

	ret_value = close(stream->fd);
	free(stream);

	if ((mode_write) && (error_encountered))
		return SO_EOF;
	else
		return ret_value;
}

int so_fileno(SO_FILE *stream)
{
	if (stream == NULL)
		return -1;

	return stream->fd;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *so_file = malloc(1 * sizeof(SO_FILE));

	if (so_file == NULL) {
		// printf("alloc failed for so_file in so_popen\n")
		free(so_file);
		return NULL;
	}

	char *const argvvec[] = {"sh", "-c", (char *const)command, NULL};
	int filedes[2];

	if (pipe(filedes) < 0) {
		free(so_file);
		return NULL;
	}

	int pid = fork();

	switch (pid) {
	case -1:
		free(so_file);
		return NULL;
	case 0:
		// child process
		if (strcmp(type, "r") == 0) {
			// child process doesn't use
			// the pipe for reading
			close(filedes[0]);
			// redirect stdout to filedes[1]
			// so the stdout reaches the parent
			dup2(filedes[1], STDOUT_FILENO);
		} else if (strcmp(type, "w") == 0) {
			// child process doesn't use
			// the pipe for writing
			close(filedes[1]);

			// redirect stdin to filedes[0]
			// so the stdin reaches the parent
			dup2(filedes[0], STDIN_FILENO);
		}

		execv("/bin/sh", argvvec);

		/* only if exec failed */
		exit(EXIT_FAILURE);
	default:
		/* parent process */
		so_file->child_pid = pid;
		if (strcmp(type, "r") == 0) {
			// parent process doesn't use
			// the pipe for writing
			close(filedes[1]);
			so_file->fd = filedes[0];
			so_file->mode_opened[MODE_WRITE] = 0;
			so_file->mode_opened[MODE_READ] = 1;
			so_file->mode_opened[MODE_APPEND] = 0;
		} else if (strcmp(type, "w") == 0) {
			// parent process doesn't use
			// the pipe for reading
			close(filedes[0]);
			so_file->fd = filedes[1];
			so_file->mode_opened[MODE_WRITE] = 1;
			so_file->mode_opened[MODE_READ] = 0;
			so_file->mode_opened[MODE_APPEND] = 0;
		}

		break;
	}

	memset(so_file->buffer, 0, BUFFER_SIZE);
	so_file->update_mode = 0;
	so_file->cursor_read = 0;
	so_file->cursor_write = 0;
	so_file->last_operation = LAST_OPERATION_OTHER;
	so_file->bytes_read = 0;
	so_file->bytes_written = 0;
	so_file->eof_reached = 0;
	so_file->error_encountered = 0;

	return so_file;
}

int so_pclose(SO_FILE *stream)
{
	if ((stream == NULL) || (stream->child_pid <= 0))
		return -1;

	int status;

	if (stream->bytes_written != 0)
		so_fflush(stream);

	// TODO - why is this necessary for waitpid
	// not to block
	close(stream->fd);
	int ret_wait = waitpid(stream->child_pid, &status, 0);

	free(stream);

	return (ret_wait != -1) ? 0 : -1;
}
