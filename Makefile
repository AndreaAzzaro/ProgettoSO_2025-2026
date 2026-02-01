CC = gcc
CFLAGS = -Wall -Wextra -Wvla -Werror -pthread -g -D_GNU_SOURCE
INCLUDES = -Iinclude

# Directory
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# File sorgente comuni (Utilities e IPC)
COMMON_SRC = $(wildcard $(SRC_DIR)/ipc/*.c) \
             $(wildcard $(SRC_DIR)/utils/*.c) \
             $(wildcard $(SRC_DIR)/common/*.c) \
             $(wildcard $(SRC_DIR)/config/*.c) \
             $(wildcard $(SRC_DIR)/statistics/*.c)

COMMON_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(COMMON_SRC))

# Target specifici
TARGETS = responsabile_mensa operatore utente operatore_cassa add_users communication_disorder

.PHONY: all clean dirs

all: dirs $(addprefix $(BIN_DIR)/, $(TARGETS))
	@rm -f statistics_report.csv
	@echo "Sistema compilato. Statistiche resettate."

# Creazione directory necessarie
dirs:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)/ipc $(OBJ_DIR)/utils $(OBJ_DIR)/common $(OBJ_DIR)/config $(OBJ_DIR)/statistics
	@mkdir -p $(OBJ_DIR)/programs/responsabile_mensa
	@mkdir -p $(OBJ_DIR)/programs/operatore
	@mkdir -p $(OBJ_DIR)/programs/utente
	@mkdir -p $(OBJ_DIR)/programs/operatore_cassa
	@mkdir -p $(OBJ_DIR)/programs/add_users
	@mkdir -p $(OBJ_DIR)/programs/communication_disorder

# Regola per gli oggetti
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Responsabile Mensa
RESP_SRC = $(SRC_DIR)/programs/responsabile_mensa/responsabile_mensa.c \
           $(SRC_DIR)/programs/responsabile_mensa/simulation_engine.c \
           $(SRC_DIR)/programs/responsabile_mensa/setup_population.c \
           $(SRC_DIR)/programs/responsabile_mensa/setup_ipc.c
RESP_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(RESP_SRC))

$(BIN_DIR)/responsabile_mensa: $(RESP_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(RESP_OBJ) $(COMMON_OBJ) -o $@ -lrt

# Operatore
OPER_SRC = $(SRC_DIR)/programs/operatore/operatore.c
OPER_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(OPER_SRC))

$(BIN_DIR)/operatore: $(OPER_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(OPER_OBJ) $(COMMON_OBJ) -o $@ -lrt

# Utente
UTENT_SRC = $(SRC_DIR)/programs/utente/utente.c
UTENT_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(UTENT_SRC))

$(BIN_DIR)/utente: $(UTENT_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(UTENT_OBJ) $(COMMON_OBJ) -o $@ -lrt

# Operatore Cassa (Aggiunto per completezza)
CASSA_SRC = $(SRC_DIR)/programs/operatore_cassa/operatore_cassa.c
CASSA_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CASSA_SRC))

$(BIN_DIR)/operatore_cassa: $(CASSA_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(CASSA_OBJ) $(COMMON_OBJ) -o $@ -lrt

# Add Users Utility
ADD_USERS_SRC = $(SRC_DIR)/programs/add_users/add_users.c
ADD_USERS_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(ADD_USERS_SRC))

$(BIN_DIR)/add_users: $(ADD_USERS_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(ADD_USERS_OBJ) $(COMMON_OBJ) -o $@ -lrt

# Communication Disorder Utility
DISORDER_SRC = $(SRC_DIR)/programs/communication_disorder/communication_disorder.c
DISORDER_OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(DISORDER_SRC))

$(BIN_DIR)/communication_disorder: $(DISORDER_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) $(DISORDER_OBJ) $(COMMON_OBJ) -o $@ -lrt

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
