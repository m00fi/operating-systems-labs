#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#define BUF_SIZE 1001
#define MAX_FD 20

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// join 2 path. returned pointer is for newly allocated memory and must be freed
char* join_paths(const char* path1, const char* path2)
{
    char* res;
    const int l1 = strlen(path1);
    if (path1[l1 - 1] == '/')
    {
        res = malloc(strlen(path1) + strlen(path2) + 1);
        if (!res)
            ERR("malloc");
        strcpy(res, path1);
    }
    else
    {
        res = malloc(strlen(path1) + strlen(path2) + 2);  // additional space for "/"
        if (!res)
            ERR("malloc");
        strcpy(res, path1);
        res[l1] = '/';
        res[l1 + 1] = 0;
    }
    return strcat(res, path2);
}

void usage(int argc, char** argv)
{
    (void)argc;
    fprintf(stderr, "USAGE: %s path\n", argv[0]);
    exit(EXIT_FAILURE);
}

void print_book(char* path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    if (!S_ISREG(path_stat.st_mode))
    {
        fprintf(stderr, "ERROR: Not a file!\n");
        return;
    }

    FILE* book;
    char* line = NULL;
    size_t size = 0;
    ssize_t nread;

    char* token;
    char* miss = "missing!\n";
    char* author = malloc(BUF_SIZE);
    author = strcpy(author, miss);
    char* title = malloc(BUF_SIZE);
    title = strcpy(title, miss);
    char* genre = malloc(BUF_SIZE);
    genre = strcpy(genre, miss);

    if ((book = fopen(path, "r")) == NULL)
        ERR("fopen");

    while ((nread = getline(&line, &size, book)) != -1)
    {
        if (strstr(line, "author:") != NULL)
        {
            free(author);
            token = strchr(line, ':');
            author = strdup(token + 1);
        }
        else if (strstr(line, "title:") != NULL)
        {
            free(title);
            token = strchr(line, ':');
            title = strdup(token + 1);
        }
        else if (strstr(line, "genre:") != NULL)
        {
            free(genre);
            token = strchr(line, ':');
            genre = strdup(token + 1);
        }
    }

    (void)token;

    printf("author: %s", author);
    printf("title: %s", title);
    printf("genre: %s", genre);

    free(line);
    free(author);
    free(title);
    free(genre);

    if (fclose(book))
        ERR("fclose");
}

int search_library(const char* name, const struct stat* s, int type, struct FTW* f)
{
    if (!S_ISREG(s->st_mode))
        return 1;

    chdir("index");
    chdir("by-visible-title");

    if (symlink(name, name))
        ERR("symlink");

    return 0;
}

int main(int argc, char** argv)
{
    if (argc > 1)
        usage(argc, argv);
    if (mkdir("index", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
        ERR("mkdir");
    chdir("index");
    if (mkdir("by-visible-title", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
        ERR("mkdir");

    chdir("..");

    nftw("library", search_library, MAX_FD, FTW_PHYS);

    return EXIT_SUCCESS;
}
