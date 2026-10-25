# M1.f: preparazione soltanto

Stato: **PREPARED / NON ESEGUITO**. Verificata soltanto la sintassi dei sorgenti nuovi e i metadati tecnici indicati sotto. **Zero generazioni di dataset, zero import dei moduli preparati, zero worker, test di correttezza o benchmark.** Nessun PASS empirico di M1.f, nessuna decisione di hash/stack del prodotto.

Autore: GPT-6.1 Sol, OpenAI, dichiarato dal runtime; nessun subagente, costo o token inventato. Letti soltanto `plans/M1-spikes.md` righe 123-131 e guardie comuni 40-44; non riletto il piano di 4.5k righe. Nuove scritture esclusivamente in `tmp/m1/f-hash/` e in questo record. Namespace inizialmente assente: nessun file altrui sovrascritto. Ownership e vecchi report/sorgenti/serie/protocollo sotto review non modificati.

## Consegna sorgenti

- `protocol.py`: cinque dataset, seed/formula, oracle letterali, matrice e limiti congelati.
- `fixture.py`, `prepare_inputs.py`: producer esplicito futuro, manifest versione 1, contenuto originale write-once e oracle scalare byte-per-byte.
- `worker.py`: futuro snapshot RAM sealed e un solo content engine scelto prima della lettura; diagnostica e pubblicazione separate.
- `harness.py`: futuro supervisore di correttezza per una ricetta, preflight su tutto tmp, monitor best effort, nessuna fase misura.
- `compare_results.py`: confronto futuro di due risultati già presenti, senza eseguire un altro engine.
- `README.md`, `env.sh`: contratto, limiti, comandi futuri espliciti, cache/temporanei nel namespace sotto root/tmp.

Manifest corrente: `tmp/m1/f-hash/preparation-manifest-002.json`, SHA-256 **`88f3eebe229646aaa0d8e999bc65e1ed18f962d257edbcb3d1d16c7edb9ec033`**. Registra hash degli otto file sorgente/README/ambiente, linee e AST dei sei moduli Python, conteggi pianificati e conteggi eseguiti zero. Output in `preparation-check-002.{stdout,stderr}`. La prima ricevuta `preparation-manifest.json` resta conservata con hash `df6e9000d616402bbb178b7cf27ffb46c9a5052384c8b81c1f3405267566c1a7`: prima della consegna è stata chiarita la distinzione fra modulo adapter Python e provider OpenSSL interno, senza alcuna esecuzione da rietichettare. I digest del manifest sono metadati dei piccoli sorgenti, non risultati su fixture SHA-256.

## Controlli realmente eseguiti

Metadati: `/usr/bin/python3 --version`, `command -v openssl`, `OPENSSL_CONF=/dev/null openssl version`; **Python 3.14.4**, **OpenSSL CLI 3.5.5**, eseguibile `/usr/bin/openssl`. Nessun compilatore necessario: solo stdlib Python e CLI già installata.

`openssl dgst -help` consultato come metadato: opzioni `-binary`, `-provider`, `-propquery` presenti. Letti soltanto i nomi/valori delle dichiarazioni F_ADD_SEALS/F_GET_SEALS/F_SEAL_* in `/usr/include/linux/fcntl.h`. **Provider hashlib e supporto kernel/policy memfd-seals NON importati/provati.** Non basta la presenza degli header per qualificare lo snapshot.

Controllo statico: `ast.parse` su **6/6 moduli valido**, senza exec/import del codice; estratti con `ast.literal_eval` solo i letterali della matrice, non invocata `recipes()`. `bash --noprofile --norc -n` su `env.sh`: exit **0**, script non eseguito. Nessuna verifica runtime della correttezza degli harness deriva da questi esiti.

Comandi ripetibili del controllo sintattico, senza avviare gli esperimenti:

```bash
env TMPDIR=/opt/workspace/astroProcessor/tmp PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -B -c 'import ast; from pathlib import Path; files=sorted(Path("tmp/m1/f-hash").glob("*.py")); assert len(files)==6; [ast.parse(p.read_text(), filename=str(p)) for p in files]; print("AST valid: 6 modules; no prepared module imported")'
env -u BASH_ENV /bin/bash --noprofile --norc -n tmp/m1/f-hash/env.sh
```

Non è stato eseguito alcun comando producer, worker, harness o confronto riportato nel README.

## Dati, GT e ricette future

Cinque dataset dichiarati: empty, `abc`, **4097 byte**, **16 MiB**, **128 MiB**. Per i pattern: `byte[i]=(73*i+19) mod 256`, indice zero-based, seed **19**, interi esatti; prefisso letterale e oracle scalare distinti dalla generazione per periodo. Nessuna fotografia o dato privato.

Oracle SHA-256 di empty/abc sono letterali standard già nel piano, non calcolati da un candidato. I digest dei pattern grandi restano **SCONOSCIUTI**. Agreement hashlib/CLI non significa due implementazioni crittografiche indipendenti: possono usare lo stesso OpenSSL.

Pianificati, NON eseguiti: **5 fasi producer**, **38 ricette** (18 clean, 16 errori attesi, 4 sealed-write/reopen positivi), **11 confronti di agreement** sui positivi. Originali condivisi read-only per caso, senza duplicare 128 MiB per ogni engine/chunk. Errori e working copy restano sul caso piccolo 4097 byte.

Engine richiesto prima del caso: hashlib/adapter Python `_hashlib` oppure OpenSSL CLI/provider default, con versioni attese esplicite. Adapter/API diverso o engine fallito deve fermare il caso: nessun passaggio automatico all'altro programma. `_hashlib` non identifica il provider crittografico OpenSSL interno, marcato NOT_INTROSPECTED; anche il modulo Python atteso è ancora da verificare in esecuzione. La dimensione dei chunk interni OpenSSL CLI non è controllabile da questo harness: non la si presenta come parametro del suo loop di hash.

## Contratto futuro di consistenza e pubblicazione

Size/mtime/stat coerenti prima/dopo **non sono prova di snapshot**. La strategia preparata verifica tutto il contenuto della versione sintetica trusted, copia quegli stessi byte in un memfd e impone F_SEAL_WRITE/GROW/SHRINK/SEAL prima dell'hash. Il digest legge soltanto il nuovo fd O_RDONLY dello snapshot sealed. Il memfd è RAM gestita anonima, non un temporaneo filesystem esterno; tutti gli artefatti nominati restano root/tmp. Seal/API/policy non disponibili: NOT RUN, nessun backend sostitutivo.

Il test preparato tenta pwrite sul fd originario ancora RW e richiede EPERM; non confonde la sola apertura read-only con immutabilità. Ogni reopen ricrea e verifica lo snapshot: nessun riuso di digest basato su mtime. `reopen-mutated` deve rifiutare il nuovo contenuto e non pubblicare il primo digest tenuto in memoria. Troncamento, trailing, dichiarazione/versione errata, mutazione con mtime ripristinato, read error e guasto dell'engine hanno diagnostiche specifiche e nessun result.json; i contatori dell'altro content engine devono restare zero.

Campi distinti: digest contenuto, riferimento/istanza/versione del file sorgente, ricetta canonica e cache key con schema/serializer/backend/versioni/snapshot identity. **Cache lookup NON IMPLEMENTATA.** Il digest non attesta validità corrente del pathname dopo la cattura. Writer esterni ostili, ABA arbitrario e snapshot di contenuti sconosciuti non sono supportati da questo spike; non si spacciano interleaving deterministico e oracle del pattern per uno stress concorrente multi-processo. Pubblicazione atomica no-replace preparata; durabilità power-loss e recovery non provate, restano distinte da M1.c.

## Budget e ciò che manca

Preflight preparazione: tmp **815398912 byte**, riserva nuova **2 MiB**, headroom **258342912 byte**. Primo controllo statico: tmp **801153024 byte**; seconda ricevuta **852877312 byte**. Nessun dato cancellato/spostato da questo agente, totale condiviso variabile. Directory `datasets/` e `runs/` assenti ai controlli: nessun payload generato.

Guardie future: **256 MiB** di RAM gestita includendo il memfd, **512 MiB RSS best effort**, scratch di **tutto tmp <=1 GiB**, preflight prima di generazione/worker e stop senza ridurre il caso. Il supervisore osserva il PID worker; non si dichiara copertura completa dei picchi di ogni subprocess/allocazione interna OpenSSL. Buffering, guardie e riserve sono ancora sorgenti non provati, non misure di consumo.

**NON IMPLEMENTATI:** fase di misura, tre processi persistenti p con due warm-up e sette round, pairing p/r, bootstrap gerarchico 2000 seed4059, cache lookup e stress con writer realmente paralleli. Nessun CI, MB/s, costo hash o vincitore inventato. Un processo per correttezza non soddisfa il protocollo statistico M1.

Primo passo successivo: review del contratto/sorgenti preparati; nel futuro turno di esecuzione seriale autorizzato, iniziare da producer empty/abc e oracle letterali, poi piccoli guasti, senza generare subito 128 MiB. **In questo incarico non si avvia nulla mentre M1.c può validare.** M1.f e il gate M1 restano aperti. Nessuna rete, installazione globale, Git, licenza prodotto o indice creato.
