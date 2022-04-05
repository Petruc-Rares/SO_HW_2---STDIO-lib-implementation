#include "so_stdio.h"
#include <string.h>
#include <stdlib.h>
#include <windows.h>

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

SO_FILE *so_fopen(const char *pathname, const char *mode) {
	HANDLE handle;
	SO_FILE *so_file = (SO_FILE *) malloc (1 * sizeof(SO_FILE));
	
	if (so_file == NULL) {
		return NULL;
	}
	
	if (strcmp(mode, "r") == 0) {
		so_file->handle = CreateFile(pathname,
									GENERIC_READ,
									FILE_SHARE_READ,
									NULL,	/* no security attributes */
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									NULL	/* no pattern */
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
									FILE_SHARE_READ,
									NULL,	/* no security attributes */
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									NULL	/* no pattern */
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
									FILE_SHARE_WRITE,
									NULL,	/* no security attributes */
									CREATE_ALWAYS,
									FILE_ATTRIBUTE_NORMAL,
									NULL	/* no pattern */
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
									FILE_SHARE_WRITE,
									NULL,	/* no security attributes */
									CREATE_ALWAYS,
									FILE_ATTRIBUTE_NORMAL,
									NULL	/* no pattern */
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
									FILE_SHARE_READ,
									NULL,	/* no security attributes */
									CREATE_ALWAYS,
									FILE_ATTRIBUTE_NORMAL,
									NULL	/* no pattern */
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
									FILE_SHARE_READ,
									NULL,	/* no security attributes */
									CREATE_ALWAYS,
									FILE_ATTRIBUTE_NORMAL,
									NULL	/* no pattern */
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

int so_fclose(SO_FILE *stream) {
	BOOL bRet;
	int error_encountered;
	int mode_write;

	if ((stream == NULL) || (stream->handle == INVALID_HANDLE_VALUE)) {
		//printf("nothing to close\n");
		return SO_EOF;
	}

	// check if there is something
	// to be written
	if (stream->bytes_written != 0)
		so_fflush(stream);

	error_encountered = stream->error_encountered;
	mode_write = stream->mode_opened[MODE_WRITE];

	bRet = CloseHandle(stream->handle);
	free(stream);
	if ((mode_write) && (error_encountered))
		return SO_EOF;
	else 
		return (bRet == TRUE) ? 0 : 1;
}

HANDLE so_fileno(SO_FILE *stream) {
	return NULL;
}

int so_fflush(SO_FILE *stream) {
	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence) {
	return 0;
}

long so_ftell(SO_FILE *stream) {
	return 0;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
	return 0;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
	return 0;
}

int so_fgetc(SO_FILE *stream) {
	return 0;
}

int so_fputc(int c, SO_FILE *stream) {
	return 0;
}

int so_feof(SO_FILE *stream) {
	return 0;
}

int so_ferror(SO_FILE *stream) {
	return 0;
}

SO_FILE *so_popen(const char *command, const char *type) {
	return NULL;
}

int so_pclose(SO_FILE *stream) {
	return 0;
}
