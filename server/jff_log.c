#include "jff_log.h"

void jff_log(const int facility, const char *format, ...)
{
    va_list args;

    va_start(args,format);

    vfprintf(stderr, format, args);
    fprintf(stderr,"\n");
    fflush(stderr);

    va_end(args);
}
