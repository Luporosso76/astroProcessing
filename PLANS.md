# PLANS.md — convenzione degli ExecPlan

Regole per scrivere e chiudere i piani di lavoro. Lo stato vive in `docs/progress.md`, le prove in `docs/evidence/`, le decisioni in `docs/decisions/`. Qui sta solo il metodo.

## Unità e file

- Un file per unità attiva: `plans/<milestone>-<unit>.md` (esempio `plans/M0-M1.md`).
- Un ExecPlan copre una milestone o una sotto-milestone, non l'intero progetto.
- Le 54 unità restano quelle del piano §21: M1.a–f, M2.a–g, M3.a–s, M4.a–g, M6.a–i, M9.a–f. Non si duplicano in ogni piano.
- Nessuna data, nessuna durata stimata. Un piano dichiara lavoro e criteri, non calendario.

## Struttura minima

1. **Ambito**: unità, obiettivo, cosa resta fuori.
2. **Prerequisiti**: gate già chiusi, contratti e ADR necessari, dipendenze reali.
3. **Deliverable**: file, artefatti, documenti attesi.
4. **Ordine e gate**: sequenza topologica e criterio di uscita verificabile.
5. **Contratto scientifico**: modello, unità, ipotesi, casi degeneri, comportamento in errore, quando l'unità produce scienza.
6. **Test e misure**: dataset e fixture con ID, metrica preregistrata, tolleranza da derivare, numero di campioni, condizioni. Rimando a `docs/test-matrix.md`.
7. **Evidenza**: percorso del record, comandi, hash, versioni, ambiente, esito e limiti.
8. **Rollback e rischi**: come tornare indietro, cosa invalida il risultato.
9. **Chiusura**: stato del gate, alternative scartate, prossimo comando verificabile.

## Stati

`PLANNED` (scritto, non avviato), `IN PROGRESS`, `BLOCKED` (causa e rami indipendenti), `DONE` (gate verificato con evidenza). Un piano non è `DONE` perché il codice esiste: serve il gate.

## Regole di gate

- Un prerequisito incompleto tiene il gate aperto. Non si aggira con un pin, una deroga o un test più debole.
- Se un test fallisce, si corregge la causa e si ripete. Mai degradare l'algoritmo, cancellare una fixture, disabilitare una funzione o allentare la tolleranza per far passare il gate.
- Una funzione P0 mancante blocca la milestone. Una P1 non si promuove né si elimina in silenzio.
- Una decisione tecnica si prende dopo le evidenze e si registra in `docs/decisions/`. Non serve un voto del proprietario.
- Le prove pianificate restano `PLANNED`. Solo un record eseguito in `docs/evidence/` sostiene un `DONE`.
- Nessun fallback nel prodotto: un percorso alternativo non è una via di uscita, è un difetto (NC-21, NFR-FBK-01).

## Avvio, chiusura, ripresa

- All'avvio di una sessione: leggere `AGENTS.md`, `docs/progress.md`, questo file e il piano attivo; verificare lo stato reale prima di agire.
- Alla chiusura: aggiornare piano, `docs/progress.md` e matrice test; registrare evidenza, blocchi e primo passo successivo.
- Una milestone chiusa fa partire la successiva senza attendere autorizzazioni di routine.
- Un blocco circoscritto si isola; il lavoro indipendente continua.

## Piani attivi

- `plans/M0-M1.md`: chiusura M0 e spike M1.a–f. È il piano corrente.
