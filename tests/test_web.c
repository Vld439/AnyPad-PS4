/* The web page's server against stand-in pads: the state it reports, what it
 * refuses, and the two things it can change (PS on a pad, stopping). */
#include "../src/web.h"
#include "../src/log.h"
#include "../src/util.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* A port per run, so two runs at once do not collide. */
static int g_port;
static web_t *g_web;

/* Slot 3: a USB pad holding cross with the left stick pushed right. Slot 0:
 * a network client at rest. The others: nothing. */
static int fake_pad_get(int slot, char *label, size_t max, pad_state *st)
{
    if (slot != 0 && slot != 3) return 0;
    pad_state_reset(st);
    if (slot == 3) {
        snprintf(label, max, "Switch \"Pro\"");   /* a quote must not break the JSON */
        st->buttons = PAD_CROSS;
        st->lx = 255;
    } else {
        snprintf(label, max, "network");
    }
    return 1;
}

static int g_clients = 1;
static int fake_net_clients(void) { return g_clients; }

static int g_ps_calls, g_ps_slot = -1;
static int fake_press_ps(int slot)
{
    g_ps_calls++;
    g_ps_slot = slot;
    return slot == 0 || slot == 3;
}

/* Sends a raw request and serves it; returns the response (status line on). */
static int request(const char *raw, char *out, int max)
{
    struct sockaddr_in sin;
    int fd = socket(AF_INET, SOCK_STREAM, 0), n = 0, i;

    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons((uint16_t)g_port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&sin, sizeof sin) != 0) {
        close(fd);
        return -1;
    }
    send(fd, raw, strlen(raw), 0);
    for (i = 0; i < 400; i++) {             /* the server runs in this thread */
        ssize_t k;
        web_poll(g_web);
        k = recv(fd, out + n, (size_t)(max - 1 - n), MSG_DONTWAIT);
        if (k > 0) n += (int)k;
        else if (k == 0) break;
        usleep(1000);
    }
    out[n] = '\0';
    close(fd);
    return n;
}

static int call(const char *method, const char *path, char *out, int max)
{
    char raw[512];
    snprintf(raw, sizeof raw, "%s %s HTTP/1.1\r\nHost: 192.168.1.50:8095\r\nX-AnyPad: 1\r\n\r\n", method, path);
    return request(raw, out, max);
}

int main(void)
{
    static char out[70000];
    volatile int stop = 0;
    web_cfg wc;

    signal(SIGPIPE, SIG_IGN);
    g_port = 20000 + (int)(getpid() % 20000);
    memset(&wc, 0, sizeof wc);
    wc.stop = &stop;
    wc.log_path = "build/test_web.log";
    wc.version = "9.9.9-test";
    wc.lang = "es";
    wc.net_url = "http://192.168.1.50:8096/";
    wc.net_clients = fake_net_clients;
    wc.pad_get = fake_pad_get;
    wc.press_ps = fake_press_ps;
    log_open(wc.log_path);
    g_web = web_start(&wc, g_port);
    CHECK(g_web != NULL);
    if (!g_web) return 1;

    printf("the page\n");
    CHECK(call("GET", "/", out, sizeof out) > 0);
    CHECK(strstr(out, "HTTP/1.1 200") == out);
    CHECK(strstr(out, "AnyPad PS4") != NULL);
    CHECK(strstr(out, "/api/pair") == NULL);         /* no pairing left on the page */

    printf("state: version, network controller, live pads only\n");
    call("GET", "/api/state", out, sizeof out);
    CHECK(strstr(out, "\"version\":\"9.9.9-test\"") != NULL);
    CHECK(strstr(out, "\"net\":{\"url\":\"http://192.168.1.50:8096/\",\"clients\":1}") != NULL);
    CHECK(strstr(out, "{\"slot\":1,\"label\":\"network\"") != NULL);
    CHECK(strstr(out, "{\"slot\":4,\"label\":\"Switch Pro\",\"lx\":255,") != NULL);
    CHECK(strstr(out, "\"buttons\":[\"cross\"]") != NULL);
    CHECK(strstr(out, "\"slot\":2") == NULL && strstr(out, "\"slot\":3") == NULL);
    CHECK(strstr(out, "bluetooth") == NULL && strstr(out, "paired") == NULL);
    g_clients = 0;
    call("GET", "/api/state", out, sizeof out);
    CHECK(strstr(out, "\"clients\":0") != NULL);

    printf("what is refused\n");
    request("POST /api/ps?slot=1 HTTP/1.1\r\nHost: 192.168.1.50:8095\r\n\r\n", out, sizeof out);
    CHECK(strstr(out, "403") != NULL && g_ps_calls == 0);           /* no X-AnyPad: another site */
    request("GET /api/state HTTP/1.1\r\nHost: evil.example:8095\r\nX-AnyPad: 1\r\n\r\n", out, sizeof out);
    CHECK(strstr(out, "400") != NULL);                               /* a name, not an address */
    call("POST", "/api/pair", out, sizeof out);
    CHECK(strstr(out, "404") != NULL);                               /* gone with Bluetooth */
    call("POST", "/api/ps?slot=9", out, sizeof out);
    CHECK(strstr(out, "400") != NULL && g_ps_calls == 0);

    printf("PS on a pad (1-based on the wire, 0-based inside)\n");
    call("POST", "/api/ps?slot=4", out, sizeof out);
    CHECK(strstr(out, "200") != NULL && g_ps_calls == 1 && g_ps_slot == 3);
    call("POST", "/api/ps?slot=2", out, sizeof out);
    CHECK(strstr(out, "404") != NULL && g_ps_slot == 1);            /* no pad there */

    printf("stop\n");
    CHECK(stop == 0);
    call("POST", "/api/stop", out, sizeof out);
    CHECK(strstr(out, "200") != NULL && stop == 1);

    web_stop(g_web);
    log_close();
    printf(g_fail ? "test_web: %d FAILED\n" : "test_web: all passed\n", g_fail);
    return g_fail != 0;
}
