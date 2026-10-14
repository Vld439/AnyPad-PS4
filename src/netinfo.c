#include "netinfo.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <unistd.h>

int local_ip(char out[16])
{
    struct sockaddr_in sin, me;
    socklen_t len = sizeof me;
    int fd = socket(AF_INET, SOCK_DGRAM, 0), ok = 0;

    strcpy(out, "127.0.0.1");
    if (fd < 0) return 0;
    memset(&sin, 0, sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(9);
    sin.sin_addr.s_addr = inet_addr("192.0.2.1");       /* TEST-NET-1 */
    if (connect(fd, (struct sockaddr *)&sin, sizeof sin) == 0 &&
        getsockname(fd, (struct sockaddr *)&me, &len) == 0 && me.sin_addr.s_addr != 0) {
        inet_ntop(AF_INET, &me.sin_addr, out, 16);
        ok = 1;
    }
    close(fd);
    return ok;
}
