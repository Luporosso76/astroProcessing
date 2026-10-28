# M1.a pocketfft: followup PREP su cleanup e directory fsync

Data: 2026-10-25. Modello dichiarato dal runtime: `openai/gpt-6.1-sol`.
Nessuna delega. Costi/token non esposti, non stimati.

## Stato effettivo

Ricevuti due finding della review indipendente: ALTO sul killpg successivo al
reap del leader, MEDIO sulle barriere directory mancanti prima della retention.
Preparata la correzione dei sorgenti effettivi e 20 preregressioni, NON eseguite.
Non READY BUILD: nuova review e primo turno preregressioni ancora necessari.
Nessun finding dichiarato chiuso per auto-approvazione o prova runtime.

Il turno runtime corrente resta a Ftiny. Non avviati compilatori, version probe,
processi dummy, worker, FFT, fixture producer, preregressioni, test o benchmark.
Nessun import di moduli propri, rete, installazione, Git, indici o delega.
Eseguiti soltanto snippet Python stdlib per snapshot, hash, diff testuale e AST.
Nessun sorgente di prodotto o input di altri autori modificato.

## Snapshot esclusivo PRIMA delle modifiche

Percorso: `tmp/m1/fft-pocket/snapshots/pre-followup-001/`.
Otto copie byte-identiche della prima consegna, più `snapshot-manifest.json`.
Creazione con mkdir/open esclusivi, senza overwrite. Manifest con percorso
originale, byte, righe, SHA-256 e mode di origine/copia. Verificati 8/8 hash e
mode dopo le modifiche. I sorgenti originali avevano mode 0664, i JSON 0600;
copie rispettivamente 0444 e 0400, directory snapshot 0500. Barriere fsync di
file e directory applicate anche alla creazione dello snapshot. Questi mode
proteggono da scritture accidentali, non provano immutabilità contro root/owner.

SHA-256 del manifest snapshot:
`f9b268088f68fb96c8f9c0ee51c78fb5f3c22fbc1652734d750b66cf4929e700`.

Incluso integralmente il report originale, 148 righe, 8553 byte, SHA-256
`4b0eaf97c242fb1420462c81fbb8f1841b518bd945a836e983df76df00d92bcc`.
L'originale in `docs/evidence/M1-a-pocketfft-preparation.md` è rimasto invariato.
Manifest e due record statici della prima consegna restano invariati e storici;
non attestano la versione corretta del supervisore, worker e piano.

## Correzioni proposte sui file reali

`supervisor.py`: eliminati poll/wait Popen e killpg incondizionale dopo reap.
`OwnedGroup` osserva il child con waitid WNOWAIT, valida PID/sessione/gruppo e
mantiene il leader non reaped fino al termine dei segnali. SIGCHLD non default
blocca il lancio. Un leader già reaped o identità persa impedisce ogni killpg.
Il reap avviene dopo i segnali; la lease viene invalidata prima del reap anche
se quel syscall fallisce. Nessun segnale successivo al reap.

Census del gruppo OWN, reap, chiusura selector e chiusura di ogni pipe sono
tentativi indipendenti, con errori raccolti. Errori di census non saltano le
chiusure. Qualsiasi cleanup irrisolto blocca il prossimo lancio nella stessa
invocazione, impedisce un risultato valido e produce diagnosi STOP. Il ramo
non deve essere riavviato dal coordinatore senza risolvere l'incertezza. Non
si dichiara assenza di orphan quando il cleanup è irrisolto. Il census considera
membri vivi del gruppo OWN, non discendenti ostili che cambiano sessione.

`supervisor.py` e `worker.cpp`: mkdir e JSON sincronizzano directory pertinenti.
Il worker sincronizza staging file e directory, poi link e directory PRIMA
dell'unlink staging, infine directory dopo unlink; anche il cleanup tenta la
barriera directory e diagnostica un suo fallimento. Il supervisore sincronizza
log file, directory caso e directory log PRIMA dell'erase; risincronizza la
directory caso dopo l'erase. Una barriera pre-erase fallita impedisce gli unlink.
Una barriera post-erase fallita produce STOP, non complete. Input procedurali,
modifiche per fault e marker di collisione hanno barriere file/directory.

Sono ordinamenti di syscall preparati, NON una prova di power loss, persistenza
fisica o correttezza sotto ogni filesystem/storage. Le condizioni di errore non
abilitano fallback, adozione parziale o prosecuzione con qualità ridotta.

`regressions.py`: 20 metodi PLANNED con stdlib unittest/mock. Leader OWN vivo e
zombie riservato; identity-lost/reaped/session mismatch; nessun segnale dopo
reap; errori signal/census/reap; chiusure indipendenti; next-launch bloccato;
SIGCHLD ignorato; run senza poll/wait; ordine e fallimenti delle barriere prima
e dopo erase; JSON/mkdir; ordine pubblicazione C++ e tre omissioni discriminate
testualmente. Nessun processo reale, dummy o compilatore è previsto da questi
test. I mock non dimostrano da soli il comportamento di processi reali.

Il runner futuro verifica i pin prima di importare il solo supervisore di questa
unità, mai sorgenti batch attivi o helper di altre unità. Non usati helper esterni.
Solo un successo effettivo dei 20 metodi può creare la ricevuta complete corrente;
la build richiede quella ricevuta, con manifest e risultati hash-identici.
Nessuna ricevuta di preregressione esiste ora.

## Contratto numerico e licenze invariati

192 casi finiti, 14 errori base, 24 non finiti e 2 collisioni: 232 PLANNED,
zero eseguiti. Tol ZERO assoluta/relativa, forward/inverse, GT analitico
indipendente e tutte le componenti/bin invariati. Verificata uguaglianza AST
delle sette funzioni scientifiche header/spatial/spectrum/rational/compare/
arguments/cases rispetto allo snapshot. Ricetta, flag e oracle del nuovo
manifest sono identici ai rispettivi oggetti del manifest originario.

Adapter invariato, 93 righe. Contratto c2c, dtype/layout e ABI del worker invariati;
solo la CLI di build del supervisore richiede ora anche il namespace della
preregressione. Nessun epsilon retuning, cambio backend o riduzione di copertura.
Vecchi risultati 001/002 e confronto FFTW 3.3.10 non letti né modificati.

Header upstream 121769 byte, SHA-256
`518a95c552aeaee352aa30122137cd9e135d23c91e2863d9272aaa758744c86d`;
LICENSE 1498 byte, SHA-256
`a85ca13fdf90160b64a0698215868c13b74d835ad0a4e2ba44713b8c5058a056`.
Entrambi ricontrollati e immutati; notices conservate. Nessuna nuova lettura
semantica integrale dell'header o acquisizione di sorgenti FFTW/Siril.

L'upstream usa internamente viste `detail::cmplx<T>`: il profilo GCC con
`-fno-strict-aliasing` e oggetti `std::complex<T>` reali non certifica assenza
di UB/lifetime problematico secondo ISO C++ generale. Il limite resta esplicito.
Runtime effettivi e relative licenze ancora da inventariare; licenza del prodotto
riservata al proprietario, nessuna scelta irreversibile compiuta.

## Artefatti attuali e verifiche reali

| File | Righe | Byte | SHA-256 |
|---|---:|---:|---|
| `tmp/m1/fft-pocket/supervisor.py` | 554 | 24705 | `90c702906876ee759e6a856d24d857ec4411cfe52fe28cbe31f8cef8ca53144b` |
| `tmp/m1/fft-pocket/worker.cpp` | 140 | 5903 | `2dad8bc0c5e3beb07cfd09e265ae784742d2dfc506483825da1a20d7f4608fe8` |
| `tmp/m1/fft-pocket/regressions.py` | 291 | 14139 | `6639e2def2f2a767207572477d68af02df5f40dafd4ee4346e510e5796009504` |
| `plans/M1-a-pocketfft.md` | 250 | 15136 | `6f55cc53598017684ee26dc2b4d0dfcd38bd030de796f4d3dc4c767b03c348dd` |
| `tmp/m1/fft-pocket/followup-manifest-001.json` | 155 | 4202 | `b2b495a745d5bc4165aad62b521f4c6c541ab2480fedd9b7b745c3d371927296` |
| `tmp/m1/fft-pocket/followup-source-diff-001.patch` | 853 | 41617 | `d986e5ac05da30a21a175e2667cb97dcf71b6e333cc918291a0a4fd102d09cec` |
| `tmp/m1/fft-pocket/static-followup-001.json` | 110 | 3143 | `b6c46b04dab98eae3c463fccc069222e31f2f5c2e546bd6e0c93712ace77bc56` |

Diff unified effettivo, prodotto con difflib senza Git: tre file modificati
(supervisore, worker, piano), più il nuovo sorgente preregressioni. Il nuovo
manifest pinna cinque sorgenti, snapshot e diff prima di dati/esecuzioni.

Letture semantiche iniziali in questo turno: supervisore 397, worker 126, piano
205, report originale 148, manifest originale 129: 1005 righe. Gli altri tre file
della prima consegna sono stati letti per snapshot/hash/mode, non dichiarati come
nuova review semantica. I testi nuovi sono stati analizzati solo con AST/hash;
nessuna esecuzione delle funzioni preparate.

Comandi effettivi: `du -s -B1 tmp` (867721216 byte al preflight), snippet
`python3 -I -B -S - <<'PY'` per snapshot esclusivo, fsync, SHA-256, difflib e
`ast.parse` dei due sorgenti Python. Python 3.14.4, versione completa nel record.
Esito statico: sintassi AST valida, 20 metodi individuati senza eseguirli,
import dichiarati solo stdlib, nessuna chiamata poll/wait/waitpid nel supervisore,
pin upstream e snapshot verificati, funzioni scientifiche AST identiche.
C++ non verificato con compilatore, nemmeno syntax-only. Questi esiti statici
NON sono PASS delle preregressioni, della numerica o del gate di build.

## Primo turno futuro

Prima review del diff e assegnazione esplicita del turno dopo Ftiny. Primo comando,
NON ESEGUITO:

```sh
python3 -I -B -S tmp/m1/fft-pocket/regressions.py preregressions-001
```

Solo dopo esito reale e review di quelle preregressioni è ammesso il comando build
del piano aggiornato. Correttezza FFT, review numerica, inventario runtime e
adottabilità restano tutti aperti. Nessuna prestazione misurata o inferita.
