
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <termios.h>

#include "termcols.h"
#include "list.h"

static size_t name_len;
static char name[256];

static size_t cur_msg_len;
static char cur_msg_buf[256];

static size_t read_full(int fd, void *buf, size_t len)
{
    char *msgptr = buf;
    size_t msglen = 0;

    while (msglen < len) {
        size_t ret;
        ret = read(fd, msgptr + msglen, len - msglen);
        if (ret == 0)
            return 0;

        msglen += ret;
    }

    return len;
}

static void display_msg_buf(void)
{
    printf("\r%s: %s", name, cur_msg_buf);
    fflush(stdout);
}

static void display_new_msg(char *msg, size_t msg_len, uint32_t msg_flags)
{
    size_t i;

    if (msg_flags & MSG_FLAG_URG)
        printf(TERM_COLOR_RED);

    printf("\r%s", msg);

    if (msg_flags & MSG_FLAG_URG)
        printf(TERM_COLOR_RESET);

    if (msg_len < cur_msg_len + 2 + name_len)
        for (i = 0; i < cur_msg_len + 2 + name_len - msg_len; i++)
            putchar(' ');

    putchar('\n');

    display_msg_buf();
}

static void handle_stdin(int sock)
{
    int ch;

    ch = getchar();

    if (ch == '\n') {
        if (cur_msg_len > 0) {
            struct msg_header header;
            memset(&header, 0, sizeof(header));

            if (cur_msg_len > 4
                && cur_msg_buf[0] == '/'
                && (cur_msg_buf[1] == 'm' || cur_msg_buf[1] == 'M')
                && (cur_msg_buf[2] == 'e' || cur_msg_buf[2] == 'E')) {

                header.msg_len = cur_msg_len - 4;
                header.flags = MSG_FLAG_ME;
                write(sock, &header, sizeof(header));
                write(sock, cur_msg_buf + 4, cur_msg_len - 4);
            } else if (cur_msg_len > 5
                && cur_msg_buf[0] == '/'
                && (cur_msg_buf[1] == 'u' || cur_msg_buf[1] == 'U')
                && (cur_msg_buf[2] == 'r' || cur_msg_buf[2] == 'R')
                && (cur_msg_buf[3] == 'g' || cur_msg_buf[3] == 'G')) {

                header.msg_len = cur_msg_len - 5;
                header.flags = MSG_FLAG_URG;
                write(sock, &header, sizeof(header));
                write(sock, cur_msg_buf + 5, cur_msg_len - 5);
            } else {
                header.msg_len = cur_msg_len;
                header.flags = 0;
                write(sock, &header, sizeof(header));
                write(sock, cur_msg_buf, cur_msg_len);
            }
            memset(cur_msg_buf, 0, sizeof(cur_msg_buf));

            int i;
            for (i = 0; i < cur_msg_len; i++)
                putchar('\b');

            for (i = 0; i < cur_msg_len; i++)
                putchar(' ');

            for (i = 0; i < cur_msg_len; i++)
                putchar('\b');

            cur_msg_len = 0;
        }
    } else if (ch == '\b' || ch == 127) {
        if (cur_msg_len > 0) {
            cur_msg_buf[--cur_msg_len] = '\0';
            printf("\b \b");
            fflush(stdout);
        }
    } else {
        if (cur_msg_len < sizeof(cur_msg_buf) - 1) {
            cur_msg_buf[cur_msg_len++] = (char)ch;
            putchar(ch);
            fflush(stdout);
        }
    }
}

static struct termios old_termios;

static void setup_terminal(void)
{
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &old_termios);

    new_termios = old_termios;

    new_termios.c_lflag &= ~(ECHO | ICANON | ECHOE);

    tcsetattr(STDIN_FILENO, 0, &new_termios);
}

static void teardown_terminal(void)
{
    tcsetattr(STDIN_FILENO, 0, &old_termios);
}

int main(int argc, char **argv)
{
    int sock;
    int ret;
    struct sockaddr_in addr;
    struct name_header name_header;

    printf("Name: ");
    fgets(name, sizeof(name), stdin);
    name_len = strlen(name);
    name_len--;
    name[name_len] = '\0';

    setup_terminal();
    atexit(teardown_terminal);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);

    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {
        perror("connect");
        return 1;
    }

    memset(&name_header, 0, sizeof(name_header));
    name_header.name_len = name_len;

    write(sock, &name_header, sizeof(name_header));
    write(sock, name, name_len);

    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sock;
    fds[1].events = POLLIN;

    display_msg_buf();

    while (1) {
        poll(fds, sizeof(fds) / sizeof(*fds), -1);

        if (fds[0].revents & POLLIN)
            handle_stdin(sock);

        if (fds[1].revents & POLLIN) {
            size_t r;
            struct msg_header header;

            memset(&header, 0, sizeof(header));

            r = read_full(sock, &header, sizeof(header));
            if (r == 0) {
                printf("Disconnected from server\n");
                return 1;
            }

            char msg[header.msg_len + 1];

            r = read_full(sock, msg, header.msg_len);
            if (r == 0) {
                printf("Disconnected from server\n");
                return 1;
            }

            msg[header.msg_len] = '\0';

            display_new_msg(msg, header.msg_len, header.flags);
        }
    }

    return 0;
}

