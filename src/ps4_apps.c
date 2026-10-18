#include "ps4_apps.h"
#include "launcher.h"
#include "log.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#if defined(__has_include)
#if __has_include(<ps4/kernel.h>)
#include <ps4/kernel.h>
#endif
#endif

extern const unsigned char icon_png_data[];
extern const unsigned int icon_png_data_len;

int apps_install_launcher(void)
{
    return 0;
}

int apps_remove_launcher(void)
{
    return 0;
}
