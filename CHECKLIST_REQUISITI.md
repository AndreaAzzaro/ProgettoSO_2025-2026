# Checklist Requisiti Progetto: 'Oasi del Golfo'

Questa tabella riassume i requisiti estratti da `consegna.md` per monitorare lo stato di avanzamento dello sviluppo e della documentazione.

| ID     | Categoria       | Requisito / Funzionalità            | Descrizione Tecnica                                                | Stato |
| :----- | :-------------- | :---------------------------------- | :----------------------------------------------------------------- | :---: |
| **1**  | **Processi**    | **Responsabile Mensa (Master)**     | Orchestratore: crea risorse IPC, lancia figli, stampa statistiche. |  ✅   |
| **2**  | **Processi**    | **Operatori (Primi/Secondi/Caffè)** | Distribuzione cibo con tempi casuali (±50/80%).                    |  ✅   |
| **3**  | **Processi**    | **Operatore Cassa (Cassiere)**      | Gestione pagamenti (±20% tempo) e calcolo incassi.                 |  ✅   |
| **4**  | **Processi**    | **Processo Utente**                 | Ciclo: Menu -> Code -> Pagamento -> Consumo -> Uscita.             |  ✅   |
| **5**  | **IPC**         | **Memoria Condivisa (SHM)**         | Utilizzo di SHM per lo stato globale e le statistiche.             |  ✅   |
| **6**  | **IPC**         | **Semafori (No Polling)**           | Sincronizzazione ed esclusione mutua senza attesa attiva.          |  ✅   |
| **7**  | **IPC**         | **Code di Messaggi (MQ)**           | Comunicazione ordini/pagamenti tra Utenti e Operatori.             |  ✅   |
| **8**  | **Logica**      | **Refill Dinamico**                 | Rifornimento porzioni ogni 10 minuti simulati.                     |  ✅   |
| **9**  | **Logica**      | **Gestione Pause (Presidio)**       | Pause casuali garantendo almeno 1 operatore attivo.                |  ✅   |
| **10** | **Logica**      | **Piatto di Ripiego**               | L'utente sceglie un'alternativa se il piatto è esaurito.           |  ✅   |
| **11** | **Logica**      | **Social Seating (Gruppi)**         | Utenti in gruppi (1-MAX) che pagano e mangiano insieme.            |  ✅   |
| **12** | **Simulazione** | **Tick Temporale**                  | 1 minuto simulato = `NNANOSECS` nanosecondi reali.                 |  ✅   |
| **13** | **Statistiche** | **KPI Giornalieri e Totali**        | Serviti, piatti distribuiti/avanzati, attese, ricavi.              |  ✅   |
| **14** | **Completa**    | **Communication Disorder**          | Utility esterna per bloccare le casse (Gate semaforico).           |  ✅   |
| **15** | **Completa**    | **Add Users (Hot-plug)**            | Utility per aggiungere utenti a simulazione in corso.              |  ✅   |
| **16** | **Completa**    | **Report Esportazione CSV**         | Salvataggio di tutte le statistiche in formato `.csv`.             |  ✅   |
| **17** | **Completa**    | **Gestione Ticket (80/20)**         | Utenti con sconto e coda validazione ticket dedicata.              |  ✅   |
| **18** | **Standard**    | **Makefile & Build**                | Compilazione con flag `-Wvla -Wextra -Werror`.                     |  ✅   |
| **19** | **Standard**    | **Cleanup IPC**                     | Deallocazione sistematica di tutte le risorse al termine.          |  ✅   |
| **20** | **Standard**    | **Modularità (exec)**               | Lancio dei vari processi tramite eseguibili distinti.              |  ✅   |

---

### Cause di Terminazione Obbligatorie

- [x] **Timeout**: simulazione finita dopo `SIMDURATION` giorni.
- [x] **Overload**: troppi utenti in attesa a fine giornata (> `OVERLOADTHRESHOLD`).

### Configurazioni Richieste

- [x] `configtimeout.conf`: genera terminazione per tempo.
- [x] `configoverload.conf`: genera terminazione per troppa coda.
