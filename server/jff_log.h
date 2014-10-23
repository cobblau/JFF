#include "jff_config.h"


#define JFF_DEBUG   0
#define JFF_INFO    1
#define JFF_WARNING 2
#define JFF_ERROR   3
#define JFF_FATAL   4

void jff_log(const int facility, const char *format, ...);
