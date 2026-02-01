# Analisi Teorica e Progettuale dei Requisiti

Questo documento descrive in dettaglio le scelte architetturali, le strutture dati e le logiche implementative utilizzate per soddisfare i requisiti del progetto "Oasi del Golfo".

---

## 1. Processi: Responsabile Mensa (Master)

- **Logica**: Il Master agisce come orchestratore centrale (Pattern Facade). Gestisce il ciclo di vita della simulazione: bootstrap, loop giornaliero coordinato e cleanup finale.
- **Strutture**: Utilizza tabelle di PID (`user_registry`) per monitorare i figli e strutture di configurazione caricate all'avvio.
- **Implementazione**: Implementa un handler `SIGCHLD` avanzato per la "Auto-Riparazione" delle barriere (Self-Healing) in caso di morte prematura dei figli.

## 2. Processi: Operatori di Distribuzione

- **Logica**: Architettura a 3 loop (Settimana, Giorno, Turno). Lavorano per "Competenza di Postazione": solo se acquisiscono un semaforo possono servire.
- **Tempo**: Il tempo di servizio è stocastico (±50/80%). Viene calcolato usando `generate_random_integer` e simulato con `nanosleep` robusto ai segnali.

## 3. Processi: Operatore Cassa (Cassiere)

- **Logica**: Simile agli operatori, ma con logica di calcolo monetario. Interagisce con la coda messaggi filtrando per `MSG_TYPE_ORDER`.
- **Prezzi**: I prezzi sono letti dinamicamente dalla SHM (`shm->configuration.prices`), rendendo il sistema flessibile a modifiche del menu senza ricompilazione.

## 4. Processo Utente

- **Logica**: Macchina a Stati Finiti (FSM). Ogni stato rappresenta una fase (Ticket -> Scelta -> Pagamento -> Consumo).
- **Integrazione**: Coordina l'accesso a 4 diverse code di messaggi e molteplici set di semafori durante il suo percorso.

## 5. Memoria Condivisa (SHM)

- **Logica**: Singolo segmento di memoria per minimizzare l'overhead di context-switch. Organizzata per contenere strutture annidate.
- **Flessibilità**: Utilizza il concetto di _Flexible Array Member_ per la gestione dei gruppi di utenti, permettendo di allocare in SHM solo lo spazio effettivamente necessario basato sulla configurazione.

## 6. Semafori (No Polling)

- **Logica**: Utilizzati per Mutu Esclusione (Mutex), Sincronizzazione (Barriere) e Controllo Risorse (Counting Semaphores).
- **Affidabilità**: Ogni operazione utilizza il flag `SEM_UNDO` (tranne che nelle barriere fisse) per garantire che il kernel liberi le risorse se un processo crasha mentre detiene un lock.

## 7. Code di Messaggi (MQ)

- **Logica**: Modello Richiesta-Risposta. L'utente invia con `type = MSG_TYPE_ORDER`, l'operatore risponde con `type = PID_UTENTE`.
- **Ottimizzazione**: Viene effettuato il tuning del parametro `msg_qbytes` a 64KB per evitare blocchi della simulazione sotto carichi elevati (Burst Mode).

## 8. Refill Dinamico delle Porzioni

- **Logica**: Task asincrona gestita dal Master. Utilizza un timer POSIX o un ciclo di controllo nel `simulation_engine`.
- **Sincronizzazione**: Protetta dal semaforo `STATION_SEM_REFILL_GATE`. Quando il Master rifornisce, gli operatori si fermano per mantenere la coerenza dei dati.

## 9. Gestione Pause e Presidio Stazioni

- **Logica**: Decisione atomica. Prima di lasciare la postazione, l'operatore consulta il valore del semaforo delle postazioni libere (`get_sem_val`).
- **Vincolo**: Se il numero di operatori attivi è pari a 1, la pausa viene negata per garantire che nessuna stazione rimanga senza presidio.

## 10. Logica del Piatto di Ripiego

- **Logica**: Algoritmo di scansione lineare. Se il piatto scelto è a `0` porzioni, l'utente itera sull'array `portions` in SHM per trovare il primo indice disponibile dello stesso tipo.
- **Fallback**: Se tutta la stazione è esaurita, l'utente passa alla fase successiva (o abbandona se è la sua ultima spiaggia).

## 11. Social Seating (Sincronizzazione Gruppi)

- **Logica**: Pattern Leader-Follower. Il gruppo condivide un indice SHM (`GroupStatus`).
- **Sincronizzazione**: Utilizzano barriere semaforiche private. Il Leader prenota il tavolo e alza il "gate" per permettere agli altri di sedersi simultaneamente.

## 12. Gestione del Tempo (Tick Temporale)

- **Logica**: Fattore di scala. Tutte le funzioni di tempo utilizzano un moltiplicatore `NNANOSECS`. Un ritardo di "10 minuti" simulati viene tradotto in un tempo reale calcolato basandosi sul tick configurato.

## 13. Raccolta Statistiche e KPI

- **Logica**: Accumulo atomico. Le statistiche sono divise in `daily` (reset ogni giorno) e `total` (permanenti).
- **Protezione**: Utilizzo di un Mutex dedicato (`MUTEX_SIMULATION_STATS`) per evitare race conditions tra centinaia di processi utenti che scrivono simultaneamente.

## 14. Communication Disorder (Guasto Cassa)

- **Logica**: Gate Passivo. L'utility esterna incrementa un semaforo di stop.
- **Reattività**: I cassieri eseguono una `wait_for_zero` prima di ogni transazione. Finché l'utility non riporta il semaforo a zero, i pagamenti sono congelati.

## 15. Add Users (Aggiunta Dinamica)

- **Logica**: Handshake via segnale. L'utility invia `SIGUSR1` al Master. Il Master autorizza tramite un semaforo di permesso solo quando il sistema è in uno stato stabile (es. fine giornata).
- **Scavenging**: L'utility riutilizza gli slot SHM dei gruppi che hanno terminato, rendendo il sistema scalabile.

## 16. Esportazione Statistiche in CSV

- **Logica**: Serializzazione dei dati. Al termine della simulazione, il Master itera sulle strutture statistiche e scrive le righe in un file `.csv` usando `fprintf`.

## 17. Gestione Ticket Sconto (80/20)

- **Logica**: Distribuzione probabilistica basata sul PID. Gli utenti con ticket devono acquisire un semaforo aggiuntivo (`semaphore_ticket_id`) che rappresenta la postazione di lettura/validazione.
- **Economia**: Il cassiere verifica la flag `ticket_is_validated` per applicare lo sconto del 50% al totale.

## 18. Build System (Makefile)

- **Logica**: Compilazione modulare. Ogni programma (`utente`, `operatore`, `master`) viene compilato separatamente.
- **Sicurezza**: L'uso di `-Werror` e `-Wvla` garantisce che il codice sia privo di warning e non utilizzi allocazione dinamica illegale nello standard richiesto.

## 19. Cleanup Totale delle Risorse

- **Logica**: Gestione dei segnali di terminazione. All'intercettazione di `SIGINT` o alla fine della simulazione, il Master invia un broadcast di terminazione e invoca `shmctl`, `semctl` e `msgctl` con `IPC_RMID`.

## 20. Modularità e Architettura Exec

- **Logica**: Disaccoppiamento forte. I processi comunicano solo tramite IPC. I parametri necessari (ID SHM, indici, etc.) vengono passati come stringhe nella `argv` durante la `exec`, rendendo ogni processo indipendente e testabile singolarmente.
