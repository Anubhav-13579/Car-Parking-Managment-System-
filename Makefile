CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./include -O2
LDFLAGS = -pthread -lrt

SRC = src/entry_manager.c src/bankers_arbiter.c src/vehicle_generator.c src/exit_fee.c src/shm_ipc.c
OBJ = $(SRC:.c=.o)

all: bin/smart_parking bin/test_concurrency

bin/smart_parking: $(OBJ) src/main.o
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: bin/smart_parking"

bin/test_concurrency: $(OBJ) tests/test_concurrency.o
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: bin/test_concurrency"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: bin/smart_parking
	./bin/smart_parking 50

test: bin/test_concurrency
	./bin/test_concurrency

clean:
	rm -rf $(OBJ) src/main.o tests/test_concurrency.o bin/
