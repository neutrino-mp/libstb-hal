#if HAVE_TRIPLEDRAGON
#include "../libtriple/pwrmngr.h"
#elif HAVE_SPARK_HARDWARE
#include "../libspark/pwrmngr.h"
#else
#error neither HAVE_TRIPLEDRAGON nor HAVE_SPARK_HARDWARE defined
#endif