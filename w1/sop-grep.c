#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(int argc, char* argv[])
{
    printf("%s pattern\n", argv[0]);
    printf("pattern - string pattern to search at standard input\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        usage(argc, argv);
    }

    char* pattern = argv[1];
    char* env = getenv("W1_LINENUMBER");
    int x = 0;
    int idx = 1;

    FILE* fptr = NULL;

    if (env != NULL && (strcmp(env, "true") == 0 || strcmp(env, "true") == 0 || strcmp(env, "1") == 0))
    {
        x = 1;
    }

    char* env2 = getenv("W1_LOGFILE");
    if (env2 != NULL)
    {
        fptr = fopen(env2, "w");
    }

    char* line = NULL;
    size_t line_len = 0;
    while (getline(&line, &line_len, stdin) != -1)  // man 3p getdelim
    {
        if (strstr(line, pattern))
        {
            if (x)
            {
                printf("%d:", idx);
                if (fptr)
                    fprintf(fptr, "%d:", idx);
            }

            printf(line);
            if (fptr)
                fprintf(fptr, line);
        }
        idx++;
    }

    if (fptr)
    {
        fclose(fptr);
    }

    if (line)
        free(line);

    return EXIT_SUCCESS;
}
