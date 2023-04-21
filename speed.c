#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <time.h>
#include <ctype.h>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// Str
typedef struct {
    long size;
    char *data;
} Str;

bool read_file(char *path, Str *str) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    struct stat statbuf;
    if (fstat(fd, &statbuf) == -1) {
        close(fd);
        return false;
    }

    str->size = statbuf.st_size;
    str->data = mmap(NULL, str->size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);
    if (str->data == MAP_FAILED) {
        return false;
    }

    long head = 0;
    for (long i = 0; i < str->size; ++i) {
        if (!isspace(str->data[i])) {
            str->data[head++] = str->data[i];
        } else if (head == 0 || str->data[head - 1] != ' ') {
            str->data[head++] = ' ';
        }
    }

    str->size = head;
    if (str->size && str->data[str->size - 1] == ' ') {
        str->size--;
    }

    return true;
}

// Terminal
typedef struct termios Term;

bool term_init(Term *term) {
    if (tcgetattr(STDIN_FILENO, term) == -1) {
        return false;
    }

    term->c_iflag &= ~IXON;
    term->c_lflag &= ~(ECHO | ICANON);
    return tcsetattr(0, 0, term) != -1;
}

bool term_exit(Term *term) {
    printf("\033[H\033[2J");
    term->c_iflag |= IXON;
    term->c_lflag |= ECHO | ICANON;
    return tcsetattr(0, 0, term) != -1;
}

bool term_cols(long *cols) {
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1) {
        return false;
    }

    *cols = ws.ws_col;
    return true;
}

// Main
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "error: file not provided\n");
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        exit(1);
    }
    char *path = argv[1];

    Str contents;
    if (!read_file(path, &contents)) {
        fprintf(stderr, "error: could not read file '%s'\n", path);
        exit(1);
    }

    Term term;
    if (!term_init(&term)) {
        fprintf(stderr, "error: could not setup terminal\n");
        exit(1);
    }

    long cols;
    if (!term_cols(&cols)) {
        fprintf(stderr, "error: could not get width of terminal\n");
        exit(1);
    }

    long head = 0;
    long wrong = 0;
    time_t clock = 0;
    while (head < contents.size) {
        printf("\033[H\033[2J\033[33m");
        fwrite(contents.data, head, 1, stdout);

        printf("\033[0m");
        fwrite(contents.data + head, contents.size - head, 1, stdout);
        printf("\033[%ld;%ldH", head / cols + 1, head % cols + 1);

        char ch = getchar();
        if (ch == CTRL('q')) {
            break;
        }

        if (ch == contents.data[head]) {
            if (head == 0) {
                clock = time(NULL);
            }
            head++;
        } else {
            wrong++;
        }
    }
    clock = time(NULL) - clock;

    if (!term_exit(&term)) {
        fprintf(stderr, "error: could not reset terminal\n");
        exit(1);
    }

    printf("WPM:      %g\n", 12.0 * head / clock);
    printf("Accuracy: %g%%\n", (head - wrong) * 100.0 / head);
}
