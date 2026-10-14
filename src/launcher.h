/* The launcher entry in the console's "Contenido multimedia" row: a small
 * app folder (param.json + icon0.png) whose only job is to open AnyPad's web
 * menu in the browser, the way Payload Manager's own entry does. Writing the
 * files is plain file work and is tested here; telling the system about the
 * folder is ps5_apps.c. */
#ifndef ANYPAD_LAUNCHER_H
#define ANYPAD_LAUNCHER_H

#include <stddef.h>

#define LAUNCHER_TITLE_ID   "ANYP00001"
#define LAUNCHER_TITLE_NAME "AnyPad"
#define LAUNCHER_URL        "http://127.0.0.1:8095/"
#define LAUNCHER_APP_ROOT   "/user/app"

/* Builds param.json into buf. Returns its length, or -1 if it does not fit. */
int launcher_param_json(char *buf, size_t size);

/* Makes <root>/<title id>/sce_sys/ and writes param.json and icon0.png there,
 * each only when missing or different. Returns 1 if anything was written,
 * 0 if everything was already as wanted, -1 on error. */
int launcher_write(const char *root, const unsigned char *icon, size_t icon_len);

/* Removes the two files and the folders (only what launcher_write made). */
int launcher_remove_files(const char *root);

#endif
