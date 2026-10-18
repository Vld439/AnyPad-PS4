#include "ps4_ui.h"
#include "log.h"

int sceSystemServiceLaunchWebBrowser(const char *uri, void *reserved);
int sceSystemServiceParamGetInt(int param_id, int *value);

#define PARAM_ID_LANG 1             /* system language */

int ui_open_browser(const char *url)
{
    int r = sceSystemServiceLaunchWebBrowser(url, 0);

    log_line("browser: %s -> %#x", url, (unsigned)r);
    return r;
}

int ui_console_language(void)
{
    int v = -1, r = sceSystemServiceParamGetInt(PARAM_ID_LANG, &v);

    log_line("console: language %d (read -> %#x)", v, (unsigned)r);
    return r == 0 ? v : -1;
}

int ui_language_is_spanish(int code)
{
    return code == 3 || code == 20;     /* Spanish, Spanish (Latin America) */
}

const char *ui_language_tag(int code)
{
    switch (code) {
    case 0:  return "ja";
    case 2: case 22: return "fr";       /* French, French (Canada) */
    case 3: case 20: return "es";       /* Spanish, Spanish (Latin America) */
    case 4:  return "de";
    case 5:  return "it";
    case 7: case 17: return "pt";       /* Portuguese, Portuguese (Brazil) */
    case 8:  return "ru";
    case 9:  return "ko";
    case 10: return "zh-Hant";          /* Chinese (traditional) */
    case 11: return "zh";               /* Chinese (simplified) */
    case 21: return "ar";
    default: return "en";
    }
}
