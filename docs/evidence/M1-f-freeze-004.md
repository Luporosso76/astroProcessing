# M1.f: freeze004, delta statico

Stato: **candidato per nuova review, non PASS dinamico**. Nessun import dei moduli propri, regressione, producer, worker, CLI OpenSSL, dataset, build o benchmark eseguito. Solo preservazione degli artefatti, SHA-256 dei piccoli sorgenti/archivi, AST e sintassi shell. Nessuna scrittura in altri namespace, progress/piani/protocollo, Git, rete, sistema o indici; nessuna delega.

Autore: GPT-6.1 Sol, OpenAI, dichiarato dal runtime, non attestato. Costi/token non esposti. Letti il codice reale di allocated, i test interessati e i riferimenti versione della guardia; non riletto il piano di 4.5k righe.

## Storia preservata

La review indipendente di freeze003, comunicata dal coordinatore, ha chiuso **staticamente** i quattro finding precedenti e segnalato i due qui corretti. Non si trasforma quella review in un'esecuzione o in approvazione della 004. Il riscontro 003 su namespace 0700 e datasets/runs assenti appartiene a quel momento.

- Nuovo snapshot prepatch: `tmp/m1/f-hash/history/prepatch-004.zip`, **30 file**, SHA-256 **`bb8ca037a087d473d964da01acf55e76cf8b6da5daa637d91d3d7fa50df7faf3`**; hash delle entry in `history/prepatch-004.json`.
- Freeze003 preservato: **`0b03300a13f10a3d12cf8a156766743967dab6b395c022b91a4510e84843836b`**.
- Snapshot003 preservato: `57596b8446b959f975ed364d778863eff8f9162a5c676e1913570e86ad15b89d`, **18/18 entry** ricontrollate.
- Report storico 61 righe, followup003 e manifest001/002 mantengono i byte originali. Nessuna riscrittura o rietichettatura delle ricevute.
- `plans/M1-spikes.md` resta **`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`**; anche protocol.py è byte-identico.

Preflight prima del nuovo snapshot: tutto tmp **855986176 byte**, riserva 2 MiB. La contabilità usata per la preparazione legge solo metadati NOFOLLOW; non produce un inventario o un'affermazione sulla presenza/assenza di link reali in altri namespace.

## Due modifiche funzionali

1. **MED, quota globale.** `securefs.allocated()` somma `lstat.st_blocks*512` del symlink e prosegue, senza aprire/seguire il target e senza modificare il link. Non impone la policy dei path candidati ad altri namespace. Restano intatte le guardie sui path effettivamente aperti: NOFOLLOW a ogni componente, file regolare/nlink1, directory private e identità FD/entry. AST di SecureFS, absolute_dir, identity e proc_status confrontato con lo snapshot: invariato.
2. **BASSO, test agreement.** I record sintetici vengono creati sotto un nuovo run esclusivo `runs/regression-schema-<id>/records`, accettato dalla whitelist. Collisione del run: stop, nessun overwrite. Il test richiede precisamente **Failure ERR_RESULT_SCHEMA** con diagnostica generation, oltre all'assenza di output/marker. Un SystemExit della whitelist non viene accettato come successo.

Nuovo metodo futuro `test_allocated_counts_links_without_target_traversal`: radice di quota simulata, link a file/directory di un namespace esterno simulato ma sempre nello scratch del test; stat deve usare follow_symlinks=False, le aperture dei link/target sono sorvegliate, il totale deve includere i blocchi dei link stessi. Gli stessi link devono restare rifiutati come input del candidato; byte e permessi del target devono restare invariati. Non vengono usati target di root o di altri utenti. **Regressione non eseguita**, nessuna presenza reale di link altrove inferita.

## Freeze e pin nuovi

- `source-freeze-004.json`: **13 file**, SHA-256 **`f880d0c7d23838f068254874bff96d04d81e0aae048e80a9755c1e035e7c8e91`**.
- Guard004: **`e52324b8c609feb52f15f2134af80c7b9738f50b3daba2ef9356ef708940a649`**; nomi freeze/approval e revisione ora 004.
- Il testo del bootstrap resta identico alla 003: SHA-256 **`afde76dc61b10f2cc260f6bcc9acee39a329fb107c2b9fc1805f6eaa1f155c7e`**. AST verificato, non eseguito.
- `trusted-invocation-004.txt`: SHA-256 **`3b1d6e8ea3111ba059058741a0a2c02da7daa760fc063460c7fce0d2c653ca29`**. Pin guard/freeze già fissati; `$REVIEW_APPROVAL_SHA256_REGRESSIONS` deve arrivare dalla review separata. Entry: **future_regressions**, run-id `freeze004-regressions`, non producer empty.

Nessuna approval003/004 creata. Dopo la review del delta, il coordinatore fornirà `review-approval-004.json` con hash coerenti e allowed_entries limitata a `future_regressions`. Nessuna ammissione implicita empty/abc: richiederà un'altra ricevuta separata. Freeze non contiene sé stesso, approval o comando con i pin; nessuna ricorsione self-hash o autoaggiornamento dopo errore.

## Copertura effettiva e residui

AST valido di **11 moduli** e del bootstrap; `bash -n` su env.sh exit **0**, senza eseguirlo. I sette metodi originali di regressione sono conservati; se ne aggiunge uno: **8 metodi pianificati, zero eseguiti**. Le tabelle negative 37+4 e le **38 ricette (18 clean, 16 errori, 4 positivi)** più **11 agreement** restano invariate. Snapshot nuovi/vecchi e hash dei sorgenti sono stati verificati, non gli esiti runtime dei test.

Diff effettivo: `delta-freeze003-freeze004.patch`, **128 righe**, SHA-256 **`12e8f4001bef6eda5ce33bf22f3fdae4f19aa1c79a4dee77783ff8bb62225483`**. Quattro file modificati: securefs.py, future_regressions.py, guard.py e README; **9/13 sorgenti invariati**. Ricevuta statica e linee/hash in `static-freeze-004.json`.

Snapshot sealed di contenuto byte-known, nessun claim ABA generico, nessun fallback di engine e limiti managed 256 MiB/RSS 512 MiB best effort/tutto tmp 1 GiB conservati. Misure comparative, 3p/2 warm-up/7 round, bootstrap statistico e cache lookup restano **NON IMPLEMENTATI**; nessun dataset 128 MiB generato.

Prossimo passo: review del delta/freeze004. Solo dopo ricevuta per regressioni, copiare il comando letterale della consegna fidata in `trusted-invocation-004.txt` con il pin approval fornito dal coordinatore. Non avviarlo ora e non usare file/comandi modificabili come propria autorità. M1.f e M1 restano aperti; nessuna interferenza operativa con il pilot di altro namespace.
