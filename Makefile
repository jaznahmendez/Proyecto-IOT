CC ?= gcc

LIB = json-c
CURL = curl
WAYLAND = wayland-client


APP_NAME = test
OBJS = test.o

all: $(APP_NAME)

$(APP_NAME): $(OBJS)
	$(CC) -o $@ $^ -l$(LIB) -l$(CURL) -l$(WAYLAND)

%.o: %.c
	$(CC) -c $< -o $@ 

clean:
	rm -f *.o $(APP_NAME)
