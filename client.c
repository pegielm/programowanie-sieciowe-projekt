#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/sha.h>

void hash_password(const char *input, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    }
    output_hex[64] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(s);
        return 1;
    }

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(s);
        return 1;
    }

    FILE *f = fdopen(s, "r+");
    if (!f) {
        perror("fdopen");
        close(s);
        return 1;
    }

    char buf[256];

    while (fgets(buf, sizeof(buf), f)) {
        printf("%s", buf);

        // Check if input is required from user
        if (strchr(buf, '?') || 
            (strchr(buf, ':') && !strstr(buf, "welcome") && !strstr(buf, "total") &&
             !strstr(buf, "your:") && !strstr(buf, "dealer:"))) {

            if (strstr(buf, "pass")) {
                // Read password and hash it before sending
                char input[256];
                if (!fgets(input, sizeof(input), stdin)) break;
                input[strcspn(input, "\n")] = '\0';

                char hash[65];
                hash_password(input, hash);
                fprintf(f, "%s\n", hash);
            } else {
                // Normal input (user, bet, h/s, y/n)
                if (!fgets(buf, sizeof(buf), stdin)) break;
                fprintf(f, "%s", buf);
            }

            fflush(f);
        }
    }

    fclose(f);
    return 0;
}