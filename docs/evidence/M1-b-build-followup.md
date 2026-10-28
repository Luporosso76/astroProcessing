# M1.b: candidato004 dopo FIRSTFAIL build003

Stato: **PREP ONLY, build004 NON ESEGUITA**. Questo turno corregge soltanto
il nome dell'helper di logging e la disposizione delle istruzioni segnalata
dal warning. Lo slot runtime appartiene a F. Nessuna compilazione, import di
candidato, GUI, oracle, suite di regressione o benchmark eseguiti.
Autore: modello dichiarato dal runtime `openai/gpt-6.1-sol`.

## Fonte e snapshot prima della modifica

Letti `qt_viewer.cpp` (408 righe), `run.py` (381), manifest003 (28) e
`docs/evidence/M1-b-build-oracle.md` (183). Il report del fallimento rimane
immutato. Non letto il piano di 4482 righe. Preregistrazione propria e
protocollo padre sono stati letti soltanto come byte per hash, mode e snapshot.

Prima della patch, verificati i 19 hash del manifest003:
`47c2bade1f71369d582891a058fa344b6eb992962e9fa7742763bb833ecca041`.
Snapshot: `tmp/m1/b-viewer/snapshots/before-build-followup-004/sources-003.zip`.
SHA-256: `a826de6cedeaae640bb6bea1accb212d02b4dfe1c03e3ff11297bba0cf6fee37`.
Il ZIP conserva 23 file: le 19 fonti pinnate, manifest/check/diff003 e report183.
Ogni membro conserva byte e mode Unix. Il receipt registra SHA, byte, righe e
mode; SHA del receipt:
`f11da64e8411414190db77af5abd984ef281cc5544ed986d365d719350287092`.

Anche dopo la patch, i 19 pin003 sono verificabili dal ZIP contro manifest003.
I 19 pin004 descrivono le fonti correnti: 18 hash invariati e il solo
`qt_viewer.cpp` aggiornato. Tutti i mode delle fonti restano invariati.

Un inventario PRE/POST verifica hash e mode di 156 voci protette, comprese
directory e file di build003, runs, execution-turn003, snapshot precedenti,
ricevute, manifest e report storici. Gli output sono stati aperti soltanto in
lettura: nessun chmod, sovrascrittura, cancellazione o adozione come output004.
La DSO parziale003 resta SHA-256
`7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`;
non è stata caricata. FIRSTFAILSTOP003 resta la prova del precedente exit 2.

Protocollo padre invariato: 169 righe, SHA-256
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
Nessuna modifica a piani, progress, altre unità o servizi di sistema.

## Correzione circoscritta

Unico file sorgente modificato: `tmp/m1/b-viewer/qt_viewer.cpp`.

- Riga 46: helper libero `event(QJsonObject)` rinominato `logEvent(QJsonObject)`.
- Tutte le 12 chiamate di logging usano lo stesso nome. Comprendono admit146,
  request158, cancel172, receive213/219, painted227 ed exportViewport252.
- Righe 177-183: stesso blocco lock/wait/stop/job/waiting, con istruzioni su
  righe distinte. La vecchia riga178 non confonde più visivamente l'assegnazione
  successiva con il ramo `if (stop_) return`.

Il rename evita il lookup dell'ereditato `QObject::event(QEvent*)` nelle chiamate
JSON. Non introduce override né altera eventi Qt, segnali o callback.
I payload JSON, compreso il campo `event`, restano identici.

La verifica statica ricostruisce esattamente il file nuovo applicando al vecchio
solo il rename e quel blocco di whitespace. Dopo rename inverso e normalizzazione
whitespace, tutto il testo coincide. Non è una prova di compilazione C++.

Kernel/dataset, numerica, cache, cancellazione, ABI2, pin memfd, ICC/export,
maschere e gate non cambiano. In particolare core.cpp/core_api.h, oracle.py,
native.py, run_io.*, session.py e run.py conservano i loro hash003.
Nessuna correzione ulteriore o suite speculative è stata aggiunta.

## Binding004 e comando futuro

Il runner effettivo **non contiene un manifest003 hardcoded**: `sources()`
calcola gli hash dei file correnti e aggiunge preregistrazione/protocollo;
provenance build e fase oracle confrontano quella stessa mappa. Non legge un
manifest scelto cercando il più recente. Per questo `run.py` non è stato cambiato
e non sono stati inventati flag per scegliere la revisione.

Il binding dichiarato di questa consegna è `source-hashes-004.json`, con tutti
i 19 percorsi. Prima di una futura esecuzione il coordinatore deve verificarne
gli hash correnti, poi usare esplicitamente build-id/run-id004. Il comando da
radice progetto, **solo dopo review indipendente e nuova ammissione**, è:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-004 --run-id build-004 --exclusive-admitted
```

Solo dopo build completa riuscita, hash/provenance conformi e nuovo preflight,
la fase successiva prevista rimane quella supervisionata:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase oracle --build-id build-004 --run-id oracle-004 --exclusive-admitted
```

Questi comandi non sono stati eseguiti e questo record non li autorizza.
La futura verifica PRE/POST deve includere i 19 hash004 e il protocollo congelato.
HOME/TMP/XDG/cache devono restare privati nella radice di progetto. Prima del
lancio servono cap fisico reale e assenza degli output, senza rinomine automatiche.
Riserve/limiti del runner sono invariati: build96 MiB, oracle32 MiB, cap1 GiB,
RSS512 MiB best effort, AS768 MiB, file32 MiB, log8 MiB, deadline oracle600 s.
Non sono stati prenotati output o budget runtime mentre F occupa lo slot.

## Controlli svolti e artefatti

Eseguiti soltanto controlli di preparazione con Python stdlib:

- AST di 8 file Python: SYNTAX_OK, senza importare moduli del progetto.
- Parsing XML QRC e controllo dei riferimenti ai file.
- `bash --noprofile --norc -n`: 2 blocchi README e un blocco con i comandi004.
- Confronti byte/hash/mode PRE/POST e delta esatto del solo sorgente C++.

Runtime dei controlli: Python
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Il testo GCC della versione Python non indica una nuova esecuzione del compilatore.
C++/CMake/QML, oracle e 11 regressioni: **NON ESEGUITI**. Nessuna prova numerica
o dato GT consumato. Non campionata la quota globale: nessun altro namespace
letto, nessuna ammissione runtime dedotta dallo spazio libero filesystem.

Artefatti sotto `tmp/m1/b-viewer/`:

- `source-hashes-004.json`, 19 pin:
  `43c90d9ef791f423f6c43c2b8540ef978152996f8e10b998a9a202387f525f52`.
- `source-diff-004.patch`, solo qt_viewer.cpp, 125 righe:
  `63d153a8a278383d1e63f7b7c2e0cf5d44a248ac319a566728b9a636a7d4cbed`.
- `preparation-checks-004.json`, ricevuta dei controlli e comandi non eseguiti:
  `e09a19042ed647c9b33050110ef45764852865c927128c0c78ed410d05540941`.

Verificata l'assenza anche come symlink di `build-004/`, `runs/build-004/` e
`runs/oracle-004/`. Nessun nuovo output di build creato.

Consegna stabile per review indipendente sul diff effettivo. **Nessun PASS build**
finché una futura esecuzione autorizzata lo dimostra. M1.b resta aperta;
renderer antifallback INCOMPLETE, GTK non ammesso e gate GUI/presentazione/
interazioni/ICC/A11y/performance restano non verificati.
