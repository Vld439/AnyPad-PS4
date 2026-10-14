/* The launcher folder: what is written, that it is idempotent, and removal. */
#include "launcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern const unsigned char icon_png_data[];
extern const unsigned int icon_png_data_len;

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static long size_of(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 ? (long)st.st_size : -1;
}

int main(void)
{
    char root[] = "/tmp/anypad-launcher-XXXXXX";
    char path[300], param[512], got[512];
    unsigned char bad[4] = { 1, 2, 3, 4 };
    FILE *f;
    int n;

    CHECK(mkdtemp(root) != NULL);

    printf("the icon is a PNG\n");
    CHECK(icon_png_data_len > 1000 && memcmp(icon_png_data, "\x89PNG\r\n\x1a\n", 8) == 0);

    printf("param.json carries the id, the name and the deep link\n");
    n = launcher_param_json(param, sizeof param);
    CHECK(n > 0 && strstr(param, "\"titleId\": \"ANYP00001\"") && strstr(param, "\"deeplinkUri\": \"http://127.0.0.1:8095/\"") &&
          strstr(param, "\"titleName\": \"AnyPad\"") && strstr(param, "65536"));
    CHECK(launcher_param_json(param, 20) == -1);

    printf("first write creates both files\n");
    CHECK(launcher_write(root, icon_png_data, icon_png_data_len) == 1);
    snprintf(path, sizeof path, "%s/ANYP00001/sce_sys/icon0.png", root);
    CHECK(size_of(path) == (long)icon_png_data_len);
    snprintf(path, sizeof path, "%s/ANYP00001/sce_sys/param.json", root);
    f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f) { n = (int)fread(got, 1, sizeof got, f); fclose(f); CHECK(n > 0 && strstr(got, "ANYP00001")); }

    printf("second write changes nothing\n");
    CHECK(launcher_write(root, icon_png_data, icon_png_data_len) == 0);

    printf("a different or damaged icon is rewritten\n");
    snprintf(path, sizeof path, "%s/ANYP00001/sce_sys/icon0.png", root);
    f = fopen(path, "wb"); fwrite(bad, 1, 4, f); fclose(f);
    CHECK(launcher_write(root, icon_png_data, icon_png_data_len) == 1);
    CHECK(size_of(path) == (long)icon_png_data_len);

    printf("a root that cannot be written is an error\n");
    CHECK(launcher_write("/nonexistent-anypad-root/x", icon_png_data, icon_png_data_len) == -1);

    printf("removal deletes the files and the folders\n");
    launcher_remove_files(root);
    snprintf(path, sizeof path, "%s/ANYP00001", root);
    CHECK(size_of(path) == -1);
    rmdir(root);

    printf(fails ? "launcher: %d FAILED\n" : "launcher: all ok\n", fails);
    return fails != 0;
}
