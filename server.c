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

const char *ranks[] = { "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K" };
const char *suits[] = { "♥", "♦", "♣", "♠" };
int deck[52];
int next_card;

typedef struct {
    char user[32];
    char pass[32];
    int tokens;
    int score;
} user_t;

void init_deck() {
    for (int i = 0; i < 52; i++) {
        deck[i] = i;
    }
    for (int i = 51; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }
    next_card = 0;
}

int draw_card() {
    if (next_card >= 52) return -1;
    return deck[next_card++];
}

int card_value(int c) {
    int r = c % 13 + 1;
    if (r == 1) return 11;
    if (r > 10) return 10;
    return r;
}

void card_str(int c, char *buf) {
    sprintf(buf, "%s%s", ranks[c % 13], suits[c / 13]);
}

int load_user(const char *u, const char *p, user_t *out) {
    FILE *f = fopen("users.txt", "r+");
    if (!f) return 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        user_t tmp;
        if (sscanf(line, "%31s %31s %d %d", tmp.user, tmp.pass, &tmp.tokens, &tmp.score) == 4) {
            if (!strcmp(tmp.user, u) && !strcmp(tmp.pass, p)) {
                *out = tmp;
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

int save_user(const user_t *u) {
    FILE *in = fopen("users.txt", "r");
    FILE *out = fopen("users.tmp", "w");
    int found = 0;

    if (in) {
        user_t tmp;
        while (fscanf(in, "%31s %31s %d %d", tmp.user, tmp.pass, &tmp.tokens, &tmp.score) == 4) {
            if (!strcmp(tmp.user, u->user)) {
                fprintf(out, "%s %s %d %d\n", u->user, u->pass, u->tokens, u->score);
                found = 1;
            } else {
                fprintf(out, "%s %s %d %d\n", tmp.user, tmp.pass, tmp.tokens, tmp.score);
            }
        }
        fclose(in);
    }

    if (!found) {
        fprintf(out, "%s %s %d %d\n", u->user, u->pass, u->tokens, u->score);
    }

    fclose(out);
    rename("users.tmp", "users.txt");
    return 1;
}

void print_hand(FILE *f, int *hand, int count, int hide_last) {
    char buf[16];
    for (int i = 0; i < count; i++) {
        if (i == count - 1 && hide_last) {
            fprintf(f, "hidden");
        } else {
            card_str(hand[i], buf);
            fprintf(f, "%s", buf);
        }
        if (i < count - 1) fprintf(f, ", ");
    }
}

void handle_client(int sock) {
    FILE *f = fdopen(sock, "r+");
    char buf[64];
    user_t u;

    // login or register
    fprintf(f, "login or register (l/r)?\n"); fflush(f);
    if (!fgets(buf, sizeof(buf), f)) return;

    if (buf[0] == 'r') {
        // registration
        fprintf(f, "user:\n"); fflush(f);
        fgets(buf, sizeof(buf), f);
        buf[strcspn(buf, "\n")] = '\0';
        strcpy(u.user, buf);

        fprintf(f, "pass:\n"); fflush(f);
        fgets(buf, sizeof(buf), f);
        buf[strcspn(buf, "\n")] = '\0';
        strcpy(u.pass, buf);

        u.tokens = 1000;
        u.score = 0;
        save_user(&u);
    } else {
        // login
        int ok = 0;
        while (!ok) {
            fprintf(f, "user:\n"); fflush(f);
            fgets(buf, sizeof(buf), f);
            buf[strcspn(buf, "\n")] = '\0';
            strcpy(u.user, buf);

            fprintf(f, "pass:\n"); fflush(f);
            fgets(buf, sizeof(buf), f);
            buf[strcspn(buf, "\n")] = '\0';
            strcpy(u.pass, buf);

            ok = load_user(u.user, u.pass, &u);
            if (!ok) {
                fprintf(f, "invalid\n"); fflush(f);
            }
        }
    }

    fprintf(f, "welcome %s tokens:%d score:%d\n", u.user, u.tokens, u.score);
    fflush(f);

    int play = 1;
    while (play && u.tokens > 0) {
        fprintf(f, "tokens:%d bet?\n", u.tokens); fflush(f);
        if (!fgets(buf, sizeof(buf), f)) break;

        int bet = atoi(buf);
        if (bet < 1 || bet > u.tokens) {
            fprintf(f, "bad bet\n");
            continue;
        }
        u.tokens -= bet;        // deal cards
        srand(time(NULL) + getpid());
        init_deck();
        int uh[12], dh[12];
        int uc = 0, dc = 0;
        uh[uc++] = draw_card();
        dh[dc++] = draw_card();
        dh[dc++] = draw_card();

        int user_sum = card_value(uh[0]);
        int dealer_sum = card_value(dh[0]);
        int hidden_card = dh[1];        fprintf(f, "your: "); print_hand(f, uh, uc, 0);
        fprintf(f, " (total: %d)\n", user_sum);
        fprintf(f, "dealer: "); print_hand(f, dh, 1, 0);
        fprintf(f, ", hidden\n");
        fflush(f);

        // player turn
        while (1) {
            fprintf(f, "h/s?\n"); fflush(f);
            if (!fgets(buf, sizeof(buf), f)) return;
            if (buf[0] == 'h') {
                int c = draw_card();
                uh[uc++] = c;
                user_sum += card_value(c);
                fprintf(f, "your: "); print_hand(f, uh, uc, 0);
                fprintf(f, " (total: %d)\n", user_sum);
                fflush(f);
                if (user_sum >= 21) break;
            } else {
                break;
            }
        }

        // dealer turn
        int dealer_total = card_value(hidden_card) + dealer_sum;
        while (dealer_total < 17) {
            int c = draw_card();
            dh[dc++] = c;
            dealer_total += card_value(c);
        }

        fprintf(f, "dealer: "); print_hand(f, dh, dc, 0);
        fprintf(f, " (total: %d)\n", dealer_total);

        // outcome
        if (user_sum > 21 || (dealer_total <= 21 && dealer_total > user_sum)) {
            fprintf(f, "lose\n");
        } else if (user_sum == dealer_total) {
            fprintf(f, "push\n");
            u.tokens += bet;
        } else {
            fprintf(f, "win\n");
            u.tokens += bet * 2;
            u.score += 2;
        }
        fflush(f);
        save_user(&u);

        fprintf(f, "play again? y/n\n"); fflush(f);
        if (!fgets(buf, sizeof(buf), f)) break;
        if (buf[0] != 'y') play = 0;
    }

    fprintf(f, "bye tokens:%d score:%d\n", u.tokens, u.score);
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

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
    if (listen(s, 5) < 0) return 1;

    while (1) {
        int c = accept(s, NULL, NULL);
        if (c < 0) continue;
        if (!fork()) {
            close(s);
            handle_client(c);
            exit(0);
        }
        close(c);
    }

    return 0;
}
