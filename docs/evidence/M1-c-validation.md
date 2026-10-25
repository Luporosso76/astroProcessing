# M1.c: prima validazione dinamica

Esito eseguito: **VALIDAZIONE PASS**, exit effettivo **0**, al primo tentativo.
SQLite: **44/44** casi PASS. JSON: **46/46** casi PASS. Nessun caso rimosso,
saltato o accettato come pubblicazione parziale. Non è una scelta del backend
né la chiusura delle misure comparative M1.c.

Il coordinatore ha comunicato la chiusura statica dei quattro finding nella
review security indipendente, sessione con prefisso `ses_e8c4fd1`, sui due SHA
riportati sotto. Questo record aggiunge l'esecuzione reale, non attribuisce
all'autore una nuova review indipendente.

## Prerequisito storage e comando effettivo

Eseguito dopo `STORAGE_PASS` documentato in [M1-storage](M1-storage.md).
Allocazione radicata prima del comando: **731 217 920 B**. Prenotazione richiesta:
**201 326 592 B** (192 MiB). Cap invariato: **1 073 741 824 B**. Il controllo di
manutenzione ha verificato anche 5 MiB aggiuntivi di margine; il supervisore c
ha poi eseguito il proprio preflight, senza modificarne soglie o ricetta.

Dalla radice `/opt/workspace/astroProcessor`:

```bash
/usr/bin/python3 -I -B tmp/m1-persistence/run.py --unit c --phase validate --backend both --series-id m1c-v2-01 --output tmp/m1-persistence/paired-series-v2-01
```

Ambiente esplicito del supervisore: `ROOT`, `TMPDIR`, cache/config XDG dentro il
workspace, `PYTHONDONTWRITEBYTECODE=1`, `PYTHONNOUSERSITE=1`,
`OPENSSL_CONF=/dev/null`, `LC_ALL=C.UTF-8`. Nessun HOME, profilo personale o
credenziale passato ai figli. SQLite e JSON sono candidati primari selezionati
prima dell'esecuzione; nessun backend sostitutivo.

Tempo esterno dell'intera validazione: **46 616 964 917 ns**. È un dato di
esecuzione con handshake e controlli, non un campione di benchmark. Stdout
termina con `status=PASS`, backend `both`, fase `validate`; stderr è vuoto.

## Versioni congelate e identità

- Runtime: Python `3.14.4`, GCC `15.2.0`, Linux `x86_64`.
- SQLite collegato: `3.46.1`.
- OpenSSL dichiarato da `ssl`: `OpenSSL 3.5.5 27 Jan 2026`; SHA provider `_hashlib`.
  Identità del collegamento fra questi provider non auditata indipendentemente.
- Modello dichiarato dal runtime dell'autore: `openai/gpt-6.1-sol`.
- Protocollo: `M1.c-isolated-v2`; serie: `m1c-v2-01`.
- Recipe SHA-256:
  `bc0a4d775349c7ed616b981f01fb8b5c3a54462e1e7c2f8e53401d891e8d47c4`.
- Protocol SHA-256:
  `c24daaf0b0a951cb5a12cb9821f0c2a8b49bba339ef3bc079963095fb505afff`.
- `persist.py`, immutato prima/dopo l'esecuzione:
  `aa1be7debaf126680e1b9b34c986f61e8dfbf450a6746b7d3b3bcb0afe127e67`.
- `run.py`, immutato prima/dopo l'esecuzione:
  `f54caff9c25685502ea035045a9cc91fe1735c32bc7a14bfe6c75f5a13f8fcb2`.

Tutti gli SHA dei sorgenti/protocollo elencati dal manifest sono stati ricontrollati
dopo l'esecuzione. Il README congelato resta lo snapshot di preparazione incluso
nella ricetta; non è stato riscritto per aggiornare retroattivamente la serie.

## Copertura effettiva

| Gruppo | SQLite | JSON |
| --- | ---: | ---: |
| Casi originali conservati ed eseguiti | 38 | 38 |
| Crash: 6 punti per 3 repliche | 18 | 18 |
| Regressioni capability/scratch symlink/manifest symlink/hardlink aggiunte | 4 | 4 |
| Regressioni fsync omessi aggiunte | 2 | 4 |
| Totale casi PASS | 44 | 46 |

I crash sono parte dei 38 casi originali, non casi aggiuntivi. I 36 figli
terminati a SIGKILL risultano exit `-9`; erano figli `Popen` propri, fermi alla
pipe nel punto richiesto. Nessun processo estraneo terminato. Prima di ACK si
recupera solo generazione 1 o 2 completa; dopo ACK esattamente la 2.

Il preflight rifiuta scratch readonly, scratch non-directory e prenotazione
insufficiente con categorie specifiche, zero eventi di accesso e zero chiamate
a entrambi i backend. Il caso readonly è una simulazione della policy sui bit
di modo, non una prova di EACCES OS. Le omissioni fsync producono
`DURABILITY_ORDER`, uscita non nulla e nessun ACK. JSON copre anche omissione
fsync manifest e directory progetto. SQLite FULL rimane distinto dai fsync
applicativi osservati: nessun fsync interno SQLite viene inventato.

Le regressioni verificano anche worker CLI senza capability, manifest capability
non coincidente, rifiuto chmod sul symlink e sentinel invariato. Nessuna pretesa
di sandbox OS. Corruzione del commit corrente non provoca recupero di un vecchio
blob, cambio formato/backend, noop o migrazione implicita. Schema futuro resta
read-only senza mutare store/sidecar. Migrazione sintetica riuscita e fallita,
concorrenza, short write ed ENOSPC simulato mantengono gli oracle originali.

**90 originali sintetici** sono stati rihashati dopo la validazione e conservano
contenuto e assenza di bit di scrittura. Dopo c risultano ancora corretti tutti
i **168 SHA** delle fixture a e invariati i **127 file** di continuità archiviata
verificati durante la manutenzione. Nessuna lettura dei sorgenti attivi
ownership/d/fault; nessuna misura comparativa e nessuna directory `measure/`.

## Tentativi, errori e risorse

Primo tentativo conservato integralmente sotto
`tmp/m1-maintenance/c-validation-attempt-001/`: `command.json`, `stdout.log`,
`stderr.log`, `exit.json`, `audit.json`. Nessun fallimento inatteso, nessuna
correzione dinamica, nessun secondo tentativo o cambio di serie necessario.
Gli errori intenzionali delle fixture sono esiti attesi nei casi PASS, non
fallimenti cancellati. Tutti i dettagli restano in `cases.json` e `cases.jsonl`.

Picco RSS massimo riportato fra i figli: **33 075 200 B**; massimo thread
osservato: **1**. Limite RSS best effort invariato a 512 MiB, buffer pianificati
entro 16 MiB e cap buffer a 256 MiB. Il report c fotografa 84 295 680 B nella
serie prima della scrittura finale dei report; la contabilità globale esterna
dopo il processo rileva **816 898 048 B**. Non si inventa un picco globale c che
il supervisore non ha registrato come metrica: rimangono i suoi guard applicativi
e la prenotazione precedente all'esecuzione.

## Artefatti verificati

Percorsi sotto `tmp/m1-persistence/paired-series-v2-01/`, SHA-256:

- `recovery.json`: `c6af73d6d8f233be352e210c431a91d139d7c9d5e24f6082e0b502961e17c16c`.
- `manifest.json`: `ae969d70d35af6e4d8accaeeca2aa30f43718490fd92ea57e9bf498252dec354`.
- `cases.json`: `846efd9a74c7fae3ed941dbf51a04665c15eac0613ad111607598456b80b72c5`.
- `cases.jsonl`: `8fade1ec5a1bebccfc842814f5920400f1f279a1761c7c98f6c1013fcdcd6ed0`.
- `artifacts.json`: `684e2a112bb18ab6923663ab4b8ef1c5ebc3320f61ca083281c6b17ec9feb732`.

Audit riproducibile, eseguito con exit 0:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1-maintenance/audit_validation.py
```

Lo script di audit è lungo 110 righe, AST verificato. SHA:
`57fccac12de748be1930b8561ec95debbedee4fde5bfd7b637a3804059271a67`.
Il suo output archiviato `c-validation-attempt-001/audit.json` ha SHA
`fd2a5f17743dbbe5f6544a6c62b1d1630697f534513110f06c9e6b0f4f7f9d1b`.
L'audit verifica gli artefatti effettivi, tutti i nomi/casi previsti, gli SHA e
la continuità; è dello stesso autore, non una review indipendente. Non rieseguirlo
nello stesso percorso: l'output `O_EXCL` conserva il record già scritto.

## Primo passo non chiuso e limiti

Restano da svolgere la review indipendente di questi risultati e, soltanto con
un nuovo incarico, le misure comparative M1.c. Nessun dato di questa validazione
viene riutilizzato come benchmark. Power-loss resta **NOT_RUN**; nessuna promessa
su altri filesystem, hardware, Linux L1/L2/L3 o schema di prodotto.

Alla fotografia dell'audit, `tmp/` occupa **817 033 216 B**. Una prenotazione
futura M1.d da 256 MiB richiederebbe **11 726 848 B** oltre il cap, ancora prima
di un ulteriore margine. Quel ramo deve fermarsi al suo preflight se la situazione
resta questa. Nessun cap aumentato, fixture cancellata o spill reso sparse.
La manutenzione ha scaldato la cache e cambiato il layout fisico delle fixture a:
le future misure dovranno dichiararlo, senza riscrivere i vecchi timing.
