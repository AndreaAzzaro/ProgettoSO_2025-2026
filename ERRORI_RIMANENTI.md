# Errori da Correggere - ProgettoSO 2025-2026

## ✅ Errori Già Corretti (9/17)

### CRITICAL (2/2)
1. ✅ **Barrier deadlock with dynamic users** - Implementato protocollo di sincronizzazione con `pending_add_users_count`
2. ✅ **Operator blocking msgrcv not waking on day-end** - Aggiunto controllo `local_daily_cycle_is_active` dopo EINTR

### HIGH (5/5)
3. ✅ **SIGCHLD handler modifying shared memory without mutex** - Aggiunte funzioni `block_sigchld()`/`unblock_sigchld()`
4. ✅ **Refill cycle writing portions without mutex** - Aggiunto `MUTEX_SHARED_DATA` in `handle_refill_cycle()` e `perform_initial_daily_refill()`
5. ✅ **Timer leak** - Salvati `timer_id` globali e chiamato `timer_delete()` prima di ricreare
6. ✅ **Barrier compensation double-decrement in SIGCHLD** - Cambiato da NO_UNDO a SEM_UNDO, rimossa compensazione manuale
7. ✅ **Race condition on sem-zero semaphore** - Rimossa compensazione manuale, SEM_UNDO gestisce automaticamente

### MEDIUM (2/5)
8. ✅ **Make is_simulation_running volatile** - Cambiato `int` in `volatile int` in common.h
9. ✅ **Fix hardcoded coffee/dessert to 4 items** - Sostituito `4` con `MAX_DISHES_PER_CATEGORY`

---

## ⚠️ Errori Rimanenti da Correggere (8/17)

### MEDIUM (3 rimanenti)

#### 10. SEM_UNDO on station posts
**Problema**: Se un operatore fa `reserve_sem` (con SEM_UNDO) su `STATION_SEM_AVAILABLE_POSTS`, poi fa `release_sem` manualmente, e POI muore, il kernel fa ancora l'undo causando un incremento extra → "ghost posts" (posti fantasma che non esistono).

**File coinvolti**:
- `src/programs/operatore/operatore.c` (righe 106, 236, 247)
- `src/programs/operatore_cassa/operatore_cassa.c` (righe 111, 225, 233)

**Soluzione proposta**: Usare `reserve_sem_no_undo()` per i posti delle stazioni, e gestire manualmente la morte degli operatori nel signal handler SIGCHLD.

**Complessità**: MEDIA-ALTA (richiede modifiche in più file + gestione nel signal handler)

---

#### 11. Double mutex acquisition risk (MUTEX_SIMULATION_STATS then MUTEX_SHARED_DATA)
**Problema**: Latent ABBA deadlock risk - alcune funzioni acquisiscono prima `MUTEX_SIMULATION_STATS` poi `MUTEX_SHARED_DATA`, altre potrebbero fare l'inverso.

**Cosa cercare**:
```c
// Cerca pattern pericolosi:
reserve_sem(shm->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);  // ← Ordine 1

// vs

reserve_sem(shm->semaphore_mutex_id, MUTEX_SHARED_DATA);
reserve_sem(shm->semaphore_mutex_id, MUTEX_SIMULATION_STATS);  // ← Ordine 2 (PERICOLOSO!)
```

**Soluzione**: Stabilire un ordine globale di acquisizione mutex e verificare che sia rispettato ovunque.

**Complessità**: MEDIA (grep + analisi + fix)

---

#### 12. strcpy() without bounds checking in menu parsing
**Problema**: Buffer overflow se i nomi dei piatti nel file `config/menu.conf` superano `MAX_DISH_NAME_LENGTH`.

**File da controllare**:
- Probabilmente in `src/config/` o dove viene fatto il parsing di `menu.conf`
- Cercare: `strcpy(.*dish.*name|strcpy.*menu)`

**Soluzione**: Sostituire `strcpy()` con `strncpy()` o `snprintf()`:
```c
// Prima:
strcpy(menu->first_courses[i].name, buffer);

// Dopo:
strncpy(menu->first_courses[i].name, buffer, MAX_DISH_NAME_LENGTH - 1);
menu->first_courses[i].name[MAX_DISH_NAME_LENGTH - 1] = '\0';
```

**Complessità**: BASSA

---

### LOW (5 rimanenti)

#### 13. Add validation for atoi() usage
**Problema**: `atoi()` usato senza validazione (es. in `add_users.c:106`). Se l'input non è un numero, ritorna 0 silenziosamente.

**File da controllare**:
- `src/programs/add_users/add_users.c`
- Altri programmi che leggono argomenti da linea di comando

**Soluzione**: Usare `strtol()` con controllo errori:
```c
// Prima:
users_to_add = atoi(argv[1]);

// Dopo:
char *endptr;
errno = 0;
long val = strtol(argv[1], &endptr, 10);
if (errno != 0 || *endptr != '\0' || val < 0 || val > INT_MAX) {
    fprintf(stderr, "[ERROR] Numero utenti non valido: %s\n", argv[1]);
    return -1;
}
users_to_add = (int)val;
```

**Complessità**: BASSA

---

#### 14. Check return values for sigaction/timer_create/setpgid
**Problema**: Chiamate a `sigaction()`, `timer_create()`, `setpgid()` senza controllo del valore di ritorno.

**File da controllare**:
- `src/programs/responsabile_mensa/simulation_engine.c`
- `src/programs/operatore/operatore.c`
- `src/programs/utente/utente.c`

**Soluzione**: Aggiungere controlli:
```c
// Prima:
timer_create(CLOCK_REALTIME, &sev, &daily_timer_id);

// Dopo:
if (timer_create(CLOCK_REALTIME, &sev, &daily_timer_id) == -1) {
    perror("[ERROR] timer_create failed");
    // Gestire l'errore appropriatamente
}
```

**Complessità**: BASSA (grep + aggiungi if)

---

#### 15. Fix deterministic has_ticket
**Problema**: `has_ticket` è deterministic (sempre stesso valore per stesso utente?) invece di casuale. Questo potrebbe causare pattern prevedibili.

**File da controllare**:
- Cercare `has_ticket` in `src/programs/utente/`

**Soluzione**: Se il calcolo di `has_ticket` è deterministico basato su PID o simile, renderlo casuale:
```c
// Assicurarsi che usi rand() correttamente:
bool has_ticket = (rand() % 100) < ticket_probability;
```

**Complessità**: BASSA

---

#### 16. Fix busy-wait polling in receive_message_with_soft_timeout
**Problema**: Implementazione con busy-wait invece di timeout reale su `msgrcv()`.

**File da controllare**: Cercare `receive_message_with_soft_timeout` (probabilmente in `src/ipc/queue.c` o `src/programs/operatore_cassa/`)

**Soluzione**: Usare `alarm()` o `timer_create()` per implementare timeout reale invece di busy-wait.

**Complessità**: MEDIA (dipende dall'implementazione attuale)

---

#### 17. Fix unsafe casting of char[] to struct pointers
**Problema**: Casting non sicuro di `message_text` (char array) a puntatori di struct, rischio di alignment issues.

**Esempio del problema**:
```c
StationPayload *payload = (StationPayload *)msg.message_text;
```

**File da controllare**:
- `src/programs/operatore/operatore.c`
- `src/programs/operatore_cassa/operatore_cassa.c`
- Ovunque si usi `SimulationMessage`

**Soluzione**: Usare `memcpy()` invece di cast diretto:
```c
// Prima:
StationPayload *payload = (StationPayload *)msg.message_text;

// Dopo:
StationPayload payload;
memcpy(&payload, msg.message_text, sizeof(StationPayload));
```

**Complessità**: BASSA-MEDIA (molte occorrenze da modificare)

---

## 📊 Riepilogo Progressi

- **CRITICAL**: 2/2 ✅ (100%)
- **HIGH**: 5/5 ✅ (100%)
- **MEDIUM**: 2/5 (40%)
- **LOW**: 0/5 (0%)
- **TOTALE**: 9/17 (53%)

---

## 🔧 File Modificati Finora

1. `include/common.h` - Aggiunto `pending_add_users_count`, reso `is_simulation_running` volatile
2. `include/sem.h` - Aggiunte funzioni `block_sigchld()` / `unblock_sigchld()`
3. `src/ipc/sem.c` - Implementate funzioni di protezione SIGCHLD, cambiato SEM_UNDO in `sync_child_start()`
4. `src/programs/responsabile_mensa/simulation_engine.c` - Molteplici fix (timer, mutex, barriere, refill)
5. `src/programs/responsabile_mensa/setup_ipc.c` - Inizializzato `pending_add_users_count`
6. `src/programs/add_users/add_users.c` - Protocollo sincronizzazione spawn, protezione SIGCHLD
7. `src/programs/operatore/operatore.c` - Fix EINTR handling
8. `src/programs/operatore_cassa/operatore_cassa.c` - Fix EINTR handling
9. `src/programs/utente/utente.c` - Rimossa compensazione manuale semafori gruppo

---

## ⚠️ Note Importanti Prima di Continuare

1. **TESTARE** le modifiche fatte prima di procedere con le altre fix
2. **COMPILARE** il progetto per verificare che non ci siano errori di sintassi
3. **ESEGUIRE** la simulazione per verificare che non ci siano deadlock o crash
4. Il problema #10 (SEM_UNDO on station posts) è più complesso e potrebbe richiedere un design diverso
5. Alcuni fix LOW potrebbero non essere critici per il funzionamento base

---

## 📝 Comandi Utili per il Debug

```bash
# Compila tutto
make clean && make

# Esegui simulazione
wsl ./bin/responsabile_mensa

# Controlla errori di memoria
wsl valgrind --leak-check=full ./bin/responsabile_mensa

# Trova pattern specifici
grep -r "strcpy" src/
grep -r "atoi" src/
grep -r "timer_create" src/
```
