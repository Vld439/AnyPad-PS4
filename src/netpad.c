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
"padding:20px;max-width:440px;width:100%;text-align:center}#st{color:#8a97b2}.on #st{color:#3c3}"
".pad{display:flex;align-items:center;justify-content:center;gap:8px;font-weight:600;margin:10px 0;word-break:break-word}"
".dot{flex:none;width:10px;height:10px;border-radius:50%;background:#c33}.dot.on{background:#3c3}"
"small{color:#8a97b2;display:block;line-height:1.5;margin-top:12px}"
"#lost{margin-top:14px;padding:12px;border:1px solid #c33;border-radius:10px;background:#2a1418;text-align:left}"
"#lost p{margin:0 0 10px;line-height:1.4}#lost input{width:60%;padding:8px;border-radius:8px;border:1px solid #243049;"
"background:#0b0f1a;color:#e7ecf5;font:inherit}#lost button{padding:8px 16px;margin-left:6px;border-radius:8px;border:0;"
"background:#9db4ff;color:#0b0f1a;font:inherit;font-weight:700}"
"b{color:#9db4ff}</style></head><body>"
"<h1>AnyPad &mdash; mando por red</h1>"
"<div class=card id=card><div id=st>Esperando mandos&hellip;</div><div id=pads></div>"
"<div id=lost hidden><p>No llega a la consola. Si su IP cambi&oacute;, escribe la nueva "
"(la de la notificaci&oacute;n de AnyPad):</p><input id=ip inputmode=decimal autocomplete=off "
"placeholder='IP de la consola'><button id=go>Ir</button></div>"
"<small>El navegador lee los mandos de este dispositivo (tambi&eacute;n los emparejados por <b>Bluetooth</b>) "
"y env&iacute;a cada uno a la consola como un mando virtual distinto. <b>Toca la pantalla una vez</b> para que no se apague "
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
/* One WebSocket per gamepad: each one is its own client on the console, and
 * so its own virtual pad. Opened when the browser shows the pad, closed (the
 * virtual pad goes) when it disconnects; reopened while the pad is there. */
"var card=document.getElementById('card'),st=document.getElementById('st'),list=document.getElementById('pads');"
"var P={};"
/* Not one pad has reached the console for 15 s: its address may have
 * changed, and only the person knows the new one (the notification shows
 * it). The page cannot find it by itself. */
"var lastOk=Date.now(),lost=document.getElementById('lost'),ipIn=document.getElementById('ip');"
"function go(){var v=ipIn.value.trim();if(!/^\\d{1,3}(\\.\\d{1,3}){3}$/.test(v))return;"
"location.href='http://'+v+':'+(location.port||'8096')+'/';}"
"document.getElementById('go').onclick=go;ipIn.onkeydown=function(e){if(e.key==='Enter')go();};"
"function watch(){var k,n=0,on=0;for(k in P){n++;if(P[k].open)on++;}if(on||!n)lastOk=Date.now();"
"var show=n>0&&!on&&Date.now()-lastOk>15000;if(show&&lost.hidden&&!ipIn.value)ipIn.value=location.hostname;lost.hidden=!show;}"
"function draw(){var k,n=0,on=0;list.textContent='';"
"for(k in P){var p=P[k],r=document.createElement('div'),d=document.createElement('span');n++;if(p.open)on++;"
"r.className='pad';d.className='dot'+(p.open?' on':'');r.appendChild(d);r.appendChild(document.createTextNode(p.name));list.appendChild(r);}"
"if(!n){var h=document.createElement('div');h.textContent='Conecta uno o varios mandos a este dispositivo y pulsa un bot\\u00f3n en cada uno';list.appendChild(h);}"
"st.textContent=n?on+' de '+n+' enviando a la consola':'Esperando mandos\\u2026';card.className=on?'on':'';}"
"function short(id){var s=id.replace(/\\s*\\(.*$/,'').replace(/^[0-9a-f]{1,4}-[0-9a-f]{1,4}-/i,'').trim();return (s||'Mando').slice(0,31);}"
/* A connection not open in 4 s is given up and tried again: one tried while
 * the console had no network hangs for about a minute, and browsers open one
 * connection to an address at a time, so it held back the other pads. */
"function link(i){var p=P[i];if(!p||p.ws)return;var ws=new WebSocket('ws://'+location.host+'/ws?n='+encodeURIComponent(p.name));ws.binaryType='arraybuffer';p.ws=ws;"
"var t=setTimeout(function(){if(p.open||p.ws!==ws)return;ws.onclose=ws.onerror=null;try{ws.close()}catch(e){}"
"p.ws=null;if(P[i]===p)setTimeout(function(){link(i);},500);},4000);"
"ws.onopen=function(){clearTimeout(t);p.open=true;draw();};"
"ws.onclose=function(){clearTimeout(t);p.open=false;p.ws=null;draw();if(P[i]===p)setTimeout(function(){link(i);},2000);};"
"ws.onerror=function(){try{ws.close()}catch(e){}};}"
"function drop(i){var p=P[i];if(!p)return;delete P[i];if(p.ws){try{p.ws.close()}catch(e){}}draw();}"
"function axis(v){return Math.max(0,Math.min(255,Math.round((v+1)*127.5)));}"
"function frame(g){var btn=0,l2=0,r2=0,b=g.buttons,a=g.axes,i;"
"for(i=0;i<map.length&&i<b.length;i++){if(b[i]&&b[i].pressed)btn|=map[i];}"
"if(b[6]){l2=Math.round((b[6].value||0)*255);if(b[6].pressed||l2>30)btn|=B.L2;}"
"if(b[7]){r2=Math.round((b[7].value||0)*255);if(b[7].pressed||r2>30)btn|=B.R2;}"
"var buf=new ArrayBuffer(10),d=new DataView(buf);"
"d.setUint32(0,btn>>>0,true);"
"d.setUint8(4,axis(a[0]||0));d.setUint8(5,axis(a[1]||0));"
"d.setUint8(6,axis(a[2]||0));d.setUint8(7,axis(a[3]||0));"
"d.setUint8(8,l2);d.setUint8(9,r2);return buf;}"
"function loop(){requestAnimationFrame(loop);"
"var gp=navigator.getGamepads?navigator.getGamepads():[],seen={},i,k;"
"for(i=0;i<gp.length;i++){var g=gp[i];if(!g||g.connected===false)continue;seen[g.index]=1;"
"if(!P[g.index]){P[g.index]={name:short(g.id),ws:null,open:false};link(g.index);draw();}"
"var p=P[g.index];if(p.open){try{p.ws.send(frame(g));}catch(e){}}}"
"for(k in P)if(!seen[k])drop(k);watch();}"
"draw();requestAnimationFrame(loop);"
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
static int g_listen_fd = -1, g_port;
static volatile int g_run;

static void close_fd(int i)
{
    pthread_mutex_lock(&g_mtx);
    if (g_cli[i].fd >= 0) { shutdown(g_cli[i].fd, SHUT_RDWR); close(g_cli[i].fd); g_cli[i].fd = -1; }
    pthread_mutex_unlock(&g_mtx);
}

/* Takes the lowest free virtual-pad slot (USB takes them from the top, so the
 * two do not collide until all four are in use). -1 if none is free. */
static int claim_slot(const char *name)
{
    char label[40];
    int s, got = -1;

    /* "network", or "network:<name>": the menu shows the name, and that it
     * came over the network. */
    snprintf(label, sizeof label, name[0] ? "network:%s" : "network", name);
    pthread_mutex_lock(&g_slot_mtx);
    for (s = 0; s < MAX_PADS; s++)
        if (vpad_add(s)) { vpad_label(s, label); got = s; break; }
    pthread_mutex_unlock(&g_slot_mtx);
    return got;
}

static int hexval(int c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
           c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* The controller's name the page sends with the connection, /ws?n=<name>
 * percent-encoded, into out (at most max - 1 bytes; "" if none). Control
 * characters, quotes and backslashes are dropped, and a UTF-8 character cut
 * by the length limit is left out whole. */
static void name_from_request(const char *req, char *out, size_t max)
{
    const char *p = req, *eol = strstr(req, "\r\n");
    size_t n = 0, i;

    out[0] = '\0';
    if (!eol) return;
    while (p < eol && *p != ' ') p++;                   /* the path, on the request line */
    for (p++; p < eol && *p != ' '; p++)
        if ((*p == '?' || *p == '&') && strncmp(p + 1, "n=", 2) == 0) break;
    if (p >= eol || *p == ' ') return;
    for (p += 3; p < eol && *p != ' ' && *p != '&' && n < max - 1; p++) {
        int c = (unsigned char)*p;
        if (c == '%' && hexval(p[1]) >= 0 && hexval(p[2]) >= 0) {
            c = hexval(p[1]) * 16 + hexval(p[2]);
            p += 2;
        } else if (c == '+') {
            c = ' ';
        }
        if (c < 0x20 || c == 0x7F || c == '"' || c == '\\') continue;
        out[n++] = (char)c;
    }
    i = n;                                              /* a cut UTF-8 sequence at the end */
    while (i > 0 && ((unsigned char)out[i - 1] & 0xC0) == 0x80) i--;
    if (i > 0 && (unsigned char)out[i - 1] >= 0xC0) {
        unsigned lead = (unsigned char)out[i - 1];
        size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
        if (n - (i - 1) < need) n = i - 1;
    }
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = '\0';
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

static void run_pad(int fd, int i, const char *name)
{
    int slot = claim_slot(name);
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
    log_line("netpad: client on slot %d%s%s", slot, name[0] ? ": " : "", name);

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
    char req[4096], key[128], accept_b64[40], name[32];

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
        name_from_request(req, name, sizeof name);
        if (send_all(fd, resp, (size_t)n) == 0)
            run_pad(fd, i, name);
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

static int netpad_busy(void);

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
    g_port = port;
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
    /* The client threads are detached: wait until each has let go of its
     * entry (and removed its pad), or one still unwinding could close the
     * socket of a client of the next start that took the same entry. */
    for (i = 0; i < 40 && netpad_busy(); i++) usleep(50000);
    log_line("netpad: stopped");
}

/* Is it still serving? The listener must still listen and a connection to
 * this machine's own address must be taken: the console can leave it dead
 * when its network is torn down or reconfigured, as web_alive says. */
int netpad_alive(void)
{
    struct sockaddr_in sin;
    int v = 0, fd, ok;
    socklen_t l = sizeof v;

    if (!g_run || g_listen_fd < 0) return 0;
    if (getsockopt(g_listen_fd, SOL_SOCKET, SO_ACCEPTCONN, &v, &l) == 0 && !v) return 0;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 1;                       /* cannot ask: do not call it dead */
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons((uint16_t)g_port);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ok = connect(fd, (struct sockaddr *)&sin, sizeof sin) == 0;
    close(fd);                                  /* the server sees a request that never comes */
    return ok;
}

static int netpad_busy(void)
{
    int i, n = 0;

    pthread_mutex_lock(&g_mtx);
    for (i = 0; i < MAX_CLIENTS; i++) n += g_cli[i].active;
    pthread_mutex_unlock(&g_mtx);
    return n;
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
