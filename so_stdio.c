#define DLL_EXPORTS

#include "so_stdio.h"
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

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

	// handle of the file descriptor
	HANDLE handle;

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

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	HANDLE handle;
	SO_FILE *so_file = (SO_FILE *) malloc(1 * sizeof(SO_FILE));

	if (so_file == NULL)
		return NULL;

	if (strcmp(mode, "r") == 0) {
		so_file->handle = CreateFile(pathname,
								GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
								NULL,
								OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
								NULL
								);

		if (so_file->handle == INVALID_HANDLE_VALUE) {
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 0;
		so_file->mode_opened[MODE_READ] = 1;
		so_file->mode_opened[MODE_WRITE] = 0;
		so_file->mode_opened[MODE_APPEND] = 0;
	} else if (strcmp(mode, "r+") == 0) {
		so_file->handle = CreateFile(pathname,
						GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL
								);

		if (so_file->handle == INVALID_HANDLE_VALUE) {
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 1;
		so_file->mode_opened[MODE_READ] = 1;
		so_file->mode_opened[MODE_WRITE] = 1;
		so_file->mode_opened[MODE_APPEND] = 0;
	} else if (strcmp(mode, "w") == 0) {
		so_file->handle = CreateFile(pathname,
						GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							CREATE_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL
								);

		if (so_file->handle == INVALID_HANDLE_VALUE) {
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 0;
		so_file->mode_opened[MODE_READ] = 0;
		so_file->mode_opened[MODE_WRITE] = 1;
		so_file->mode_opened[MODE_APPEND] = 0;
	} else if (strcmp(mode, "w+") == 0) {
		so_file->handle = CreateFile(pathname,
						GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							CREATE_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL
								);

		if (so_file->handle == INVALID_HANDLE_VALUE) {
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 1;
		so_file->mode_opened[MODE_READ] = 1;
		so_file->mode_opened[MODE_WRITE] = 1;
		so_file->mode_opened[MODE_APPEND] = 0;
	} else if (strcmp(mode, "a") == 0) {
		so_file->handle = CreateFile(pathname,
							FILE_APPEND_DATA,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							OPEN_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL
								);

		if (so_file->handle == INVALID_HANDLE_VALUE) {
			free(so_file);
			return NULL;
		}

		// here, filed opened succesfully
		so_file->update_mode = 0;
		so_file->mode_opened[MODE_READ] = 0;
		so_file->mode_opened[MODE_WRITE] = 0;
		so_file->mode_opened[MODE_APPEND] = 1;
	} else if (strcmp(mode, "a+") == 0) {
		so_file->handle = CreateFile(pathname,
				FILE_APPEND_DATA | GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_ALWAYS,
						FILE_ATTRIBUTE_NORMAL,
						NULL
								);

		if (so_file->handle == INVALID_HANDLE_VALUE) {
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

int so_fclose(SO_FILE *stream)
{
	BOOL bRet;
	int error_encountered;
	int mode_write;
	int ret_fflush = 0;

	if ((stream == NULL) || (stream->handle == INVALID_HANDLE_VALUE)) {
		//printf("nothing to close\n");
		return SO_EOF;
	}

	// check if there is something
	// to be written
	if (stream->bytes_written != 0)
		ret_fflush = so_fflush(stream);

	bRet = CloseHandle(stream->handle);
	free(stream);

	return ((bRet == FALSE) || (ret_fflush == SO_EOF)) ? SO_EOF : 0;
}

HANDLE so_fileno(SO_FILE *stream)
{
	return stream->handle;
}

int so_fflush(SO_FILE *stream)
{
	int no_bytes_written = 0;

	if ((stream == NULL) || (stream->handle == INVALID_HANDLE_VALUE))
		return SO_EOF;

	while (no_bytes_written < stream->bytes_written) {
		int no_bytes_write;
		int ret_write;

		ret_write = WriteFile(stream->handle,
				  stream->buffer + no_bytes_written,
				  stream->bytes_written - no_bytes_written,
				  &no_bytes_write,
				  NULL);

		if (ret_write == 0) {
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
	int ret_value;

	if (stream->last_operation == LAST_OPERATION_WRITE) {
		// write content of buffer to fle
		so_fflush(stream);
		stream->bytes_written = 0;
	} else if (stream->last_operation == LAST_OPERATION_READ) {
		// invalidate buffer data
		memset(stream->buffer, 0, BUFFER_SIZE);
		stream->bytes_read = 0;
	}


	ret_value = SetFilePointer(stream->handle,
								   offset,
								   NULL,
								   whence);

	if (ret_value == INVALID_SET_FILE_POINTER)
		return -1;

	stream->cursor_write = ret_value;

	if (!((stream->mode_opened[MODE_APPEND]) && (stream->update_mode)))
		stream->cursor_read  = ret_value;

	stream->last_operation = LAST_OPERATION_SEEK;

	return 0;
}

long so_ftell(SO_FILE *stream)
{
	if ((stream == NULL) || (stream->handle == INVALID_HANDLE_VALUE))
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
	// check buffer empty
	// or not enough data available (size * nmemb)
	// for reading
	// stream
	int it;

	if ((stream == NULL) ||
		(stream->handle == INVALID_HANDLE_VALUE))
		return 0;


	for (it = 0; it < size * nmemb; it++) {
		char character_read = so_fgetc(stream);

		if (so_feof(stream) || (stream->error_encountered)) {
			stream->last_operation = LAST_OPERATION_READ;
			return it / size;
		}

		memcpy((char *)ptr + it, &character_read, 1);
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

int so_fgetc(SO_FILE *stream)
{
	int bytes_read = 0;

	if ((stream == NULL) || (stream->handle == INVALID_HANDLE_VALUE))
		return SO_EOF;

	if (((stream->cursor_read % BUFFER_SIZE != 0) &&
		((stream->cursor_read % BUFFER_SIZE) >= stream->bytes_read)) ||
		(stream->cursor_read % BUFFER_SIZE == 0)) {
		int ret_read;

		if (stream->bytes_read >= BUFFER_SIZE)
			stream->bytes_read %= BUFFER_SIZE;

		// read how much you can from stream->fd into stream->buffer
		ret_read = ReadFile(stream->handle,
				stream->buffer +
				(stream->cursor_read % BUFFER_SIZE),
				BUFFER_SIZE -
				(stream->cursor_read % BUFFER_SIZE),
				&bytes_read,
				NULL);

		if (ret_read == 0) {
			stream->error_encountered = 1;
			return SO_EOF;
		} else if ((ret_read != 0) && (bytes_read == 0)) {
			stream->eof_reached = 1;
			stream->cursor_read++;
			return SO_EOF;
		}
	}

	if (bytes_read > 0)
		stream->bytes_read = stream->cursor_read % BUFFER_SIZE +
							bytes_read;

	return stream->buffer[(stream->cursor_read++) % BUFFER_SIZE];
}

int so_fputc(int c, SO_FILE *stream)
{
	if ((stream == NULL) || (stream->handle == INVALID_HANDLE_VALUE))
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

SO_FILE *so_popen(const char *command, const char *type)
{
	return NULL;
}

int so_pclose(SO_FILE *stream)
{
	return 0;
}
