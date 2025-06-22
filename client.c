#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if (argc<2) return 1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr))) return 1;
    FILE *f = fdopen(s, "r+");
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        printf("%s", buf);
        if (strchr(buf, '?')) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            fprintf(f, "%s", buf);
            fflush(f);
        }
    }
    fclose(f);
    return 0;
}
