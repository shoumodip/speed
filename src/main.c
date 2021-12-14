#define _XOPEN_SOURCE 500
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>

#include "sv.h"

typedef enum {
    COLOR_TYPED,
    COLOR_WRONG,
    COLOR_LEFT
} Color;

typedef struct {
    char *head;
    SV *list;
    size_t list_length;
    size_t list_capacity;
    size_t list_minimum;

    size_t *display;
    size_t display_length;
    size_t display_capacity;
    size_t display_padding;
    size_t display_current;
} Speed;

void speed_free(Speed *speed)
{
    if (speed->head) free(speed->head);
    if (speed->list) free(speed->list);
    if (speed->display) free(speed->display);
}

#define DISPLAY_COVERAGE 0.8
#define DISPLAY_ROW 5
#define DISPLAY_WRONG_FLASH_DURATION 100

#define MINIMUM_CAPACITY 256

#define MAX(a, b) (a > b) ? a : b
#define MIN(a, b) (a < b) ? a : b

void speed_list_push(Speed *speed, SV word)
{
    if (speed->list_length >= speed->list_capacity) {
        speed->list_capacity = MAX(speed->list_capacity * 2, MINIMUM_CAPACITY);
        speed->list = speed->list
            ? realloc(speed->list, speed->list_capacity * sizeof(SV))
            : malloc(speed->list_capacity * sizeof(SV));
    }

    speed->list[speed->list_length++] = word;
    speed->list_minimum = MIN(speed->list_minimum, word.length);
}

void speed_list_load(Speed *speed, const char *file_path)
{
    SV file = sv_read_file(file_path);
    speed->head = (char *) file.source;
    while (file.length) speed_list_push(speed, sv_trim(sv_split(&file, '\n'), ' '));
}

void speed_display_grow(Speed *speed)
{
    speed->display_capacity = COLS * DISPLAY_COVERAGE;
    speed->display_padding = (COLS - speed->display_capacity) / 2;
    speed->display = malloc(speed->display_capacity / speed->list_minimum * sizeof(size_t));
}

void speed_display_load(Speed *speed)
{
    size_t left = speed->display_capacity;

    speed->display_length = 0;
    while (left >= speed->list_minimum) {
        size_t index = 0;

        while (speed->list[(index = rand() % speed->list_length)].length > left);

        left -= speed->list[index].length;
        if (left) left--;

        speed->display[speed->display_length++] = index;
    }

    speed->display_current = speed->display_padding;
}

void speed_display_render(const Speed *speed, bool wrong)
{
    bool text_left = false;
    size_t text_left_count = speed->display_current - speed->display_padding;

    if (wrong) {
        attron(COLOR_PAIR(COLOR_WRONG));
    } else if (text_left_count) {
        attron(COLOR_PAIR(COLOR_TYPED));
        text_left = true;
    } else {
        attron(COLOR_PAIR(COLOR_LEFT));
    }

    move(DISPLAY_ROW, speed->display_padding);
    for (size_t i = 0; i < speed->display_length; ++i) {
        SV word = speed->list[speed->display[i]];

        if (text_left) {
            if (text_left_count >= word.length) {
                printw(SVFmt" ", SVArg(word));
                text_left_count -= word.length;

                if (text_left_count) {
                    text_left_count--;
                } else {
                    text_left = false;
                }
            } else {
                printw(SVFmt, text_left_count, word.source);
                attron(COLOR_PAIR(COLOR_LEFT));
                printw(SVFmt" ", word.length - text_left_count, word.source + text_left_count);

                text_left_count = 0;
                text_left = false;
            }
        } else {
            printw(SVFmt" ", SVArg(word));
        }
    }

    if (wrong) {
        attroff(COLOR_PAIR(COLOR_WRONG));
    } else if (!text_left) {
        attroff(COLOR_PAIR(COLOR_LEFT));
    }

    attron(COLOR_PAIR(COLOR_TYPED));

    move(DISPLAY_ROW, speed->display_current);
    refresh();
}

void speed_train_wrong(const Speed *speed)
{
    speed_display_render(speed, true);
    usleep(DISPLAY_WRONG_FLASH_DURATION * 1000);
    speed_display_render(speed, false);
}

// TODO: use character transposition statistics to generate next generation
void speed_train_input(Speed *speed, char expect)
{
    move(DISPLAY_ROW, speed->display_current);

    while (true) {
        const int ch = getch();

        if (ch == expect) {
            addch(ch);
            speed->display_current++;
            return;
        } else if (ch == 27) {
            speed_free(speed);
            endwin();
            exit(0);
        } else {
            speed_train_wrong(speed);
        }
    }
}

void speed_train_start(Speed *speed)
{
    erase();
    speed_display_load(speed);
    speed_display_render(speed, false);

    for (size_t i = 0; i < speed->display_length; ++i) {
        if (i) speed_train_input(speed, ' ');

        SV word = speed->list[speed->display[i]];

        for (size_t j = 0; j < word.length; ++j) {
            speed_train_input(speed, word.source[j]);
        }
    }
}

int main(int argc, char **argv)
{
    Speed speed = {0};
    speed.list_minimum = UINT64_MAX;

    if (argc == 2) {
        speed_list_load(&speed, argv[1]);
    } else {
        // TODO: better default word list
        speed_list_push(&speed, SVStatic("foo"));
        speed_list_push(&speed, SVStatic("bar"));
        speed_list_push(&speed, SVStatic("lmao"));
    }

    initscr();
    raw();
    noecho();
    start_color();
    use_default_colors();

    init_pair(COLOR_WRONG, 1, -1);
    init_pair(COLOR_LEFT, 8, -1);

    speed_display_grow(&speed);

    // TODO: severals generations
    speed_train_start(&speed);

    endwin();
    speed_free(&speed);
    return 0;
}
