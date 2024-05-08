CC ?= gcc

LIB = json-c
CURL = curl
WAYLAND = wayland-client
JPG = jpeg

APP_NAME = image-test
OBJS = image-test.o

all: $(APP_NAME)

$(APP_NAME): $(OBJS)
	$(CC) -o $@ $^ -l$(LIB) -l$(CURL) -l$(WAYLAND) -l$(JPG)

%.o: %.c
	$(CC) -c $< -o $@ 

clean:
	rm -f *.o $(APP_NAME)
