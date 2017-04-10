
CFLAGS := -Wall -I./include -g

CLIENT := ./bin/client
SERVER := ./bin/server

PROGS := $(CLIENT) $(SERVER)

COMMON_SRCS := $(wildcard ./common/*.c)
COMMON_OBJS := $(COMMON_SRCS:.c=.o)

SERVER_SRCS := $(wildcard ./server/*.c)
SERVER_OBJS := $(SERVER_SRCS:.c=.o)

CLIENT_SRCS := $(wildcard ./client/*.c)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

CLEAN_LIST += $(COMMON_OBJS)
CLEAN_LIST += $(SERVER_OBJS)
CLEAN_LIST += $(CLIENT_OBJS)
CLEAN_LIST += $(PROGS)

all: $(PROGS)

./bin:
	mkdir -p $@

$(CLIENT): $(CLIENT_OBJS) $(COMMON_OBJS) | ./bin
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS) $(COMMON_OBJS)

$(SERVER): $(SERVER_OBJS) $(COMMON_OBJS) | ./bin
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(COMMON_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@for file in $(CLEAN_LIST); do \
		if [ -e $$file ]; then \
			echo "rm -rf $$file"; \
			rm -rf $$file; \
		fi \
	done

.PHONY: all

