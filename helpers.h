#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stdout, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		2048	// dimensiunea maxima a calupului de date
#define END_INPUT	"COMMAND_DONE"
int endCommand(const char *str, int size)
{
    if (!str)
        return 0;
    size_t lensuffix = strlen(END_INPUT);
    if (lensuffix >  size)
	{
		return 0;
	}
    return strncmp(str + size - lensuffix, END_INPUT, lensuffix) == 0;
}
#endif
