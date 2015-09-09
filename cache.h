#include <stdlib.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

struct proxy_line{
    int size;
    int age;
    char *buf;
    char *name;
    struct proxy_line * next;

};
typedef struct proxy_line proxy_line;

void insert(char *buf, char *name);

proxy_line* search_cache(char *name);
