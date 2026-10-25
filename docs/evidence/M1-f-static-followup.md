# M1.f: followup statico 003

Stato: **correzioni candidate, review indipendente richiesta**. Soltanto AST/sintassi e verifica degli archivi/hash. Nessun import dei moduli propri, né invocazione di producer, worker, CLI OpenSSL, test o benchmark. SHA-256 usato soltanto sui piccoli artefatti/archivi per la preservazione. Nessun dataset generato; M1.f e M1 non dichiarati PASS.

Autore: GPT-6.1 Sol, OpenAI, dichiarato dal runtime; nessuna attestazione, delega o stima di costi/token. Letti i sorgenti precedenti effettivi: harness 109 righe, fixture 124, prepare_inputs 37, compare_results 41, worker 263, protocol 50; poi verifiche mirate del codice modificato. Non riletto il piano di 4.5k righe. Nessuna operazione Git, rete, installazione o licenza prodotto.

## Preservazione prima della patch

Snapshot byte-identico di **18 file**, inclusi tutti gli artefatti precedenti del namespace e il report di 61 righe:

- `tmp/m1/f-hash/history/prepatch-003.zip`, SHA-256 **`57596b8446b959f975ed364d778863eff8f9162a5c676e1913570e86ad15b89d`**.
- Manifest dei byte in `history/prepatch-003.json`; ogni entry dello ZIP ricontrollata tramite hash.
- `docs/evidence/M1-f-preparation.md` resta invariato: `71bbe078a32016e149ba06a5902efaebe6621771b4e07048e4c34d98331dd182`.
- Prima ricevuta `preparation-manifest.json` (001) e `preparation-manifest-002.json` preservate ai loro percorsi, senza rietichettarle.
- `plans/M1-spikes.md` invariato: **`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`**. Anche protocol.py resta byte-identico al prepatch: le 38 ricette non cambiano.

Preflight prima delle nuove scritture: tmp **853520384 byte**, riserva 2 MiB. Il solo namespace f-hash, verificato tramite descriptor/identità, è stato reso privato **0700**; nessun permesso di sistema o altro namespace modificato. Sorgenti e nuovi record restano esclusivamente nel perimetro autorizzato.

## Quattro rilievi: modifiche statiche

1. **Raccolta figli.** `harness.supervise` racchiude Popen e monitor in try/finally; `collect_owned` gestisce kill della sola sessione creata, ProcessLookupError, wait e fallback waitpid. waitid WNOWAIT mantiene il leader non raccolto fino al cleanup, evitando di riutilizzare un group ID già rilasciato. Scan/proc/KeyboardInterrupt non saltano il cleanup. Gli errori di cleanup diventano note alla causa originale, non la sostituiscono. Non è una garanzia contro condizioni kernel/credenziali fuori contratto.
2. **Filesystem.** Nuovo `securefs.py`: traversal dirfd relativo, NOFOLLOW a ogni componente, directory private possedute, file regolari nlink=1 e identità entry/FD prima di accesso e mutazioni. Nessun resolve che nasconda un ancestor symlink; nessun mkdir/scrittura prima dei preflight. chmod/pwrite/truncate/utime usano FD verificati e soltanto working copy registrate. Producer/record adottano marker-last, senza hardlink di pubblicazione nlink=2 o rename con overwrite. Il reader richiede byte completi, marker e checksum; power-loss/recovery non sono provati.
3. **Codice/protocollo pinnati.** Nuovo `guard.py`: bootstrap esterno fidato verifica i byte guard, poi manifest, tutti i sorgenti, ricevuta di review e byte reali del protocollo. Moduli propri eseguiti da byte catturati/verificati, non riaperti per pathname dopo il controllo. Ricontrolli prima di abilitare nuove scritture/pubblicazioni; ogni worker ripete la catena. Una modifica ferma la serie: nessun refresh automatico del manifest dopo errore.
4. **Agreement.** Nuovo `result_schema.py`: verifica completa prima dell'output. Generation reference==spec; schema/serializer ammessi; backend cache==recipe; coppia esatta hashlib/OpenSSL-default; SHA-256 64 hex; riferimenti/canonical JSON/recipe/chunk/snapshot/contatori/trace coerenti e oracle letterali quando disponibili. Istanze sorgente differenti ammesse e dichiarate, versioni incoerenti rifiutate. Il comparator non avvia engine alternativi né produce agreement su record invalidi.

Le directory private e i controlli FD non sono un sandbox OS per attori ostili con stesso UID/privilegi kernel. I target dei test link sono simulati dentro il futuro scratch di regressione, mai file di root/utenti reali; i test verificano byte e permessi invariati anche con UID 0. L'alias kernel del proprio memfd sealed resta distinto dai path input su filesystem.

## Freeze consegnato, senza self-hash ricorsivo

- **13 file** congelati in `tmp/m1/f-hash/source-freeze-003.json`, SHA-256 **`0b03300a13f10a3d12cf8a156766743967dab6b395c022b91a4510e84843836b`**.
- Guard SHA-256 **`22b94cd26255088a73e713ece930664bba704b1a3877d012b125a9fac2b6453d`**.
- Stato del freeze: CANDIDATE_AWAITING_INDEPENDENT_REVIEW. Il manifest contiene guard/sorgenti, non sé stesso, approval o comando con i suoi pin.
- `review-approval-003.json` **NON creato**. Dopo review, il coordinatore deve fornire una ricevuta schema 1/status APPROVED con gli stessi hash e allowed_entries esplicite; il relativo SHA arriva dalla review fidata.
- Bootstrap letterale pronto in `trusted-invocation-003.txt`, SHA-256 **`a538b52bba3adfe382b2eb1a31efb66a2a3895d0f71cbb63003145b6821106f8`**. Usa Python -I -B -S, pin guard/freeze già fissati e `$REVIEW_APPROVAL_SHA256` ancora da ricevere. Non eseguirlo ora né trattare un launcher modificabile come propria autorità: la copia del comando/pin deve provenire dalla consegna e review fidate.

La catena non importa codice locale prima della verifica del guard. I sorgenti vengono importati dal loader solo dopo autenticazione del freeze/review e controllo del protocollo reale. Una nuova versione richiede nuovi pin e review, non un manifest ricalcolato dal candidato dopo un errore.

## Verifiche realmente svolte e regressioni NON eseguite

`ast.parse` valido su **11 moduli** e sul testo del bootstrap, senza exec/import; `bash --noprofile --norc -n` su env.sh exit **0**, senza eseguirlo. Snapshot/preparazioni 001/002/report storico ricontrollati byte per byte tramite SHA. Ricevuta statica in `tmp/m1/f-hash/static-followup-003.json`; fonte/hash/linee e contatori runtime zero registrati lì. Nessuna verifica runtime delle correzioni deriva dall'AST.

Controllo sintattico ripetibile, non comando di avvio:

```bash
env TMPDIR=/opt/workspace/astroProcessor/tmp PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -I -B -S -c 'import ast; from pathlib import Path; files=sorted(Path("tmp/m1/f-hash").glob("*.py")); assert len(files)==11; [ast.parse(p.read_text(), filename=str(p)) for p in files]; print("AST only: 11 modules, no own module imported")'
env -u BASH_ENV /bin/bash --noprofile --norc -n tmp/m1/f-hash/env.sh
```

`future_regressions.py` prepara **7 metodi**, con **37 casi negativi di campo** e **4 casi di profilo CLI**, oltre ai sottocasi canonical/cross-field/trace/pair, assenza output, symlink ancestor/intermedio/leaf, hardlink, identità sostituita, preflight, scan/proc/interruzione, race cleanup/wait e pin sorgenti/protocollo/review. **Tutti NON ESEGUITI**. I byte e i permessi dei target simulati devono restare invariati; una failure di pulizia non diventa successo del test e non nasconde l'eccezione originale.

Restano **18 clean +16 errori +4 positivi =38 ricette hash**, cinque fasi producer e **11 agreement** pianificati. Oracle letterali empty/abc e snapshot sealed del contenuto byte-known invariati; nessuna pretesa di snapshot generico o soluzione ABA arbitraria. Nessun altro content engine dopo errore. Managed 256 MiB, RSS 512 MiB best effort e tutto tmp <=1 GiB restano fissi.

Misure, processi persistenti p/r, bootstrap statistico 2000 seed4059 e cache lookup sono ancora **NON IMPLEMENTATI**. Nessun throughput, CI, vettore grande o test di regressione dichiarato PASS.

## Prossimo passo

Prima review indipendente dei 13 file e della catena di bootstrap. Dopo la ricevuta APPROVED con hash pinnato: eseguire prima le piccole regressioni, poi empty/abc, nel turno seriale assegnato. Il comando già preparato per il producer empty è nel file con hash sopra; non funzionerà senza la ricevuta di review. Non rigenerare sorgenti/manifest in risposta a errori e non avviare direttamente producer/worker. Nessuna esecuzione è stata avviata in questa consegna.
