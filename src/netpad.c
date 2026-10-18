/* Controllers over the local network: see netpad.h. A browser reads a real
 * gamepad and sends its state here over a WebSocket; each client drives one
 * virtual pad, fed through the same vpad_* path the USB pads use. */
#include "netpad.h"
#include "pad.h"
#include "ps4_vpad.h"
#include "log.h"
#include "nosleep_media.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>

/* ---- SHA-1 (RFC 3174), only for the WebSocket handshake ------------------- */

typedef struct { uint32_t h[5]; uint64_t len; uint8_t buf[64]; size_t n; } sha1_t;

static uint32_t rol32(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

static void sha1_block(sha1_t *s, const uint8_t *p)
{
    uint32_t w[80], a, b, c, d, e, f, k, t;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i*4] << 24 | (uint32_t)p[i*4+1] << 16 |
               (uint32_t)p[i*4+2] << 8 | p[i*4+3];
    for (i = 16; i < 80; i++)
        w[i] = rol32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4];
    for (i = 0; i < 80; i++) {
        if      (i < 20) { f = (b & c) | (~b & d);            k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                     k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);   k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                     k = 0xCA62C1D6; }
        t = rol32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol32(b, 30); b = a; a = t;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}

static void sha1_init(sha1_t *s)
{
    s->h[0] = 0x67452301; s->h[1] = 0xEFCDAB89; s->h[2] = 0x98BADCFE;
    s->h[3] = 0x10325476; s->h[4] = 0xC3D2E1F0;
    s->len = 0; s->n = 0;
}

static void sha1_push(sha1_t *s, const void *data, size_t len)
{
    const uint8_t *p = data;

    s->len += len;
    while (len) {
        size_t take = 64 - s->n;
        if (take > len) take = len;
        memcpy(s->buf + s->n, p, take);
        s->n += take; p += take; len -= take;
        if (s->n == 64) { sha1_block(s, s->buf); s->n = 0; }
    }
}

static void sha1_done(sha1_t *s, uint8_t out[20])
{
    uint64_t bits = s->len * 8;
    uint8_t pad = 0x80;
    uint8_t lenbe[8];
    int i;

    sha1_push(s, &pad, 1);
    pad = 0;
    while (s->n != 56) sha1_push(s, &pad, 1);
    for (i = 0; i < 8; i++) lenbe[i] = (uint8_t)(bits >> (56 - i*8));
    sha1_push(s, lenbe, 8);
    for (i = 0; i < 5; i++) {
        out[i*4]   = (uint8_t)(s->h[i] >> 24);
        out[i*4+1] = (uint8_t)(s->h[i] >> 16);
        out[i*4+2] = (uint8_t)(s->h[i] >> 8);
        out[i*4+3] = (uint8_t)s->h[i];
    }
}

static void base64(const uint8_t *in, size_t n, char *out)
{
    static const char t[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0, o = 0;

    while (i + 3 <= n) {
        uint32_t v = in[i] << 16 | in[i+1] << 8 | in[i+2];
        out[o++] = t[(v >> 18) & 63]; out[o++] = t[(v >> 12) & 63];
        out[o++] = t[(v >> 6) & 63];  out[o++] = t[v & 63];
        i += 3;
    }
    if (n - i == 1) {
        uint32_t v = in[i] << 16;
        out[o++] = t[(v >> 18) & 63]; out[o++] = t[(v >> 12) & 63];
        out[o++] = '='; out[o++] = '=';
    } else if (n - i == 2) {
        uint32_t v = in[i] << 16 | in[i+1] << 8;
        out[o++] = t[(v >> 18) & 63]; out[o++] = t[(v >> 12) & 63];
        out[o++] = t[(v >> 6) & 63];  out[o++] = '=';
    }
    out[o] = '\0';
}

/* ---- the page the browser opens ------------------------------------------- */

static const char PAGE[] =
"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
"<!doctype html><html lang=es><head><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>AnyPad &mdash; mando por red</title><style>"
"*{box-sizing:border-box}body{margin:0;font:16px system-ui,sans-serif;background:#0b0f1a;color:#e7ecf5;"
"min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:24px;gap:18px}"
"h1{font-size:1.3rem;margin:0}.card{background:#141b2e;border:1px solid #243049;border-radius:14px;"
"padding:20px;max-width:440px;width:100%;text-align:center}#dot{display:inline-block;width:10px;height:10px;"
"border-radius:50%;background:#c33;margin-right:8px;vertical-align:middle}.on #dot{background:#3c3}"
"#pad{font-weight:600;margin:10px 0}small{color:#8a97b2;display:block;line-height:1.5;margin-top:12px}"
"b{color:#9db4ff}</style></head><body>"
"<h1>AnyPad &mdash; mando por red</h1>"
"<div class=card id=card><div><span id=dot></span><span id=st>Conectando&hellip;</span></div>"
"<div id=pad>Conecta un mando a este dispositivo y pulsa un bot&oacute;n</div>"
"<small>El navegador lee tu mando (incluso por <b>Bluetooth</b> emparejado a este tel&eacute;fono) y lo "
"env&iacute;a a la consola como un mando virtual. <b>Toca la pantalla una vez</b> para que no se apague "
"mientras juegas; baja el brillo al m&iacute;nimo para ahorrar bater&iacute;a.</small></div>"
"<script>"
/* Keep the screen awake. Wake Lock API when the context is secure; otherwise a
 * muted looping clip (NoSleep.js trick), which is all that works over http. */
"var WEBM='" NOSLEEP_WEBM "';var MP4='" NOSLEEP_MP4 "';"
"var vid=document.createElement('video');vid.setAttribute('playsinline','');vid.setAttribute('muted','');"
"vid.muted=true;vid.loop=true;vid.style.cssText='position:fixed;width:1px;height:1px;opacity:0;pointer-events:none';"
"var sw=document.createElement('source');sw.src='data:video/webm;base64,'+WEBM;sw.type='video/webm';"
"var sm=document.createElement('source');sm.src='data:video/mp4;base64,'+MP4;sm.type='video/mp4';"
"vid.appendChild(sw);vid.appendChild(sm);document.body.appendChild(vid);var wl=null;"
"function keepAwake(){if(navigator.wakeLock){navigator.wakeLock.request('screen').then(function(w){wl=w;}).catch(function(){});}"
"vid.play().catch(function(){});}"
"['click','touchstart','keydown'].forEach(function(e){window.addEventListener(e,keepAwake,{passive:true});});"
"document.addEventListener('visibilitychange',function(){if(document.visibilityState==='visible')keepAwake();});"
"var B={CREATE:1,L3:2,R3:4,OPTIONS:8,UP:16,RIGHT:32,DOWN:64,LEFT:128,L2:256,R2:512,L1:1024,R1:2048,"
"TRIANGLE:4096,CIRCLE:8192,CROSS:16384,SQUARE:32768,PS:65536};"
"var map=[B.CROSS,B.CIRCLE,B.SQUARE,B.TRIANGLE,B.L1,B.R1,0,0,B.CREATE,B.OPTIONS,B.L3,B.R3,"
"B.UP,B.DOWN,B.LEFT,B.RIGHT,B.PS];"
"var card=document.getElementById('card'),st=document.getElementById('st'),padEl=document.getElementById('pad');"
"var ws,open=false;"
"function connect(){ws=new WebSocket('ws://'+location.host+'/ws');ws.binaryType='arraybuffer';"
"ws.onopen=function(){open=true;card.className='on';st.textContent='Conectado a la consola';};"
"ws.onclose=function(){open=false;card.className='';st.textContent='Sin conexi\\u00f3n, reintentando\\u2026';setTimeout(connect,1000);};"
"ws.onerror=function(){try{ws.close()}catch(e){}};}"
"connect();"
"function axis(v){return Math.max(0,Math.min(255,Math.round((v+1)*127.5)));}"
"function loop(){requestAnimationFrame(loop);if(!open)return;"
"var gp=navigator.getGamepads(),g=null,i;for(i=0;i<gp.length;i++){if(gp[i]){g=gp[i];break;}}"
"if(!g){padEl.textContent='Conecta un mando y pulsa un bot\\u00f3n';return;}"
"padEl.textContent=g.id;"
"var btn=0,l2=0,r2=0,b=g.buttons,a=g.axes;"
"for(i=0;i<map.length&&i<b.length;i++){if(b[i]&&b[i].pressed)btn|=map[i];}"
"if(b[6]){l2=Math.round((b[6].value||0)*255);if(b[6].pressed||l2>30)btn|=B.L2;}"
"if(b[7]){r2=Math.round((b[7].value||0)*255);if(b[7].pressed||r2>30)btn|=B.R2;}"
"var buf=new ArrayBuffer(10),d=new DataView(buf);"
"d.setUint32(0,btn>>>0,true);"
"d.setUint8(4,axis(a[0]||0));d.setUint8(5,axis(a[1]||0));"
"d.setUint8(6,axis(a[2]||0));d.setUint8(7,axis(a[3]||0));"
"d.setUint8(8,l2);d.setUint8(9,r2);"
"try{ws.send(buf);}catch(e){}}"
"requestAnimationFrame(loop);"
"window.addEventListener('gamepadconnected',function(e){padEl.textContent=e.gamepad.id;});"
"</script></body></html>";

static const char NOTFOUND[] =
"HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";

/* ---- socket helpers ------------------------------------------------------- */

static int read_n(int fd, void *buf, size_t n)
{
    uint8_t *p = buf;

    while (n) {
        ssize_t r = recv(fd, p, n, 0);
        if (r <= 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

static int send_all(int fd, const void *buf, size_t n)
{
    const uint8_t *p = buf;

    while (n) {
        ssize_t w = send(fd, p, n, 0);
        if (w <= 0) return -1;
        p += w; n -= (size_t)w;
    }
    return 0;
}

/* One WebSocket data frame from the client (always masked). Returns the
 * payload length and its opcode, or -1 on error / close. Fragmented or
 * over-long frames are treated as errors; browsers send neither here. */
static int ws_recv(int fd, int *opcode, uint8_t *out, size_t max)
{
    uint8_t hdr[2], mask[4];
    uint64_t len;
    size_t i;

    if (read_n(fd, hdr, 2) != 0) return -1;
    *opcode = hdr[0] & 0x0f;
    if (!(hdr[1] & 0x80)) return -1;            /* client frames must be masked */
    len = hdr[1] & 0x7f;
    if (len == 126) {
        uint8_t e[2];
        if (read_n(fd, e, 2) != 0) return -1;
        len = (uint64_t)e[0] << 8 | e[1];
    } else if (len == 127) {
        return -1;                              /* far larger than anything we read */
    }
    if (read_n(fd, mask, 4) != 0) return -1;
    if (len > max) return -1;
    if (len && read_n(fd, out, (size_t)len) != 0) return -1;
    for (i = 0; i < len; i++) out[i] ^= mask[i & 3];
    return (int)len;
}

static int ws_send(int fd, int opcode, const void *data, size_t len)
{
    uint8_t hdr[2];

    if (len > 125) return -1;                   /* all we ever send is tiny */
    hdr[0] = 0x80 | (uint8_t)opcode;
    hdr[1] = (uint8_t)len;
    if (send_all(fd, hdr, 2) != 0) return -1;
    return len ? send_all(fd, data, len) : 0;
}

/* ---- clients -------------------------------------------------------------- */

#define MAX_CLIENTS 8

static struct { int fd; int active; int slot; } g_cli[MAX_CLIENTS];
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_slot_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_listener;
static int g_listen_fd = -1;
static volatile int g_run;

static void close_fd(int i)
{
    pthread_mutex_lock(&g_mtx);
    if (g_cli[i].fd >= 0) { shutdown(g_cli[i].fd, SHUT_RDWR); close(g_cli[i].fd); g_cli[i].fd = -1; }
    pthread_mutex_unlock(&g_mtx);
}

/* Takes the lowest free virtual-pad slot (USB takes them from the top, so the
 * two do not collide until all four are in use). -1 if none is free. */
static int claim_slot(void)
{
    int s, got = -1;

    pthread_mutex_lock(&g_slot_mtx);
    for (s = 0; s < MAX_PADS; s++)
        if (vpad_add(s)) { vpad_label(s, "network"); got = s; break; }
    pthread_mutex_unlock(&g_slot_mtx);
    return got;
}

/* Reads the HTTP request head into buf (NUL-terminated). Returns its length,
 * or -1. Stops at the blank line that ends the headers. */
static int read_request(int fd, char *buf, size_t max)
{
    size_t n = 0;

    while (n < max - 1) {
        ssize_t r = recv(fd, buf + n, 1, 0);
        if (r <= 0) return -1;
        n++;
        if (n >= 4 && memcmp(buf + n - 4, "\r\n\r\n", 4) == 0) break;
    }
    buf[n] = '\0';
    return (int)n;
}

/* Case-insensitive search for a header value. Writes it (trimmed) into out. */
static int header(const char *req, const char *name, char *out, size_t max)
{
    const char *p = req;
    size_t nl = strlen(name);

    while (*p) {
        if (strncasecmp(p, name, nl) == 0) {
            p += nl;
            while (*p == ' ' || *p == ':') p++;
            size_t i = 0;
            while (p[i] && p[i] != '\r' && p[i] != '\n' && i < max - 1) { out[i] = p[i]; i++; }
            out[i] = '\0';
            return 1;
        }
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
    return 0;
}

static void run_pad(int fd, int i)
{
    int slot = claim_slot();
    pad_state st;
    uint8_t msg[256];
    int op, len, announced = 0;

    if (slot < 0) {
        log_line("netpad: a client connected but every slot is in use");
        return;
    }
    pthread_mutex_lock(&g_mtx);
    g_cli[i].slot = slot;
    pthread_mutex_unlock(&g_mtx);
    log_line("netpad: client on slot %d", slot);

    while (g_run) {
        len = ws_recv(fd, &op, msg, sizeof msg);
        if (len < 0) break;
        if (op == 0x8) break;                       /* close */
        if (op == 0x9) { ws_send(fd, 0xA, msg, (size_t)len); continue; }   /* ping -> pong */
        if (op != 0x2 || len < 10) continue;        /* only our binary state */

        pad_state_reset(&st);
        st.buttons = (uint32_t)msg[0] | (uint32_t)msg[1] << 8 |
                     (uint32_t)msg[2] << 16 | (uint32_t)msg[3] << 24;
        st.lx = msg[4]; st.ly = msg[5]; st.rx = msg[6]; st.ry = msg[7];
        st.l2 = msg[8]; st.r2 = msg[9];

        if (!announced && vpad_live(slot)) {
            announced = 1;
            log_line("netpad: slot %d live, driven by a network client", slot);
        }
        vpad_update(slot, &st);
    }
    log_line("netpad: client on slot %d left", slot);
    vpad_remove(slot);
}

static void *client_main(void *arg)
{
    int i = (int)(intptr_t)arg;
    int fd;
    char req[4096], key[128], accept_b64[40];

    pthread_detach(pthread_self());
    pthread_mutex_lock(&g_mtx);
    fd = g_cli[i].fd;
    pthread_mutex_unlock(&g_mtx);

    if (read_request(fd, req, sizeof req) < 0) goto done;

    if (header(req, "Sec-WebSocket-Key", key, sizeof key)) {
        sha1_t s;
        uint8_t dig[20];
        char resp[256];
        int n;

        sha1_init(&s);
        sha1_push(&s, key, strlen(key));
        sha1_push(&s, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
        sha1_done(&s, dig);
        base64(dig, 20, accept_b64);
        n = snprintf(resp, sizeof resp,
                     "HTTP/1.1 101 Switching Protocols\r\n"
                     "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n", accept_b64);
        if (send_all(fd, resp, (size_t)n) == 0)
            run_pad(fd, i);
    } else if (strncmp(req, "GET / ", 6) == 0 || strncmp(req, "GET /index.html ", 16) == 0) {
        send_all(fd, PAGE, sizeof PAGE - 1);
    } else {
        send_all(fd, NOTFOUND, sizeof NOTFOUND - 1);
    }

done:
    close_fd(i);
    pthread_mutex_lock(&g_mtx);
    g_cli[i].active = 0;
    g_cli[i].slot = -1;
    pthread_mutex_unlock(&g_mtx);
    return NULL;
}

/* Loopback or a private range, as web.c requires too: the page, and the
 * controller it drives, are for the local network only. */
static int peer_is_local(const struct sockaddr_in *sin)
{
    uint32_t a = ntohl(sin->sin_addr.s_addr);

    return (a >> 24) == 127 || (a >> 24) == 10 || (a >> 20) == 0xAC1 ||
           (a >> 16) == 0xC0A8 || (a >> 16) == 0xA9FE;
}

static void *listener_main(void *arg)
{
    (void)arg;
    while (g_run) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof peer;
        int nfd = accept(g_listen_fd, (struct sockaddr *)&peer, &plen);
        int i, placed = -1;

        if (nfd < 0) {
            if (g_run) continue;
            break;
        }
        if (plen < (socklen_t)sizeof peer || !peer_is_local(&peer)) {
            close(nfd);                 /* not from the local network */
            continue;
        }
        pthread_mutex_lock(&g_mtx);
        for (i = 0; i < MAX_CLIENTS; i++)
            if (!g_cli[i].active) { g_cli[i].active = 1; g_cli[i].fd = nfd; g_cli[i].slot = -1; placed = i; break; }
        pthread_mutex_unlock(&g_mtx);

        if (placed < 0) { close(nfd); continue; }
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_main, (void *)(intptr_t)placed) != 0) {
            close_fd(placed);
            pthread_mutex_lock(&g_mtx);
            g_cli[placed].active = 0;
            pthread_mutex_unlock(&g_mtx);
        }
    }
    return NULL;
}

int netpad_start(int port)
{
    struct sockaddr_in a;
    int fd, on = 1, i;

    for (i = 0; i < MAX_CLIENTS; i++) { g_cli[i].fd = -1; g_cli[i].slot = -1; }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { log_line("netpad: socket failed (%d)", errno); return 0; }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&a, sizeof a) != 0) {
        log_line("netpad: port %d busy (%d)", port, errno);
        close(fd);
        return 0;
    }
    if (listen(fd, 8) != 0) { log_line("netpad: listen failed (%d)", errno); close(fd); return 0; }

    g_listen_fd = fd;
    g_run = 1;
    if (pthread_create(&g_listener, NULL, listener_main, NULL) != 0) {
        log_line("netpad: no listener thread");
        close(fd);
        g_listen_fd = -1;
        g_run = 0;
        return 0;
    }
    log_line("netpad: controllers over the network on port %d", port);
    return 1;
}

void netpad_stop(void)
{
    int i;

    if (!g_run && g_listen_fd < 0) return;
    g_run = 0;
    if (g_listen_fd >= 0) { shutdown(g_listen_fd, SHUT_RDWR); close(g_listen_fd); g_listen_fd = -1; }
    for (i = 0; i < MAX_CLIENTS; i++) close_fd(i);
    pthread_join(g_listener, NULL);
    usleep(150000);                     /* let detached client threads unwind */
    log_line("netpad: stopped");
}

int netpad_clients(void)
{
    int i, n = 0;

    pthread_mutex_lock(&g_mtx);
    for (i = 0; i < MAX_CLIENTS; i++)
        if (g_cli[i].active && g_cli[i].slot >= 0) n++;
    pthread_mutex_unlock(&g_mtx);
    return n;
}
