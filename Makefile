CC = gcc
CFLAGS = -Wall -Wextra -I include -I rpc $(shell pkg-config --cflags libtirpc)
CFLAGS_RPC = -Wall -I include -I rpc $(shell pkg-config --cflags libtirpc)
LDFLAGS = -lpthread $(shell pkg-config --libs libtirpc)

BUILD_DIR = build

all: $(BUILD_DIR)/server $(BUILD_DIR)/servidor_rpc

$(BUILD_DIR)/server: server/server.c server/sock_utils.c rpc/plantilla_clnt.c rpc/plantilla_xdr.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) server/server.c server/sock_utils.c rpc/plantilla_clnt.c rpc/plantilla_xdr.c $(LDFLAGS) -o $(BUILD_DIR)/server

$(BUILD_DIR)/servidor_rpc: rpc/log_server.c rpc/plantilla_svc.c rpc/plantilla_xdr.c | $(BUILD_DIR)
	$(CC) $(CFLAGS_RPC) rpc/log_server.c rpc/plantilla_svc.c rpc/plantilla_xdr.c $(LDFLAGS) -o $(BUILD_DIR)/servidor_rpc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean