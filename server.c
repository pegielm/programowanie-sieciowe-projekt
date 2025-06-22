#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>

const char *ranks[] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};
const char *suits[] = {"♥","♦","♣","♠"};
int deck[52];
int next_card;

void init_deck() {
    for (int i = 0; i < 52; i++) deck[i] = i;
    for (int i = 51; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
    }
    next_card = 0;
}

int draw_card() {
    if (next_card >= 52) return -1;
    return deck[next_card++];
}

int card_value(int card) {
    int rank = card % 13 + 1;
    if (rank == 1) return 11;
    if (rank > 10) return 10;
    return rank;
}

void card_str(int card, char *buf) {
    int r = card % 13;
    int s = card / 13;
    sprintf(buf, "%s%s", ranks[r], suits[s]);
}

void print_hand(FILE *f, int *hand, int count, int hide_last) {
    char buf[16];
    for (int i = 0; i < count; i++) {
        if (i == count-1 && hide_last) {
            fprintf(f, "hidden");
        } else {
            card_str(hand[i], buf);
            fprintf(f, "%s", buf);
        }
        if (i < count-1) fprintf(f, ", ");
    }
}

void handle_client(int sock) {
    FILE *f = fdopen(sock, "r+");
    char buf[32];
    int play = 1;
    while (play) {
        init_deck();
        int user_hand[12], dealer_hand[12];
        int ucount=0, dcount=0;
        user_hand[ucount++] = draw_card();
        dealer_hand[dcount++] = draw_card();
        dealer_hand[dcount++] = draw_card();

        int user_sum = card_value(user_hand[0]);
        int dealer_sum = card_value(dealer_hand[0]);
        int dealer_hidden = dealer_hand[1];

        fprintf(f, "your: "); print_hand(f, user_hand, ucount, 0);
        fprintf(f, " (total: %d)\n", user_sum);
        fprintf(f, "dealer: "); print_hand(f, dealer_hand, 1, 0);
        fprintf(f, ", hidden\n");
        fflush(f);

        while (1) {
            fprintf(f, "hit or stand (h/s)?\n"); fflush(f);
            if (!fgets(buf, sizeof(buf), f)) return;
            if (buf[0]=='h') {
                int c = draw_card();
                user_hand[ucount++] = c;
                user_sum += card_value(c);
                fprintf(f, "your: "); print_hand(f, user_hand, ucount, 0);
                fprintf(f, " (total: %d)\n", user_sum);
                fflush(f);
                if (user_sum>=21) break;
            } else if (buf[0]=='s') break;
        }

        int dealer_total = card_value(dealer_hidden) + dealer_sum;
        while (dealer_total < 17) {
            int c = draw_card();
            dealer_hand[dcount++] = c;
            dealer_total += card_value(c);
        }

        fprintf(f, "reveal dealer: "); print_hand(f, dealer_hand, dcount, 0);
        fprintf(f, " (total: %d)\n", dealer_total);

        if (user_sum>21 || (dealer_total<=21 && dealer_total>user_sum))
            fprintf(f, "you lose\n");
        else if (user_sum==dealer_total)
            fprintf(f, "push\n");
        else
            fprintf(f, "you win\n");
        fflush(f);

        fprintf(f, "play again (y/n)?\n"); fflush(f);
        if (!fgets(buf, sizeof(buf), f)) return;
        if (buf[0]!='y') play=0;
    }
    fclose(f);
}

int main() {
    signal(SIGCHLD, SIG_IGN);
    srand(time(NULL));
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(12345);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr))<0) return 1;
    if (listen(s, 5)<0) return 1;
    while (1) {
        int c = accept(s, NULL, NULL);
        if (c<0) continue;
        if (!fork()) {
            close(s);
            handle_client(c);
            exit(0);
        }
        close(c);
    }
    return 0;
}