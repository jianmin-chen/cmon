#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef __APPLE__
#ifndef st_ctime
#define st_ctime st_ctimespec.tv_sec
#endif
#endif

#define INTERVAL 5007

void *reallocate(void *ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    void *result = realloc(ptr, new_size);
    return result;
}

typedef struct Opts {
    const char **watch;
    const char *command;
    int count;
    int capacity;
} Opts;

void initOpts(Opts *opts) {
    opts->watch = NULL;
    opts->command = NULL;
    opts->count = 0;
    opts->capacity = 0;
}

void freeOpts(Opts *opts) {
    reallocate(opts->watch, sizeof(const char *) * opts->capacity, 0);
}

void addWatchOpt(Opts *opts, const char *path) {
    if (opts->capacity < opts->count + 1) {
        int old_capacity = opts->capacity;
        opts->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        opts->watch = (const char **)reallocate(
            opts->watch,
            sizeof(char *) * old_capacity,
            sizeof(char *) * opts->capacity
        );
    }

    opts->watch[opts->count] = path;
    opts->count++;
}

void _debugOpts(Opts *opts) {
    for (int i = 0; i < opts->count; i++) {
        const char *c = opts->watch[i];
        printf("%s\n", c);
    }
}

void setTimeout(unsigned int ms) {
    unsigned int acc = 0;
    clock_t before = clock();
    do {
        clock_t dt = clock() - before;
        acc = dt * 1000 / CLOCKS_PER_SEC;
    } while (acc < ms);
}

void expect(bool condition, const char *throw) {
    if (!condition) {
        printf("%s", throw);
        exit(-2);
    }
}

bool changed(clock_t before, Opts *opts);

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Usage: cmon [options] [args]\nSee \"cmon --help\" for more.\n");
        return 0;
    }

    Opts options;
    initOpts(&options);

    unsigned int it = 1;
    while (it < argc) {
        const char *opt = argv[it];
        if (strcmp(opt, "-x") == 0) {
            expect(++it < argc, "Usage: cmon [options] -x [command]\n");
            options.command = argv[it];
        } else {
            addWatchOpt(&options, opt);
        }
        it++;
    }

    while (true) {
        clock_t before = time(NULL);
        setTimeout(INTERVAL);
        if (changed(before, &options)) {
            system(options.command);
        }
    }

    return 0;
}

bool changed(clock_t before, Opts *opts) {
    for (int i = 0; i < opts->count; i++) {
        const char *path = opts->watch[i];
        struct stat attr;
        if (stat(path, &attr) != 0) {
            perror("stat");
            exit(-3);
        }

        clock_t ctime = attr.st_ctime;
        if (ctime > before) {
            printf("%s changed, running `%s`.\n", path, opts->command);
            fflush(stdout);
            return false;
        }
    }
    return false;
}
