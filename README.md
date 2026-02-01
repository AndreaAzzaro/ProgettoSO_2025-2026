# Documentazione Progetto: Simulazione Mensa Universitaria

## 1. Infrastruttura Globale e Stato del Sistema (common.h / common.c)

Questa sezione descrive i componenti fondamentali che permettono la comunicazione e la sincronizzazione tra i diversi attori della simulazione (Responsabile Mensa, Operatori e Utenti). L'intero sistema si basa sul paradigma della memoria condivisa (Shared Memory) e sull'uso orchestrato di set di semafori e code di messaggi.

### 1.1 La Memoria Condivisa Principale (MainSharedMemory)

La struttura `MainSharedMemory` rappresenta lo stato globale del sistema. È l'unica risorsa accessibile in lettura e scrittura da ogni processo, garantendo che tutti i componenti abbiano una visione coerente della simulazione (es. minuti trascorsi, disponibilità cibo, tavoli liberi).

#### Panoramica della Struttura

La struttura è progettata per essere dinamica e modulare. Di seguito lo snippet principale dell'implementazione:

```c
/**
 * @brief Struttura principale della Memoria Condivisa.
 *
 * Contiene lo stato globale della simulazione accessibile a tutti i processi.
 * Allineata per supportare il Flexible Array Member per la gestione dei gruppi.
 */
struct MainSharedMemory {
    SimulationConfiguration configuration;    /**< Parametri di configurazione caricati dai file .conf */
    SimulationStatistics statistics;          /**< Statistiche globali aggiornate in tempo reale */
    SimulationMenu food_menu;                 /**< Menu della mensa con i nomi dei piatti */

    int shared_memory_id;               /**< ID della risorsa Shared Memory stessa per operazioni di controllo */
    int semaphore_sync_id;              /**< ID Set Semafori Barriera per sincronizzazione fasi giornaliere */
    int semaphore_mutex_id;             /**< ID Set Semafori Mutex per protezione sezioni critiche */

    // ... Stazioni e Attori ...
    FoodDistributionStation first_course_station;  /**< Stazione dei primi piatti */
    CashierStation register_station;                /**< Stazione di pagamento (Cassa) */
    DiningArea seat_area;                          /**< Gestione fisica dei tavoli e posti a sedere */

    int is_simulation_running;          /**< Flag globale (1: Attiva, 0: Arresto Totale) */

    /**
     * Membro ad array flessibile per lo stato dei gruppi.
     * Permette di allocare spazio per N gruppi in base alla configurazione.
     */
    GroupStatus group_statuses[];
};
```

#### Breakdown dei Componenti della SHM

- **`GroupStatus group_statuses[]`**: Questa è una **Flexible Array Member**. Viene utilizzata per ospitare lo stato di ogni gruppo di amici in modo dinamico. Poiché il numero di gruppi può variare in base ai parametri di input, la memoria condivisa viene allocata con una dimensione calcolata a runtime: `sizeof(MainSharedMemory) + (n_gruppi * sizeof(GroupStatus))`.
- **`SimulationStatistics statistics`**: Una struttura dedicata alla raccolta dei dati (es. piatti serviti, tempi medi). Ogni volta che un utente termina un pasto, i dati vengono aggiornati qui in modo thread-safe tramite mutex.
- **Indici delle IPC**: Vengono salvati direttamente nella SHM (es. `semaphore_sync_id`). Questo permette ai processi figli descritti in altri file di accedere alle risorse senza dover ricalcolare le chiavi tramite `ftok`, accelerando le operazioni di startup.

### 1.2 Protocollo di Sincronizzazione: I Semafori

Il sistema non usa semafori singoli, ma **Set di Semafori** suddivisi per finalità logica. Questo riduce l'overhead del kernel e semplifica la gestione degli ID. Gli indici sono definiti tramite macro ed enumerazioni in `common.h`.

#### Set di Mutex (`MutexSemaphoreIndex`)

Utilizzati per garantire l'atomicità degli accessi alla SHM.

- **`MUTEX_SIMULATION_STATS`**: Protegge l'aggiornamento simultaneo delle statistiche da parte di più utenti.
- **`MUTEX_TABLES`**: Garantisce che la ricerca di un tavolo libero nell'area refezione sia un'operazione atomica, evitando che due gruppi occupino lo stesso tavolo contemporaneamente.

#### Barriera di Sincronizzazione Giornaliera (`SyncBarrierIndex`)

Implementa un pattern di tipo **Rendez-vous** o **Double Barrier**. Tutti i processi (Operatori e Utenti) devono attendere che il Direttore apra il "cancello" di inizio giornata prima di procedere, e devono segnalare la fine delle loro attività per permettere lo scatto del giorno successivo.

### 1.3 Funzioni di Gestione (common.c)

La logica di gestione risiede in `common.c` e si occupa di astrarre le operazioni di sistema.

```c
/**
 * @brief Esegue la pulizia di tutte le risorse IPC allocate.
 *
 * Questa funzione è critica per evitare "leaks" di risorse nel sistema operativo.
 * Viene chiamata dal Responsabile Mensa al termine della simulazione.
 *
 * @param shared_memory_ptr Puntatore alla struttura di memoria condivisa.
 */
void cleanup_ipc_resources(MainSharedMemory *shared_memory_ptr) {
    if (!shared_memory_ptr) return;

    // Rimozione delle code messaggi delle stazioni
    remove_message_queue(shared_memory_ptr->first_course_station.message_queue_id);

    // Rimozione dei set semaforici (Globali e di Stazione)
    delete_sem_set(shared_memory_ptr->semaphore_sync_id);

    // Detach e rimozione definitiva del segmento di memoria condivisa
    int shmid = shared_memory_ptr->shared_memory_id;
    detach_shared_memory_segment(shared_memory_ptr);
    remove_shared_memory_segment(shmid);
}
```

La funzione `terminate_simulation_gracefully` assicura che il Master attenda la terminazione di ogni processo figlio (`while (wait(NULL) > 0)`) prima di procedere alla distruzione delle IPC, garantendo che nessun processo tenti di accedere a memoria o semafori non più esistenti, evitando errori di segmentazione (Segmentation Fault).

## 2. Gestione della Configurazione (config.h / config.c)

Il modulo di configurazione funge da interfaccia tra l'utente (che definisce i parametri nel file `config.conf`) e il sistema software. Il suo compito principale è il parsing asincrono dei dati all'avvio del Responsabile Mensa, che poi propaga tali informazioni a tutti i figli tramite la memoria condivisa.

### 2.1 Architettura della Configurazione

La configurazione è organizzata gerarchicamente nella struttura `SimulationConfiguration`, suddivisa in sottogruppi funzionali:

- **Quantità (`ConfigurationQuantities`)**: Definisce il numero di processi da spawnare (Attori, Utenti iniziali, batch di nuovi utenti) e i limiti di comportamento (numero di pause consentite).
- **Capacità Area (`ConfigurationSeats`)**: Mappa i posti disponibili per ogni stazione di servizio e il numero totale di posti a sedere nell'area refezione (`NOFTABLESEATS`).
- **Tempi e Scala Temporale (`ConfigurationTimings`)**: Gestisce la velocità della simulazione. Il parametro `nanoseconds_per_tick` è cruciale: definisce quanto tempo reale deve trascorrere per ogni "minuto" simulato.
- **Soglie Operative (`ConfigurationThresholds`)**: Include parametri per la gestione delle emergenze (overload) e le soglie di rifornimento per i primi e secondi piatti.

### 2.2 Il Meccanismo di Parsing (Lookup Table)

Per garantire flessibilità e facilità di estensione, l'implementazione in `config.c` evita l'hardcoding delle chiavi di configurazione nelle logiche di switch. Viene utilizzata una **Tabella di Lookup**:

```c
/**
 * @brief Associazione tra stringa nel file e identificatore Enum.
 */
typedef struct {
    const char *string_key;        /**< Chiave letterale nel file .conf */
    ConfigurationKey enum_identifier; /**< Identificatore interno univoco */
} ConfigurationKeyMap;

/** Tabella di lookup per mappatura stringa -> enum */
static const ConfigurationKeyMap configuration_mapping_table[] = {
    {"NOF_WORKERS", KEY_NUMBER_OF_WORKERS},
    {"PRICE_PRIMI", KEY_PRICE_PRIMI},
    {"SIM_DURATION", KEY_SIMULATION_DURATION},
    // ... Altre mappature ...
    {NULL, KEY_UNKNOWN}
};
```

#### Funzionamento del Caricamento:

1. **Risoluzione Chiave**: La funzione `resolve_configuration_key` scansiona la tabella di lookup per trovare l'identificatore enum corrispondente alla stringa letta.
2. **Assegnazione Tipizzata**: Lo `switch` principale nel file `config.c` utilizza l'enum per assegnare il valore alla variabile corretta, eseguendo il casting appropriato (es. `atol` per tempi lunghi, `double` per i prezzi).
3. **Robustezza**: Il parser ignora le righe di commento (`#`) e gestisce file di configurazione con percorsi personalizzati o default.

### 2.3 Importanza del Modulo nella Sincronizzazione

Sebbene il parsing avvenga nel processo Master, la struttura `SimulationConfiguration` fa parte della `MainSharedMemory`.
Questo significa che una volta caricata dal Responsabile Mensa, ogni processo operatore o utente può accedere istantaneamente ai propri parametri operativi (es. tempo medio di servizio, soglie di pazienza) senza dover rileggere il disco, garantendo prestazioni elevate e coerenza dei dati tra tutti i nodi della simulazione.

## 3. Gestione del Menu e dei Piatti (menu.h / menu.c)

Il modulo Menu gestisce il catalogo gastronomico della mensa. La sua funzione è caricare i nomi dei piatti disponibili e fornire un'interfaccia per la risoluzione dei nomi durante la simulazione e la fase di reporting delle statistiche.

### 3.1 Struttura del Menu in Memoria Condivisa

Per permettere a ogni Utente e Operatore di conoscere istantaneamente l'offerta della mensa, la struttura `SimulationMenu` è inclusa direttamente nella memoria condivisa globale.

```c
/**
 * @brief Struttura completa del menu della mensa.
 *
 * Contiene tutti i piatti suddivisi per categoria, con contatori
 * per il numero effettivo di piatti caricati.
 */
typedef struct {
    MenuDish first_courses[MAX_DISHES_PER_CATEGORY];   /**< Array dei primi piatti */
    int number_of_first_courses;                        /**< Numero di primi effettivamente caricati */

    // ... Altre categorie (Secondi, Contorni, Dolci, Bevande) ...

    MenuDish beverage_courses[MAX_DISHES_PER_CATEGORY];/**< Array delle bevande */
    int number_of_beverage_courses;                     /**< Numero di bevande caricate */
} SimulationMenu;
```

#### Caratteristiche Tecniche:

- **Allocazione Statica**: Gli array dei piatti hanno una dimensione massima predefinita (`MAX_DISHES_PER_CATEGORY`). Questa scelta previene problemi di puntatori non validi tra spazi di indirizzamento di processi diversi, tipici della memoria dinamica non condivisa correttamente.
- **Integrità del Referenziamento**: Durante la simulazione, gli utenti selezionano i piatti tramite il loro indice nell'array. Questo rende lo scambio di messaggi (interchange) estremamente leggero, poiché non vengono mai scambiate stringhe di testo tra i processi.

### 3.2 Il Parser del Menu

Il caricamento avviene all'avvio dal file `menu.conf`. Il formato previsto è `[CATEGORIA] [NOME_PIATTO]`.

- `P`: Primo Piatto
- `S`: Secondo Piatto
- `C`: Contorno (Side)
- `D`: Dolce (Dessert)
- `B`: Bevanda (Beverage)

L'implementazione utilizza una tabella di mappatura interna che associa questi caratteri alle enumerazioni `MenuDishCategory`, garantendo un caricamento ordinato negli specifici array della struttura.

### 3.3 Risoluzione del Nome Piatto (ID-to-Name Recovery)

Una delle funzioni più importanti è `get_dish_name_by_id`. Questa funzione è l'unico punto che permette di trasformare una coppia `(Categoria, Indice)` nel nome visualizzabile della pietanza.

```c
/**
 * @brief Recupera il nome di un piatto partendo dalla categoria e dall'indice.
 *
 * @param menu_ptr Puntatore alla struttura menu in memoria condivisa.
 * @param category Categoria del piatto (MenuDishCategory).
 * @param dish_index Indice del piatto 0-based.
 * @return const char* Nome leggibile del piatto.
 */
const char* get_dish_name_by_id(SimulationMenu *menu_ptr, MenuDishCategory category, int dish_index);
```

Questa astrazione è vitale per:

- **Logging**: Mostrare nei log di sistema "L'utente mangia Pasta" invece di "L'utente mangia Piatto 0".
- **Statistiche**: Generare il report finale aggregando i dati per nome del piatto, facilitando l'analisi post-simulazione.

## 4. Meccanismi di Sincronizzazione: I Semafori (sem.h / sem.c)

Il coordinamento tra centinaia di processi concorrenti è affidato al modulo Semafori. Questo modulo astrae la complessità delle syscall System V per fornire primitive ad alto livello, garantendo l'assenza di Race Condition e Deadlock.

### 4.1 Architettura del Wrapper Semaforico

Il core del modulo è la gestione robusta delle operazioni atomiche tramite la funzione privata `_execute_sem_op`.

```c
/**
 * @brief Esegue un'operazione semaforica atomica con retry su EINTR.
 *
 * Questa funzione gestisce il loop di riprova se la chiamata semop() viene
 * interrotta da un segnale di sistema (errno == EINTR).
 */
static int _execute_sem_op(int sem_id, int sem_idx, short op, short flags) {
    struct sembuf sb;
    sb.sem_num = (unsigned short)sem_idx;
    sb.sem_op = op;
    sb.sem_flg = flags;

    while (semop(sem_id, &sb, 1) == -1) {
        if (errno != EINTR) return -1; // Fallimento critico
    }
    return 0;
}
```

#### Scelta dei Flag: `SEM_UNDO` vs No-Undo

Il progetto adotta una politica differenziata per la gestione della persistenza semaforica:

- **`SEM_UNDO` Attivo**: Utilizzato per i Mutex (sezione 1.2). Garantisce che se un Utente o un Operatore termina inaspettatamente, il kernel "pulisca" le sue acquisizioni, evitando che una risorsa rimanga bloccata per sempre.
- **Senza `SEM_UNDO`**: Utilizzato nelle barriere dove il superamento della fase dipende dal raggiungimento dello zero. Un "reset" automatico del kernel qui falserebbe il conteggio dei processi pronti.

### 4.2 Pattern Implementati

#### La Barriera di Sincronizzazione (Ready-Gate)

Per assicurare che tutti i processi partano esattamente nello stesso istante (es. all'apertura quotidiana), viene usato il pattern Barrier:

1. **Fase di Appello**: Ogni processo figlio esegue `sync_child_start`, che decrementa un semaforo di "pronti" (`ready_idx`).
2. **Attesa Cancello**: Subito dopo, il figlio si blocca su `wait_for_zero` del semaforo "cancello" (`gate_idx`).
3. **Sblocco Globale**: Il Responsabile Mensa, monitorando il semaforo di appello, quando vede che è arrivato a zero, "apre il cancello" portandolo a zero. Tutti i figli si svegliano istantaneamente.

#### Acquisizione Atomica di Risorse Multiple

Per la gestione del _Social Seating_ (più amici che cercano posti insieme), viene utilizzata l'acquisizione ponderata:

```c
/**
 * @brief Esegue l'acquisizione atomica di una quantità n di risorse.
 * Usato dai gruppi per occupare N sedie contemporaneamente.
 */
int reserve_sem_amount(int sem_id, int semaphore_index, int amount) {
    if (amount <= 0) return 0;
    return _execute_sem_op(sem_id, semaphore_index, (short)(-amount), SEM_UNDO);
}
```

Questa operazione è fondamentale: assicura che un gruppo di 4 persone occupi 4 posti solo se sono tutti disponibili contemporaneamente, evitando che due sottogruppi si "incastrino" occupando posti parziali.

### 4.3 Gestione delle Risorse

Il modulo include `delete_sem_set` per la deallocazione definitiva delle strutture nel kernel Linux via `IPC_RMID`. Questa operazione viene richiamata centralmente in `cleanup_ipc_resources` (Sezione 1.3).

## 5. Comunicazione tramite Memoria Condivisa (shm.h / shm.c)

La Memoria Condivisa (Shared Memory) rappresenta il "database in real-time" della simulazione. Il modulo `shm.h/c` fornisce l'agente di basso livello che interagisce con il kernel Linux per gestire i segmenti di memoria System V.

### 5.1 Il Modello di Allocazione e Accesso

Per garantire che ogni processo (Responsabile, Utente o Operatore) possa leggere e scrivere lo stato della simulazione senza passare messaggi pesanti, il modulo implementa un pattern di **Attachment Dinamico**.

```c
/**
 * @brief Collega il segmento di memoria condivisa allo spazio di indirizzamento.
 *
 * @param shared_memory_id ID della memoria condivisa restituito da create.
 * @param is_read_only Se true, impedisce la scrittura nel segmento per questo processo.
 * @return void* Puntatore tipizzato all'area di memoria, NULL su errore.
 */
void *attach_shared_memory_segment(int shared_memory_id, bool is_read_only) {
    int connection_flags = is_read_only ? SHM_RDONLY : 0;
    void *attached_address = shmat(shared_memory_id, NULL, connection_flags);

    if (attached_address == (void *)-1) {
        perror("shmat failed");
        return NULL;
    }
    return attached_address;
}
```

#### Soluzioni di Robustezza:

- **Normalizzazione degli Errori**: Il wrapper cattura il valore di errore di sistema `(void*)-1` e lo normalizza a `NULL`, rendendo i controlli di sicurezza uniformi in tutto il resto del progetto.
- **Supporto Read-Only**: Attraverso il parametro `is_read_only`, il sistema può isolare i processi che devono solo monitorare lo stato (come i generatori di statistiche) da quelli che devono modificarlo, riducendo i rischi di corruzione involontaria.

### 5.2 Gestione del Ciclo di Vita delle Risorse

Il modulo segue rigorosamente il ciclo di vita System V:

1. **Creazione**: Il Responsabile Mensa invoca `create_shared_memory_segment` all'avvio del sistema, calcolando la dimensione necessaria inclusiva dei gruppi dinamici.
2. **Distribuzione**: L'ID (`shmid`) viene distribuito ai processi figli tramite argomenti della riga di comando (`execv`).
3. **Utilizzo**: Ogni figlio invoca il wrapper per mappare la memoria nel proprio spazio virtuale.
4. **Cleanup**: Al termine della simulazione, dopo che tutti i processi hanno eseguito il `detach`, il Responsabile Mensa chiama `remove_shared_memory_segment` inviando il comando `IPC_RMID` al kernel.

```c
/**
 * @brief Rimuove definitivamente il segmento dal sistema operativo.
 * Viene eseguita solo dal Master durante la chiusura globale.
 */
int remove_shared_memory_segment(int shared_memory_id) {
    return shmctl(shared_memory_id, IPC_RMID, NULL);
}
```

### 5.3 Coerenza con il Pattern "Double Prototype"

Il modulo `shm` funge da implementazione di basso livello. Le logiche di business (come l'inizializzazione del contenuto della memoria) sono delegate a `common.c`, mantenendo una separazione netta tra la gestione delle risorse OS e la logica applicativa.

## 6. Comunicazione Asincrona: Code di Messaggi (queue.h / queue.c)

Mentre la Shared Memory gestisce lo stato "statico", le code di messaggi (Message Queues) gestiscono il flusso dinamico delle operazioni. Esse permettono lo scambio di ordini tra Utenti e Operatori e la gestione delle richieste di ingresso (add_users).

### 6.1 Lo Standard dei Messaggi (SimulationMessage)

Il sistema adotta una struttura unificata per tutti i messaggi scambiati nelle IPC System V, garantendo compatibilità tra i diversi moduli.

```c
/**
 * @brief Struttura per lo scambio di messaggi IPC.
 */
typedef struct {
    long message_type;                           /**< Tipo del messaggio (Priorità o Destinatario) */
    char message_text[MAX_MESSAGE_TEXT_SIZE];    /**< Payload informativo (es. ID piatto o PID utente) */
} SimulationMessage;
```

#### Protocollo di Utilizzo:

- **`message_type`**: Viene utilizzato per distinguere la tipologia di interazione (es. 1 per Ordini, 2 per Pagamenti) o per indirizzare un messaggio a un processo specifico fornendo il suo PID come tipo.
- **`message_text`**: Un buffer serializzato che contiene i dettagli dell'operazione. Questo approccio a payload fisso previene errori di buffer overflow tra processi con spazi di indirizzamento diversi.

### 6.2 Affidabilità della Comunicazione

Le code di messaggi possono essere bloccanti. Il modulo `queue.c` implementa meccanismi di guardia per assicurare che nessun messaggio venga perso o ignorato:

```c
/**
 * @brief Invia un messaggio con gestione automatica delle interruzioni.
 */
int send_message_to_queue(int message_queue_id, SimulationMessage *message_ptr, size_t size, int flags) {
    while (msgsnd(message_queue_id, (void *)message_ptr, size, flags) == -1) {
        if (errno != EINTR) return -1; // Riprova se interrotto da segnale
    }
    return 0;
}
```

#### Gestione degli Errori di Runtime:

- **`ENOMSG`**: Gestito nelle letture non bloccanti (es. controllo dei nuovi utenti da aggiungere), permettendo al Master di proseguire se la coda è vuota.
- **`EIDRM`**: Gestito durante la chiusura del sistema. Se il Responsabile rimuove la coda mentre un Operatore è in attesa di un ordine, l'Operatore riconosce la rimozione della risorsa e termina il proprio ciclo di lavoro in modo pulito.

### 6.3 Monitoraggio e Analisi del Carico

Il sistema utilizza le statistiche IPC del kernel per monitorare lo stato di salute della mensa. La funzione `get_message_queue_length` interroga la struttura `msqid_ds` gestita dal kernel per conoscere il numero di messaggi pendenti (`msg_qnum`).
Questa informazione è vitale per le statistiche finali, indicando ad esempio quanti ordini sono rimasti inevasi al momento della chiusura (Leftover Plates).

## 7. Raccolta Dati e Statistiche (statistics.h / statistics.c)

Il modulo Statistiche è responsabile del monitoraggio delle performance della mensa. Esso aggrega i dati provenienti da tutti i processi attivi e genera report dettagliati sia a terminale che su file persistente.

### 7.1 Gestione Sicura degli Aggregatori

Tutti i dati statistici risiedono nella sottostruttura `SimulationStatistics` all'interno della memoria condivisa. Data la natura concorrente del sistema, l'estrazione dei dati segue un protocollo di isolamento:

```c
/**
 * @brief Raccoglie le statistiche dalla SHM in modo atomico.
 */
SimulationStatistics collect_simulation_statistics(MainSharedMemory *shm_ptr) {
    SimulationStatistics stats;
    // Blocco delle scritture concorrenti durante la copia
    reserve_sem(shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);
    memcpy(&stats, &shm_ptr->statistics, sizeof(SimulationStatistics));
    release_sem(shm_ptr->semaphore_mutex_id, MUTEX_SIMULATION_STATS);

    // Calcolo delle medie dinamiche basate sui campioni raccolti
    // ...
    return stats;
}
```

#### Soluzioni Implementate:

- **Accumulatori di Tempo**: Invece di memorizzare medie parziali, il sistema salva la `sum_wait` (somma millisecondi) e il `count` (numero campioni). Questo metodo elimina l'errore di precisione cumulativo e permette di calcolare la media reale in qualsiasi istante.
- **Isolamento Mutex**: L'uso del mutex `MUTEX_SIMULATION_STATS` previene lo scenario in cui un report venga generato mentre un utente sta aggiornando il proprio tempo di attesa, garantendo la coerenza del report finale.

### 7.2 Struttura dei Report

Il sistema genera tre tipologie di output informativo:

1.  **Report Giornaliero (Log di Sistema)**: Stampato a fine giornata, mostra l'efficienza degli operatori e il numero di piatti avanzati (Leftover Plates).
2.  **Report Finale**: Riepilogo complessivo con analisi della causa di terminazione (es. Overload se i rinunciatari superano la soglia critica).
3.  **Log CSV Persistente**: Salvataggio su file `statistics_report.csv` per analisi statistiche esterne.

### 7.3 Analisi della Performance (KPI)

Il modulo traccia indicatori chiave (Key Performance Indicators) fondamentali per valutare la gestione della mensa:

- **Waste Ratio**: Rapporto tra piatti preparati e piatti effettivamente consumati.
- **Wait Time Consistency**: Analisi dei tempi medi di attesa stazione per stazione, vitale per identificare colli di bottiglia nel servizio.
- **Ticket Efficiency**: Percentuale di utenti che accedono tramite ticket automatico rispetto al totale dei serviti.

## 8. Utility e Gestione del Tempo (utils.h / utils.c)

Le funzioni di utilità forniscono il supporto infrastrutturale per la generazione di eventi stocastici e la gestione precisa del timing della simulazione, garantendo la robustezza del sistema contro le interruzioni asincrone.

### 8.1 Simulazione del Tempo Reale

La precisione temporale è garantita dall'uso della syscall `nanosleep`. Il sistema implementa un meccanismo di "sonno persistente" che impedisce ai segnali di accorciare i tempi di attesa previsti (es. durata del pasto).

```c
/**
 * @brief Simula il passaggio del tempo con gestione delle interruzioni.
 */
void simulate_time_passage(int units, long nanosecs_per_tick) {
    struct timespec req;
    long total_ns = (long)units * nanosecs_per_tick;
    req.tv_sec = total_ns / 1000000000L;
    req.tv_nsec = total_ns % 1000000000L;

    // Riprova finché il tempo richiesto non è effettivamente trascorso
    while (nanosleep(&req, &req) == -1) {
        if (errno != EINTR) break;
    }
}
```

#### Soluzioni di Timing:

- **Resilienza ai Segnali**: Grazie all'aggiornamento automatico della struttura `req` operato dal kernel durante l'interruzione, la funzione riprende l'attesa per il tempo rimanente esatto, evitando che il traffico di segnali (es. interazione Master-Figli) acceleri artificialmente la simulazione.
- **Convertitore di Scala**: Permette di trasformare i parametri di configurazione (es. `AVG_SRVC_TICKET`) in nanosecondi reali, scalando l'intera simulazione in base alle esigenze di test (es. simulare un'ora in un secondo).

### 8.2 Modello Probabilistico e Variabilità

Per rendere la simulazione realistica, il modulo implementa meccanismi di variabilità statistica conformi ai requisiti di progetto (es. tempi medi con scostamento del 50%).

- **`calculate_varied_time`**: Calcola un valore casuale in un raggio percentuale attorno alla media. Questo assicura che, sebbene la media dei pasti sia di 30 minuti, alcuni utenti finiscano prima e altri dopo, generando dinamiche di coda non deterministiche.
- **`evaluate_probability_event`**: Utilizzato per decisioni binarie (es. "L'utente prende il dessert?", "Si verifica un Communication Disorder?").

### 8.3 Gestione Centralizzata dei Fallimenti

La funzione `display_critical_error` funge da barriera finale per la sicurezza del sistema. Ogni fallimento nelle syscall IPC o nella gestione della memoria invoca questa utility, che:

1.  Utilizza `perror` per tradurre l'ultimo `errno` del sistema in un messaggio comprensibile.
2.  Interrompe immediatamente il processo (`EXIT_FAILURE`), innescando la gestione dei segnali nel Master per la pulizia delle risorse (Sezione 1.3).

## 9. Il Responsabile Mensa: Orchestrazione e Ciclo di Vita (responsabile_mensa.h / .c)

Il processo Master (Responsabile Mensa) funge da supervisore dell'intero ecosistema. Esso coordina l'inizializzazione, la sincronizzazione di massa e la terminazione sicura della simulazione, agendo secondo un'architettura a **Facade**.

### 9.1 Fasi di Inizializzazione (Bootstrapping)

Il Master segue un ordine rigoroso di attivazione per garantire la stabilità delle comunicazioni IPC:

1.  **Caricamento Parametri**: Lettura di `config.conf` e `menu.conf`.
2.  **Allocazione Spazio Globale**: Calcola la memoria necessaria per ospitare la `MainSharedMemory` e gli stati di tutti i gruppi di utenti.
3.  **Wiring delle Risorse**: Crea la Shared Memory, i Set di Semafori (Mutex e Barriere) e le Code di Messaggi.
4.  **Installazione Handler**: Configura la gestione dei segnali di sistema (`SIGCHLD`, `SIGINT`, `SIGALRM`).

### 9.2 La Barriera di Startup (Synchronized Startup)

Per prevenire race condition in cui un utente tenta di inviare un ordine a una stazione non ancora inizializzata, il Master implementa una **Startup Barrier**.

```c
/**
 * @brief Gestisce l'attesa del Master sulla barriera di startup.
 */
void synchronize_prework_barrier(MainSharedMemory *shm_ptr) {
    // Attesa che il contatore READY arrivi a 0
    // Significa che N figli hanno chiamato sync_child_start()
    wait_for_zero(shm_ptr->semaphore_sync_id, BARRIER_STARTUP_READY);

    // Sblocco del cancello globale per l'inizio del servizio
    open_barrier_gate(shm_ptr->semaphore_sync_id, BARRIER_STARTUP_GATE);
}
```

Nessun processo figlio può iniziare a operare finché il Master non ha aperto il cancello globale. Questo garantisce che la simulazione parta da uno stato di coerenza totale (`is_simulation_running = 1`).

### 9.3 Gestione Dinamica della Popolazione

Il Master non gestisce solo i processi iniziali, ma monitora l'evoluzione della popolazione tramite:

- **`setup_worker_distribution`**: Assegnazione degli operatori alle stazioni in base alla configurazione.
- **`launch_simulation_users`**: Creazione della prima ondata di clienti.
- **`SIGCHLD` Monitoring**: Ricezione dei segnali di morte dei figli per aggiornare i contatori dei serviti o rilevare fallimenti critici.

### 9.4 Terminazione Coordinata e Cleanup

In caso di fine simulazione (per raggiungimento della durata o per overload), il Master attiva la procedura `terminate_simulation_gracefully`.
Questa operazione è fondamentale per:

1.  Inviare un segnale di terminazione ai gruppi di processi figli.
2.  Attendere che ogni figlio abbia effettuato il `detach` dalla memoria (`wait(NULL)`).
3.  Invocare il cleanup definitivo delle IPC (Sezione 1.3), lasciando il sistema operativo pulito.

## 10. Il Motore Temporale (simulation_engine.h / .c)

Il `Simulation Engine` gestisce la logica degli stati della mensa, scandendo lo scorrere del tempo simulato tramite timer POSIX e coordinando le fasi di apertura, rifornimento e chiusura.

### 10.1 Anatomia del Ciclo Giornaliero

La simulazione è suddivisa in giorni, ognuno dei quali segue una rigida sequenza di sincronizzazione coordinata dal Master:

1.  **Fase Morning (Startup)**: Il Master inizializza le risorse, esegue il primo refill e attende che tutti i processi figli si dichiarino pronti sulla barriera mattutina.
2.  **Fase Operativa (Service)**: Il Master arma un timer POSIX (`timer_settime`) pari alla durata del turno. In questa fase, il Master rimane in attesa passiva (`pause()`), svegliandosi solo per eventi asincroni (refill o segnali di emergenza).
3.  **Fase Evening (Teardown)**: Allo scadere del timer, il Master invia un broadcast `SIGUSR2` ai figli. Segue una fase di calcolo delle statistiche, gestione degli utenti avanzati e calcolo dello spreco alimentare (Food Waste).

### 10.2 Il Sistema di Rifornimento Asincrono

Il rifornimento delle stazioni simula l'arrivo dei cuochi che caricano nuove porzioni.

```c
/**
 * @brief Gestisce un ciclo di rifornimento dei piatti.
 */
void handle_refill_cycle(MainSharedMemory *shm) {
    // Simulazione tempo fisico di caricamento (variabile)
    simulate_time_passage(varied_refill_time, shm->configuration.timings.nanoseconds_per_tick);

    // Blocco delle stazioni e incremento porzioni
    release_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
    shm->first_course_station.portions[i] += refill_amount;
    reserve_sem(shm->first_course_station.semaphore_set_id, STATION_SEM_REFILL_GATE);
}
```

L'uso del semaforo `STATION_SEM_REFILL_GATE` garantisce che nessun operatore tenti di prelevare porzioni mentre il Master sta aggiornando i contatori, prevenendo corruzioni della memoria condivisa.

### 10.3 Tolleranza ai Guasti e Gestione Zombies

Un aspetto critico gestito dall'engine è la morte inaspettata dei processi figli. L'handler `handle_sigchld` implementa una logica di **Auto-Riparazione delle Barriere**:

- Se un processo muore prima di aver raggiunto una barriera (es. `BARRIER_EVENING_READY`), il Master rileva la morte tramite `waitpid` e decrementa forzatamente il semaforo mancante.
- Questa soluzione impedisce il blocco dell'intero sistema (Deadlock di Barriera) a causa del fallimento di un singolo componente.

### 10.4 Condizioni di Terminazione

La simulazione può terminare per tre motivi principali:

1.  **Esecuzione Completata**: Raggiungimento del numero di giorni previsto (`simulation_duration_days`).
2.  **Overload Critico**: Se in un singolo giorno il numero di utenti che rinunciano supera la soglia `overload_threshold`.
3.  **Intervento Manuale**: Ricezione di `SIGINT` o `SIGTERM` da parte dell'utente.

## 11. Allocazione e Configurazione Risorse (setup_ipc.h / .c)

Il modulo `setup_ipc` gestisce la creazione fisica delle risorse System V nel kernel Linux. Implementa logiche di allocazione dinamica e ottimizzazione dei buffer per garantire alte prestazioni sotto carico.

### 11.1 Allocazione Dinamica della Memoria Globale

La memoria condivisa non ha una dimensione fissa, ma viene calcolata all'avvio per ospitare il numero esatto di gruppi di utenti previsti dalla configurazione.

```c
/**
 * @brief Calcola e alloca il segmento SHM con Flexible Array Member.
 */
MainSharedMemory* initialize_simulation_shared_memory(int group_pool_size) {
    size_t total_size = sizeof(MainSharedMemory) + (group_pool_size * sizeof(GroupStatus));
    int shmid = create_shared_memory_segment(key, total_size, IPC_CREAT | 0666);
    // ...
    return attach_shared_memory_segment(shmid, false);
}
```

#### Caratteristiche dell'Allocazione:

- **Zero-Initialization**: Ogni byte della SHM viene azzerato (`memset`) prima dell'uso per evitare che dati residui di sessioni precedenti corrompano lo stato.
- **Fail-Safe**: Se una risorsa con la stessa chiave esiste già (es. dopo un crash), il sistema esegue una rimozione atomica e una ricreazione per assicurare la pulizia del kernel.

### 11.2 Tuning delle Code di Messaggi

Per supportare il traffico intenso tra centinaia di Utenti e decine di Operatori, il Master riconfigura i limiti del kernel per le Message Queues:

```c
// Espansione del buffer della coda a 64KB
struct msqid_ds ds;
msgctl(msqid, IPC_STAT, &ds);
ds.msg_qbytes = 65536;
msgctl(msqid, IPC_SET, &ds);
```

Questa operazione è fondamentale: impedisce che le funzioni di invio (`msgsnd`) vadano in blocco forzato a causa del riempimento della coda, situazione comune quando molti utenti ordinano piatti contemporaneamente.

### 11.3 Gestione dei Pool Semaforici

Invece di allocare centinaia di singoli semafori, il sistema utilizza la tecnica del **Semaforo Clusterizzato**:

- **Set Mutex**: Un unico set contiene tutti i lock per le statistiche, i tavoli e i dati comuni.
- **Set Gruppi**: Un set massivo gestisce la sincronizzazione di tutti i gruppi di utenti. Ogni gruppo accede ai propri semafori tramite un calcolo di offset: `base_index = group_index * 3`.
  Questa architettura riduce drasticamente il numero di identificatori IPC consumati, rispettando i limiti tipici delle configurazioni kernel standard di Linux.

### 11.4 Isolamento tramite IPC_PRIVATE

L'uso del flag `IPC_PRIVATE` garantisce che le risorse create non siano visibili tramite chiavi note all'esterno dei processi figli. Solo i processi generati dal Master (che ricevono l'ID via `exec`) possono accedere alle risorse, implementando un livello di sicurezza nativo a livello di sistema operativo.

## 12. Gestione della Popolazione (setup_population.h / .c)

Il modulo `setup_population` si occupa della genesi dei processi figli e dell'organizzazione gerarchica del sistema tramite i Process Group di Linux.

### 12.1 Ottimizzazione del Personale nelle Stazioni

Il numero di operatori per ogni stazione non è fisso, ma viene calcolato dinamicamente per massimizzare il throughput:

```c
/**
 * @brief Distribuisce gli operatori in proporzione ai tempi di servizio.
 */
void calculate_worker_distribution(int total, int *avg_times, int *results, int count) {
    // Proporzionalità: Stazione più Lenta -> Più Operatori
    results[i] = (int)((double)total * avg_times[i] / sum_times);
    // ...
}
```

Questa logica riduce la probabilità di formazione di "hot-spot" (code eccessive) in una singola stazione, bilanciando il carico di lavoro degli operatori.

### 12.2 Architettura dei Segnali di Gruppo (PGID)

Per gestire simultaneamente centinaia di processi, il Master assegna un **Process Group ID (PGID)** unico a ciascuna categoria:

- **`GROUP_USERS`**: Contiene tutti gli Utenti.
- **`GROUP_FIRST_COURSES` / `SECOND` / `DESSERT`**: Raggruppano gli operatori per stazione.
- **`GROUP_CASHIERS`**: Raggruppa i cassieri.

Questo permette al Master di eseguire broadcast immediati (es. notifica di chiusura serale) utilizzando segnali inviati al PGID negativo: `kill(-pgid, signal)`.

### 12.3 Social Seating e Gestione Gruppi

Il sistema implementa un modello di interazione sociale tra gli utenti:

- **Pianificazione Gruppi**: Gli utenti vengono creati in gruppi (amici). Il Master designa un **Leader** che coordina le interazioni atomiche (es. la richiesta di un tavolo per N persone).
- **Topologia Tavoli**: All'avvio viene generata una mappa dinamica di tavoli con capacità mista (2, 4, 6 posti).

### 12.4 Registro di Sicurezza (Self-Healing)

Il Master mantiene un `user_registry` in memoria condivisa dove associa ogni `pid` al proprio `group_index`. Questo registro è vitale per il meccanismo di **Auto-Riparazione**: se un processo utente crasha, il Master rileva la morte tramite `SIGCHLD`, consulta il registro e pulisce le risorse semaforiche del relativo gruppo, evitando deadlock del sistema.

## 13. Gli Operatori: Logica di Servizio e Continuità (operatore.h / operatore_cassa.h)

I processi operatore rappresentano il personale della mensa. Operano secondo un modello di **Servizio Passivo** guidato da code di messaggi e sincronizzato per garantire la continuità operativa.

### 13.1 Ciclo di Vita a Tre Livelli

Ogni operatore gestisce il proprio tempo seguendo una gerarchia di loop:

1.  **Simulazione (Settimana)**: Ciclo macroscopico che termina solo alla chiusura globale del sistema.
2.  **Esercizio (Giorno)**: Sincronizzazione atomica con le barriere del Master.
3.  **Turno (Postazione)**: Fase in cui l'operatore compete per una sedia al bancone (`STATION_SEM_AVAILABLE_POSTS`).

### 13.2 Protocollo di Evasione Ordini

Il core business degli operatori si basa sullo scambio di messaggi IPC:

- **Evasione Piatti**: L'operatore riceve un ordine, verifica in Memoria Condivisa la disponibilità della porzione e simula il tempo di servizio (calcolato con variabilità del ±50% o ±80% per caffè/dolci).
- **Gestione Cassa**: Il cassiere calcola l'importo totale in base al menu, applica eventuali sconti ticket (50%) e aggiorna l'incasso globale protetto da mutex.

```c
/**
 * @brief Esempio di gestione atomica dell'incasso.
 */
reserve_sem(shm->mutex_id, MUTEX_SIMULATION_STATS);
shm->statistics.income_statistics.current_daily_income += amount;
release_sem(shm->mutex_id, MUTEX_SIMULATION_STATS);
```

### 13.3 Algoritmo anti-interruzione (Pause Policy)

Per evitare che una stazione rimanga scoperta, il sistema implementa una politica di pause asimmetrica:

- Un operatore rilascia la sua postazione solo se ci sono altri colleghi attivi al bancone.
- La decisione di andare in pausa avviene solo tra un cliente e l'altro (punto di coerenza), evitando di lasciare ordini a metà in caso di segnalazione esterna (`SIGUSR1`).
- L'uso di `SEM_UNDO` nell'acquisizione della postazione garantisce che, in caso di crash dell'operatore, il posto al bancone venga liberato automaticamente dal kernel.

### 13.4 Resilience al Communication Disorder (Solo Cassieri)

I cassieri implementano una barriera reattiva per simulare i guasti tecnici:

- Prima di processare ogni pagamento, eseguono una `wait_for_zero` sul semaforo `STATION_SEM_STOP_GATE`.
- Se un processo esterno (Disorder Manager) incrementa tale semaforo, tutti i cassieri si bloccano istantaneamente, liberando la CPU finché il problema non viene risolto (decremento a zero).

## 14. L'Utente: Comportamento Sociale e Percorso Mensa (utente.h / .c)

Il processo Utente implementa il comportamento del cliente finale, gestendo un complesso grafo di stati e interagendo con i propri "amici" per coordinare le fasi collettive del pasto.

### 14.1 Il Modello di Gruppo (Social Seating)

Ogni gruppo di utenti agisce come un'unità logica coordinata da un **Leader**. La sincronizzazione di gruppo utilizza un set di semafori privati che implementano tre punti di sincronizzazione fondamentali:

- **Meeting Point**: Gli amici si riuniscono dopo le stazioni di distribuzione e prima della cassa. Nessun membro procede al pagamento finché l'intero gruppo non è fisicamente presente al punto di incontro.
- **Table Booking**: Il Leader cerca atomicamente nel registro dei tavoli in SHM una postazione con capacità sufficiente per l'intero gruppo (`capacity >= group_size`). Una volta trovato il tavolo, sblocca il `GROUP_SEM_TABLE_GATE` per permettere agli amici di sedersi.
- **Exit Ceremony**: Al termine del pasto e del caffè, il gruppo attende che l'ultimo membro abbia finito prima di liberare lo spazio in SHM ed uscire simultaneamente dalla mensa.

### 14.2 Logica di Scelta e Ripiego (Choice Handling)

L'utente non si limita a richiedere un piatto, ma valuta attivamente lo stato della mensa per massimizzare la probabilità di successo:

1.  **Pazienza (Queue Monitoring)**: Prima di mettersi in coda, l'utente consulta il numero di messaggi pendenti nella MQ della stazione (`msg_qnum`). Se la coda supera la soglia di pazienza generata casualmente, l'utente "salta" la stazione per evitare ritardi eccessivi.
2.  **Sostituzione Atomica**: Se l'ordine fallisce perché il piatto preferito è terminato (`ORDER_STATUS_OUT_OF_STOCK`), l'utente consulta istantaneamente il menu in Memoria Condivisa per scegliere un'alternativa ancora disponibile, evitando di perdere il turno di servizio.

### 14.3 Gestione del Tempo e Timeouts Soft

Per rimanere reattivo ai segnali del Master (es. chiusura improvvisa della mensa) anche durante le fasi di attesa su code di messaggi, l'utente implementa un meccanismo di **Soft Timeout**:

```c
/**
 * @brief Ricezione da MQ con monitoraggio reattivo del ciclo giornaliero.
 */
ssize_t receive_message_with_soft_timeout(int queue_id, SimulationMessage *msg, size_t size, long type) {
    while (local_daily_cycle_is_active) {
        // Tentativo di ricezione non bloccante
        ssize_t res = msgrcv(queue_id, msg, size, type, IPC_NOWAIT);
        if (res != -1) return res;
        if (errno != ENOMSG) return -1;
        // Polling bilanciato per garantire reattività ai segnali di chiusura
        usleep(5000);
    }
    return -1; // Interruzione dovuta a fine giornata o segnale
}
```

### 14.4 Formal Withdrawal (Prevenzione Deadlock)

Se un utente è costretto a ritirarsi (es. per esaurimento del cibo in tutte le stazioni o per ordine del Master), esegue una procedura di **Ritiro Formale**. Questa operazione decrementa il contatore dei membri attivi del gruppo e sblocca i semafori di sincronizzazione pendenti, garantendo che i suoi amici non rimangano bloccati indefinitamente (Deadlock di Gruppo) in attesa di un processo che ha terminato l'esecuzione.

## 15. Simulazione Guasti: Communication Disorder (communication_disorder.h / .c)

Il modulo `communication_disorder` è un'utility standalone progettata per iniettare fallimenti tecnici temporanei nell'infrastruttura di pagamento della mensa, simulando guasti alla linea dati o ai terminali POS.

### 15.1 Il Protocollo di Stop Passivo

Per simulare un guasto senza interrompere bruscamente i processi cassiere o causare corruzioni di memoria, viene utilizzato un **Semaforo di Gate** (`STATION_SEM_STOP_GATE`):

1.  **Innesco**: L'utility incrementa il valore del semaforo tramite `release_sem` (portandolo a 1).
2.  **Blocco Reattivo**: Ogni Cassiere, prima di iniziare il calcolo di un nuovo scontrino, esegue una `wait_for_zero` su questo specifico semaforo. Se il valore è `> 0`, il cassiere entra in stato di attesa passiva, sospendendo il servizio.
3.  **Ripristino**: Trascorsa la durata del guasto prevista dalla configurazione, l'utility riporta il semaforo a zero (`reserve_sem`), sbloccando istantaneamente tutti i cassieri in attesa e ripristinando la normale operatività.

### 15.2 Connettività Indipendente (Dynamic Attachment)

Questo programma dimostra la flessibilità dell'architettura IPC System V:

- **Rintracciamento Risorse**: Utilizza `ftok` sulla chiave di progetto per individuare la `shmid` attiva nel kernel, permettendogli di agganciarsi alla simulazione anche se lanciato manualmente da un terminale separato.
- **Isolamento**: Nonostante agisca sulla Memoria Condivisa, l'utility interagisce solo con i semafori di controllo, garantendo che i dati sensibili delle transazioni non vengano alterati.

### 15.3 Impatto sulla Simulazione

L'intervento del Disorder Manager ha effetti a catena su tutto l'ecosistema:

- **Pressione sulle Code**: Mentre le casse sono bloccate, gli utenti si accumulano nel Meeting Point, mettendo alla prova le soglie di pazienza configurate.
- **Stress Test delle Statistiche**: Il ritardo accumulato permette di verificare la precisione dei KPI relativi ai tempi medi di attesa e alla consistenza del servizio stazione per stazione.

## 16. Scalabilità Dinamica: Add Users (add_users.h / .c)

L'utility `add_users` permette di aumentare la popolazione della mensa in tempo reale, simulando l'arrivo di nuovi scaglioni di studenti o dipendenti senza dover riavviare la simulazione.

### 16.1 Protocollo di Handshake con il Master

L'inserimento di nuovi processi segue un protocollo di sicurezza coordinato per evitare interferenze con la sincronizzazione delle barriere:

1.  **Segnalazione**: L'utility invia un segnale `SIGUSR1` al Master e deposita la configurazione del nuovo carico nella coda di controllo.
2.  **Autorizzazione**: Il Master riceve la richiesta e attende il primo momento di "quiete" del sistema (tipicamente la fine del ciclo di servizio). In quel momento, rilascia il semaforo `MUTEX_ADD_USERS_PERMISSION`.
3.  **Spawn**: L'utility riceve il permesso, individua gli slot liberi nel pool di sincronizzazione (`group_pool`) e lancia i nuovi processi `utente` tramite `fork` ed `exec`.

### 16.2 Riciclo delle Risorse (Slot Management)

Per supportare l'aggiunta infinita di utenti in un pool di memoria condivisa finito, il sistema implementa una logica di **Scavenging**:

- L'utility scansiona la SHM per trovare gruppi con `active_members == 0`.
- Questi slot vengono resettati e riutilizzati per i nuovi utenti entranti.
- Tale approccio garantisce che la simulazione possa scalare dinamicamente entro i limiti fisici configurati (`group_pool_size`), massimizzando l'efficienza della memoria.

### 16.3 Allineamento del Registro Utenti

Ogni processo generato dall'utility esterna viene registrato atomicamente nel `user_registry` globale. Questo passaggio è fondamentale perché permette al Master di:

- Tracciare correttamente i PID per la gestione degli zombie.
- Includere i nuovi utenti nei calcoli del throughput e degli incassi giornalieri.
- Gestire la terminazione collettiva in modo che anche gli utenti aggiunti a metà simulazione ricevano i segnali di cleanup finale.
