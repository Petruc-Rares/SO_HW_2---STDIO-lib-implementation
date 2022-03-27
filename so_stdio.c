#include "so_stdio.h"
#include "stdio.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define NO_MODES_OPEN 3
#define MODE_READ 0
#define MODE_WRITE 1
#define MODE_APPEND 2
#define DEFAULT_PERMISSIONS 0644
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
};

SO_FILE *so_fopen(const char *pathname, const char *mode) {
    SO_FILE *so_file = (SO_FILE *) malloc (1 * sizeof(SO_FILE));

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

    return so_file;
}

int so_fgetc(SO_FILE *stream) {
    if ((stream == NULL) || (stream->fd < 0)) {
        //printf("Nothing to read, invalid stream\n");
        return SO_EOF;
    }
    int bytes_read = 0;

    if (((stream->cursor_read % BUFFER_SIZE != 0) && ((stream->cursor_read % BUFFER_SIZE) >= stream->bytes_read)) ||
        (stream->cursor_read % BUFFER_SIZE == 0)) {
        if (stream->bytes_read >= BUFFER_SIZE) {
            stream->bytes_read %= BUFFER_SIZE;
        }

        // read how much you can from stream->fd into stream->buffer
        bytes_read = read(stream->fd, stream->buffer + (stream->cursor_read % BUFFER_SIZE), BUFFER_SIZE - (stream->cursor_read % BUFFER_SIZE));

        //printf("bytes_read: %d\n", bytes_read);
        if (bytes_read < 0) {
            //printf("Can't read from file\n");
            return SO_EOF;
        } else if (bytes_read == 0){
            //printf("Read everything possible from the file\n");
            //return SO_EOF;
        } else {
            stream->bytes_read = stream->cursor_read % BUFFER_SIZE + bytes_read;
        }
    }


    return stream->buffer[(stream->cursor_read++) % BUFFER_SIZE];
}

int so_fputc(int c, SO_FILE *stream) {
    if ((stream == NULL) || (stream->fd < 0)) {
        return SO_EOF;
    }

    if (stream->bytes_written >= BUFFER_SIZE) {
        so_fflush(stream);
    }

    memcpy(stream->buffer + stream->bytes_written, &c, 1);
    stream->bytes_written += 1;
    stream->last_operation = LAST_OPERATION_WRITE;

    return c;
}

int so_feof(SO_FILE *stream) {

}

int so_ferror(SO_FILE *stream) {
    
}

int so_fflush(SO_FILE *stream) {
    if ((stream == NULL) || (stream->fd < 0)) {
        return SO_EOF;
    }

    int no_bytes_written = 0;

    while (no_bytes_written < stream->bytes_written) {
        int no_bytes_write = write(stream->fd, stream->buffer + no_bytes_written, stream->bytes_written - no_bytes_written);
        if (no_bytes_write < 0) {
            return -1;
        }
        no_bytes_written += no_bytes_write;
        stream->cursor_write += no_bytes_written;
    }
    
    stream->bytes_written = 0;
    memset(stream->buffer, 0, BUFFER_SIZE);
    return (no_bytes_written >= 0) ? 0: -1;
}

int so_fseek(SO_FILE *stream, long offset, int whence) {
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
    if (ret_value < 0) {
        return -1;
    }

    stream->cursor_write = ret_value;

    if (!((stream->mode_opened[MODE_APPEND]) && (stream->update_mode))) {
        stream->cursor_read  = ret_value;
    }

    return 0;
}

long so_ftell(SO_FILE *stream) {
    if ((stream == NULL) || (stream->fd < 0)) {
        return -1;
    }

        if (stream->bytes_written != 0) {
        so_fflush(stream);
    }

    if (stream->mode_opened[MODE_READ]) {
        return stream->cursor_read;
    } else if (stream->mode_opened[MODE_WRITE]) {
        return stream->cursor_write;
    }
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
    // TODO: check buffer empty
    // or not enough data available (size * nmemb)
    // for reading
    //stream

    int it;

    for (it = 0; it < size * nmemb; it++) {
        int character_read = so_fgetc(stream);
        if ((unsigned char) character_read == SO_EOF) {
            stream->last_operation = LAST_OPERATION_READ;
            return it / size;
        }
        memcpy(ptr + it, &character_read, 1);
    }

    stream->last_operation = LAST_OPERATION_READ;
    return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream) {
    int it;

    for (it = 0; it < size * nmemb; it++) {
        int ret_value = so_fputc(((const char *)ptr)[it], stream);
        if ((unsigned char) ret_value == SO_EOF) {
            stream->last_operation = LAST_OPERATION_WRITE;
            return it / size;
        }
    }

    stream->last_operation = LAST_OPERATION_WRITE;
    return nmemb;
}

int so_fclose(SO_FILE *stream) {
    if ((stream == NULL) || (stream->fd < 0)) {
        //printf("nothing to close\n");
        return SO_EOF;
    } else {
        // check if there is something
        // to be written
        if (stream->bytes_written != 0) {
            so_fflush(stream);
        }
        //printf("Nu s-a terminat flush-ul\n");

        int ret_value;
        ret_value = close(stream->fd);
        free(stream);
        return ret_value;
    }
}

int so_fileno(SO_FILE *stream) {
    if (stream == NULL) {
        return -1;
    }

    return stream->fd;
}

SO_FILE *so_popen(const char *command, const char *type) {

}

int so_pclose(SO_FILE *stream) {

}
