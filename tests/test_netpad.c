/* The network controller's server against a stand-in for the virtual pads:
 * two clients at once (two controllers on one phone) get a slot each, their
 * frames reach their own slot, and one leaving removes only its own pad.
 * With --page, prints the page it serves (to check its script). */
#include "../src/netpad.c"
#include "../src/util.h"

#include <arpa/inet.h>
#include <signal.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

/* ---- stand-ins for ps4_vpad.c ---- */

static pthread_mutex_t g_vmtx = PTHREAD_MUTEX_INITIALIZER;
static int g_taken[MAX_PADS], g_removed[MAX_PADS], g_frames[MAX_PADS];
static char g_labels[MAX_PADS][40];
static pad_state g_last[MAX_PADS];

int vpad_add(int slot)
{
    int ok;
    pthread_mutex_lock(&g_vmtx);
    ok = slot >= 0 && slot < MAX_PADS && !g_taken[slot];
    if (ok) g_taken[slot] = 1;
    pthread_mutex_unlock(&g_vmtx);
    return ok;
}
void vpad_label(int slot, const char *label) { snprintf(g_labels[slot], sizeof g_labels[slot], "%s", label); }
int vpad_live(int slot) { return slot >= 0 && slot < MAX_PADS && g_taken[slot]; }
void vpad_update(int slot, const pad_state *st)
{
    pthread_mutex_lock(&g_vmtx);
    g_last[slot] = *st;
    g_frames[slot]++;
    pthread_mutex_unlock(&g_vmtx);
}
void vpad_remove(int slot)
{
    pthread_mutex_lock(&g_vmtx);
    g_taken[slot] = 0;
    g_removed[slot]++;
    pthread_mutex_unlock(&g_vmtx);
}

/* ---- a WebSocket client, as a browser is ---- */

static int g_port;

static int ws_open(const char *path)
{
    struct sockaddr_in sin;
    char req[512], resp[512];
    int fd = socket(AF_INET, SOCK_STREAM, 0), k;
    ssize_t n;

    k = snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
                 "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                 "Sec-WebSocket-Version: 13\r\n\r\n", path);

    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons((uint16_t)g_port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&sin, sizeof sin) != 0) { close(fd); return -1; }
    send_all(fd, req, (size_t)k);
    n = recv(fd, resp, sizeof resp - 1, 0);
    resp[n > 0 ? n : 0] = '\0';
    CHECK(strncmp(resp, "HTTP/1.1 101", 12) == 0);
    /* RFC 6455's own example key and answer */
    CHECK(strstr(resp, "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != NULL);
    return fd;
}

static void ws_frame(int fd, int opcode, const uint8_t *p, size_t len)
{
    uint8_t f[2 + 4 + 125];
    static const uint8_t mask[4] = { 0x12, 0x34, 0x56, 0x78 };
    size_t i;

    f[0] = 0x80 | (uint8_t)opcode;
    f[1] = 0x80 | (uint8_t)len;
    memcpy(f + 2, mask, 4);
    for (i = 0; i < len; i++) f[6 + i] = p[i] ^ mask[i & 3];
    send_all(fd, f, 6 + len);
}

static void pad_frame(int fd, uint32_t buttons, uint8_t lx)
{
    uint8_t m[10] = { 0 };
    m[0] = (uint8_t)buttons; m[1] = (uint8_t)(buttons >> 8);
    m[2] = (uint8_t)(buttons >> 16); m[3] = (uint8_t)(buttons >> 24);
    m[4] = lx; m[5] = m[6] = m[7] = 128;
    ws_frame(fd, 0x2, m, sizeof m);
}

static void settle(void) { usleep(150000); }

static const char *name_of(const char *line)
{
    static char req[300], out[32];
    snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: x\r\n\r\n", line);
    name_from_request(req, out, sizeof out);
    return out;
}

int main(int argc, char **argv)
{
    int a, b, c, d, e, i, taken;

    if (argc > 1 && strcmp(argv[1], "--page") == 0) {
        fputs(strstr(PAGE, "\r\n\r\n") + 4, stdout);
        return 0;
    }
    signal(SIGPIPE, SIG_IGN);
    g_port = 20000 + (int)(getpid() % 20000) + 1;
    CHECK(netpad_start(g_port) == 1);
    CHECK(netpad_alive() == 1);

    printf("the controller's name from the page, cleaned and cut whole\n");
    CHECK(strcmp(name_of("/ws?n=DualSense%20Wireless%20Controller"), "DualSense Wireless Controller") == 0);
    CHECK(strcmp(name_of("/ws?n=Mando+%22X%22%5C%01%C3%B1"), "Mando X\xC3\xB1") == 0);   /* quote, backslash, control */
    CHECK(strcmp(name_of("/ws?v=1&n=Pad&w=2"), "Pad") == 0);
    CHECK(strcmp(name_of("/ws?xn=Pad"), "") == 0);                      /* not the n= parameter */
    CHECK(strcmp(name_of("/ws"), "") == 0 && strcmp(name_of("/ws?n="), "") == 0);
    CHECK(strcmp(name_of("/ws?n=%zz%4"), "%zz%4") == 0);                 /* bad escapes kept as they are */
    /* 30 bytes, then a two-byte character that does not fit in 31: left out */
    CHECK(strcmp(name_of("/ws?n=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa%C3%B1"), "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") == 0);
    CHECK(strcmp(name_of("/ws?n=aaaaaaaaaaaaaaaaaaaaaaaaaaaaa%C3%B1"), "aaaaaaaaaaaaaaaaaaaaaaaaaaaaa\xC3\xB1") == 0);

    printf("two controllers on one phone: a slot each, from the bottom, with their names\n");
    a = ws_open("/ws?n=GameSir");
    b = ws_open("/ws");
    settle();
    CHECK(g_taken[0] && g_taken[1] && !g_taken[2]);
    CHECK((strcmp(g_labels[0], "network:GameSir") == 0 && strcmp(g_labels[1], "network") == 0) ||
          (strcmp(g_labels[1], "network:GameSir") == 0 && strcmp(g_labels[0], "network") == 0));
    CHECK(netpad_clients() == 2);

    printf("each one's frames reach its own slot\n");
    pad_frame(a, PAD_CROSS, 255);
    pad_frame(b, PAD_CIRCLE | PAD_L2, 0);
    settle();
    /* which slot each socket took depends on the order the threads ran */
    {
        int sa = g_last[0].buttons == PAD_CROSS ? 0 : 1, sb = 1 - sa;
        CHECK(g_last[sa].buttons == PAD_CROSS && g_last[sa].lx == 255);
        CHECK(g_last[sb].buttons == (PAD_CIRCLE | PAD_L2) && g_last[sb].lx == 0);
    }
    CHECK(g_frames[0] == 1 && g_frames[1] == 1);

    printf("a ping is answered, a short or text frame is ignored\n");
    {
        uint8_t pong[16];
        ws_frame(a, 0x9, (const uint8_t *)"hi", 2);
        /* header and payload may come as two segments: wait for all four bytes */
        CHECK(recv(a, pong, 4, MSG_WAITALL) == 4 && pong[0] == 0x8A && pong[2] == 'h');
        ws_frame(a, 0x2, (const uint8_t *)"xy", 2);
        ws_frame(a, 0x1, (const uint8_t *)"0123456789", 10);
        settle();
        CHECK(g_frames[0] + g_frames[1] == 2);
    }

    printf("one controller unplugged: only its pad goes\n");
    {
        int before0 = g_removed[0], before1 = g_removed[1];
        ws_frame(a, 0x8, NULL, 0);
        close(a);
        settle();
        CHECK(g_removed[0] + g_removed[1] == before0 + before1 + 1);
        CHECK(g_taken[0] + g_taken[1] == 1);
        CHECK(netpad_clients() == 1);
    }

    printf("no slot left: the client is turned away, nothing is taken twice\n");
    c = ws_open("/ws");
    d = ws_open("/ws");
    e = ws_open("/ws");                      /* four slots: b, c, d hold three, the freed one */
    settle();
    for (taken = 0, i = 0; i < MAX_PADS; i++) taken += g_taken[i];
    CHECK(taken == MAX_PADS);
    {
        int f = ws_open("/ws");
        char x;
        settle();
        CHECK(recv(f, &x, 1, 0) == 0);  /* closed by the server */
        close(f);
    }

    printf("stop: every client and its pad go\n");
    netpad_stop();
    for (taken = 0, i = 0; i < MAX_PADS; i++) taken += g_taken[i];
    CHECK(taken == 0);
    CHECK(netpad_busy() == 0);                 /* every client thread let go */
    CHECK(netpad_alive() == 0);
    close(b); close(c); close(d); close(e);

    printf("served again (the network changed): a new client gets a pad and is fed\n");
    CHECK(netpad_start(g_port) == 1);
    CHECK(netpad_alive() == 1);
    a = ws_open("/ws?n=Again");
    settle();
    CHECK(g_taken[0] && strcmp(g_labels[0], "network:Again") == 0);
    pad_frame(a, PAD_TRIANGLE, 128);
    settle();
    CHECK(g_last[0].buttons == PAD_TRIANGLE);
    netpad_stop();
    CHECK(!g_taken[0] && netpad_alive() == 0);
    close(a);

    printf("its port taken by another program: tried again quietly, said once\n");
    {
        struct sockaddr_in sin;
        int other = socket(AF_INET, SOCK_STREAM, 0), k;
        unsigned long seq;
        char lines[2048];

        int on = 1;

        setsockopt(other, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);   /* the test's own TIME_WAITs */
        memset(&sin, 0, sizeof sin);
        sin.sin_family = AF_INET;
        sin.sin_port = htons((uint16_t)g_port);
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
        CHECK(bind(other, (struct sockaddr *)&sin, sizeof sin) == 0 && listen(other, 1) == 0);
        seq = log_seq();
        for (k = 0; k < 3; k++) CHECK(netpad_start(g_port) == 0);
        CHECK(log_seq() == seq + 1);
        log_since(seq, lines, sizeof lines);
        CHECK(strstr(lines, "busy") != NULL);
        CHECK(netpad_alive() == 0);
        close(other);
        seq = log_seq();
        CHECK(netpad_start(g_port) == 1);
        log_since(seq, lines, sizeof lines);
        CHECK(log_seq() == seq + 1 && strstr(lines, "free again") != NULL);
        netpad_stop();
        seq = log_seq();
        CHECK(netpad_start(g_port) == 1);              /* after a success, the plain line again */
        log_since(seq, lines, sizeof lines);
        CHECK(strstr(lines, "controllers over the network on port") != NULL);
        netpad_stop();
    }

    printf(g_fail ? "test_netpad: %d FAILED\n" : "test_netpad: all passed\n", g_fail);
    return g_fail != 0;
}
