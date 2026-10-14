/* Things AnyPad PS5 shows on the console itself. */
#ifndef ANYPAD_PS5_UI_H
#define ANYPAD_PS5_UI_H

/* Opens the console's web browser at `url`, as Payload Manager does for its
 * own page. Returns 0 on success. */
int ui_open_browser(const char *url);

/* The console's system language (SCE language code), or -1 if it cannot be
 * read. */
int ui_console_language(void);

/* Spanish (Spain or Latin America)? The texts are Spanish or English. */
int ui_language_is_spanish(int code);

/* The menu page's language for the console's: es en fr it pt de ru zh zh-Hant
 * ja ko ar; English for any other. */
const char *ui_language_tag(int code);

#endif
