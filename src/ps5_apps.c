#include "ps5_apps.h"
#include "launcher.h"
#include "log.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

#if defined(TARGET_PS4)
#if defined(__has_include)
#if __has_include(<ps4/kernel.h>)
#include <ps4/kernel.h>
#endif
#endif
#else
#if defined(__has_include)
#if __has_include(<ps5/kernel.h>)
#include <ps5/kernel.h>
#endif
#endif
#endif

extern const unsigned char icon_png_data[];
extern const unsigned int icon_png_data_len;

#if defined(TARGET_PS4)
int apps_install_launcher(void)
{
    return 0;
}

int apps_remove_launcher(void)
{
    return 0;
}
#else
int sceAppInstUtilInitialize(void);
int sceAppInstUtilTerminate(void);

typedef int (*install_dir_fn)(const char *title_id, const char *dir, void *reserved);
typedef int (*install_all_fn)(void *reserved);

static intptr_t find_app_inst(const char *name, const char *nid)
{
    uint32_t lib;
    intptr_t f;

    if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &lib) != 0) return 0;
    f = kernel_dynlib_dlsym(-1, lib, name);
    if (!f) f = kernel_dynlib_resolve(-1, lib, nid);
    return f;
}

static install_dir_fn find_install_dir(void)
{
    return (install_dir_fn)find_app_inst("sceAppInstUtilAppInstallTitleDir", "Wudg3Xe3heE");
}
int sceAppInstUtilAppUnInstall(const char *title_id);

int apps_install_launcher(void)
{
    struct stat st;
    install_dir_fn install_dir;
    install_all_fn install_all;
    int w, r;

    w = launcher_write(LAUNCHER_APP_ROOT, icon_png_data, icon_png_data_len);
    if (w < 0) {
        log_line("launcher: could not write the files under %s", LAUNCHER_APP_ROOT);
        return -1;
    }
    if (w == 0 && stat(LAUNCHER_APP_ROOT "/" LAUNCHER_TITLE_ID "/sce_sys/icon0.png", &st) == 0) {
        log_line("launcher: already installed and up to date");
        return 0;
    }
    install_dir = find_install_dir();
    install_all = install_dir ? NULL : (install_all_fn)find_app_inst("sceAppInstUtilAppInstallAll", "+scQA5stvjs");
    if (!install_dir && !install_all) {
        log_line("launcher: this firmware has no installer for the media-row entry; use the page address");
        return -1;
    }
    if ((r = sceAppInstUtilInitialize()) != 0) {
        log_line("launcher: sceAppInstUtilInitialize -> %#x", (unsigned)r);
        return -1;
    }
    if (install_dir) {
        r = install_dir(LAUNCHER_TITLE_ID, LAUNCHER_APP_ROOT "/", 0);
    } else {
        /* Registers every app folder under /user/app, ours among them. */
        r = install_all(0);
        log_line("launcher: by the install-all fallback");
    }
    sceAppInstUtilTerminate();
    log_line("launcher: installed %s in the media row -> %#x", LAUNCHER_TITLE_ID, (unsigned)r);
    return r == 0 ? 0 : -1;
}

int apps_remove_launcher(void)
{
    int r = sceAppInstUtilInitialize();

    if (r == 0) {
        r = sceAppInstUtilAppUnInstall(LAUNCHER_TITLE_ID);
        sceAppInstUtilTerminate();
    }
    log_line("launcher: removed %s -> %#x", LAUNCHER_TITLE_ID, (unsigned)r);
    launcher_remove_files(LAUNCHER_APP_ROOT);
    return r == 0 ? 0 : -1;
}
#endif
