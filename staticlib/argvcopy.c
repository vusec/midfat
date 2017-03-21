#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char **strarrcopy(char **arr) {
    int count = 0, i;
    char **newarr;

    if (!arr) return NULL;

    while (arr[count]) count++;

    newarr = malloc(sizeof(char *) * (count + 1));
    if (!newarr) {
	perror("argvcopy: malloc failed");
	exit(-1);
    }
    for (i = 0; i < count; i++) {
	if (!(newarr[i] = strdup(arr[i]))) {
	    perror("argvcopy: strdup failed");
	    exit(-1);
	}
    }
    return newarr;
}

void argvcopy(char ***argvptr, char ***envpptr) {
    extern char **environ;

    if (argvptr) *argvptr = strarrcopy(*argvptr);
    assert(!envpptr || !*envpptr || environ == *envpptr);
    environ = strarrcopy(environ);
    if (envpptr) *envpptr = environ;
}
