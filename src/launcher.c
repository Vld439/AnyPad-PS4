#include "launcher.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int launcher_param_json(char *buf, size_t size)
{
    int n = snprintf(buf, size,
        "{\n"
        "    \"titleId\": \"" LAUNCHER_TITLE_ID "\",\n"
        "    \"applicationCategoryType\": 65536,\n"
        "    \"deeplinkUri\": \"" LAUNCHER_URL "\",\n"
        "    \"localizedParameters\": {\n"
        "        \"defaultLanguage\": \"en-US\",\n"
        "        \"en-US\": {\n"
        "            \"titleName\": \"" LAUNCHER_TITLE_NAME "\"\n"
        "        }\n"
        "    }\n"
        "}\n");

    return n < 0 || (size_t)n >= size ? -1 : n;
}

static int same_file(const char *path, const unsigned char *want, size_t len)
{
    struct stat st;
    unsigned char *got;
    FILE *f;
    int same;

    if (stat(path, &st) != 0 || (size_t)st.st_size != len) return 0;
    if (!(f = fopen(path, "rb"))) return 0;
    got = malloc(len ? len : 1);
    same = got && fread(got, 1, len, f) == len && memcmp(got, want, len) == 0;
    free(got);
    fclose(f);
    return same;
}

static int put_file(const char *path, const unsigned char *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    int ok;

    if (!f) return -1;
    ok = fwrite(data, 1, len, f) == len;
    ok = fclose(f) == 0 && ok;
    return ok ? 0 : -1;
}

static int make_dir(const char *path)
{
    return mkdir(path, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

int launcher_write(const char *root, const unsigned char *icon, size_t icon_len)
{
    char dir[256], sys[300], pj[340], ic[340], param[512];
    int plen, changed = 0;

    snprintf(dir, sizeof dir, "%s/" LAUNCHER_TITLE_ID, root);
    snprintf(sys, sizeof sys, "%s/sce_sys", dir);
    snprintf(pj, sizeof pj, "%s/param.json", sys);
    snprintf(ic, sizeof ic, "%s/icon0.png", sys);
    if ((plen = launcher_param_json(param, sizeof param)) < 0) return -1;

    if (make_dir(dir) || make_dir(sys)) return -1;
    if (!same_file(pj, (const unsigned char *)param, (size_t)plen)) {
        if (put_file(pj, (const unsigned char *)param, (size_t)plen)) return -1;
        changed = 1;
    }
    if (!same_file(ic, icon, icon_len)) {
        if (put_file(ic, icon, icon_len)) return -1;
        changed = 1;
    }
    return changed;
}

int launcher_remove_files(const char *root)
{
    char dir[256], sys[300], pj[340], ic[340];

    snprintf(dir, sizeof dir, "%s/" LAUNCHER_TITLE_ID, root);
    snprintf(sys, sizeof sys, "%s/sce_sys", dir);
    snprintf(pj, sizeof pj, "%s/param.json", sys);
    snprintf(ic, sizeof ic, "%s/icon0.png", sys);
    unlink(pj);
    unlink(ic);
    rmdir(sys);
    rmdir(dir);
    return 0;
}
