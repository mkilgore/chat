
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

#include "list.h"

static int server_fd;

static size_t client_count;
static list_head_t client_list = LIST_HEAD_INIT(client_list);

struct client {
    list_node_t entry;
    union {
        struct sockaddr_storage addr;
        struct sockaddr_in      addr_in;
    };
    size_t name_len;
    char *name;
    char *addr_name;
    int fd;
};

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


static void client_init(struct client *client)
{
    memset(client, 0, sizeof(*client));
    list_node_init(&client->entry);
}

static void client_clear(struct client *client)
{
    free(client->name);
    free(client->addr_name);
}

static void client_set_addr_name(struct client *client)
{
    switch (client->addr.ss_family) {
    case AF_INET:
        client->addr_name = strdup(inet_ntoa(client->addr_in.sin_addr));
        break;

    default:
        client->addr_name = strdup("Unknown");
        break;
    }
}

static void client_add(struct client *client)
{
    client_count++;
    list_add(&client_list, &client->entry);
}

static void client_del(struct client *client)
{
    client_count--;
    list_del(&client->entry);
}

static void msg_broadcast(struct client *client, char *msg, size_t msg_len, uint32_t msg_flags)
{
    size_t outmsg_len = msg_len + client->name_len + 20;
    char outmsg[outmsg_len];
    size_t len;
    struct client *snd_client;
    struct msg_header header;

    if (msg_flags & MSG_FLAG_ME) {
        len = snprintf(outmsg, sizeof(outmsg), "* %s ", client->name);
    } else {
        len = snprintf(outmsg, sizeof(outmsg), "%s: ", client->name);
    }

    memcpy(outmsg + len, msg, msg_len);

    header.msg_len = len + msg_len;
    header.flags = msg_flags;

    list_foreach_entry(&client_list, snd_client, entry) {
        write(snd_client->fd, &header, sizeof(header));
        write(snd_client->fd, outmsg, header.msg_len);
    }
}

static void handle_new_client(int server_fd)
{
    struct client *client;
    size_t ret;
    struct name_header header;

    socklen_t addr_len = sizeof(client->addr);

    client = malloc(sizeof(*client));
    client_init(client);

    client->fd = accept(server_fd, (struct sockaddr *)&client->addr, &addr_len);

    client_set_addr_name(client);

    ret = read_full(client->fd, &header, sizeof(header));
    if (!ret)
        goto client_free;

    client->name_len = header.name_len;

    client->name = malloc(client->name_len + 1);
    ret = read_full(client->fd, client->name, client->name_len);
    if (!ret)
        goto client_free;

    client->name[client->name_len] = '\0';
    printf("Client %s: %s\n", client->addr_name, client->name);

    client_add(client);

    return ;

  client_free:
    close(client->fd);
    client_clear(client);
    free(client);
    return ;
}

static void handle_client_pollin(struct client *client)
{
    size_t ret;
    struct msg_header header;

    ret = read_full(client->fd, &header, sizeof(header));
    if (ret) {
        char msg[header.msg_len + 1];

        ret = read_full(client->fd, msg, header.msg_len);
        msg[header.msg_len] = '\0';
        if (!ret)
            goto client_close;

        printf("Msg %s: %s\n", client->name, msg);

        msg_broadcast(client, msg, header.msg_len, header.flags);

        return ;
    }

  client_close:
    printf("Client %s close\n", client->addr_name);
    client_del(client);
    close(client->fd);
    client_clear(client);
    free(client);
    return ;
}

int main(int argc, char **argv)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);

    while (1) {
        struct client *client, *next;
        struct pollfd fds[client_count + 1];
        size_t i = 1;

        fds[0].fd = server_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        printf("Client count: %zd\n", client_count);

        list_foreach_entry(&client_list, client, entry) {
            fds[i].fd = client->fd;
            fds[i].events = POLLIN;
            fds[i].revents = 0;
            i++;
        }

        poll(fds, client_count + 1, -1);

        i = 1;
        for (client = list_first_entry(&client_list, struct client, entry);
             !list_ptr_is_head(&client_list, &client->entry);
             client = next) {
            next = list_next_entry(client, entry);

            if (fds[i].revents & POLLIN)
                handle_client_pollin(client);
            i++;
        }

        if (fds[0].revents & POLLIN)
            handle_new_client(server_fd);
    }

    return 0;
}

