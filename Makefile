CC ?= gcc
CFLAGS = $(shell pkg-config --cflags json-c)
LDFLAGS = $(shell pkg-config --libs json-c) -lcurl

APP_NAME = test-ac
OBJS = test-ac.o

all: $(APP_NAME)

$(APP_NAME): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(APP_NAME)
