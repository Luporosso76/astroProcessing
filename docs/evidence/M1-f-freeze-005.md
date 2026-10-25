# M1.f: freeze005, preparazione statica

Stato: candidato stabile per review. **Nessun import proprio, test, producer, worker, CLI OpenSSL, fixture o misura eseguito.** Gli 8/8 della 004 restano prova storica confermata dalla review indipendente; non sono copertura della 005. Nessuna delega, Git, rete, installazione o modifica ad altri namespace.

Autore: GPT-6.1 Sol, OpenAI, nome dichiarato dal runtime. Costi/token non esposti. Lettura mirata di guard167, regressioni260-299, README54, producer44 e harness77-114; poi hash e AST dei sorgenti, senza rileggere il piano completo.

## Preservazione prima delle modifiche

`tmp/m1/f-hash/history/prepatch-005.zip` conserva **16 entry**: tutti i 13 sorgenti della 004 più freeze004, approval004 e trusted004. Byte e modi Unix di ogni entry sono stati verificati; UID/GID, inode, mtime e nlink originali sono registrati in `history/prepatch-005.json`. Archive e ricevuta sono nuovi, O_EXCL, fsync e mode0400. Il preflight tmp contava 861020160 byte, con riserva 2 MiB entro 1 GiB.

**139 file storici** ricontrollati invariati per hash, modi, UID/GID, nlink, inode e mtime: includono FIRSTFAIL, turni precedenti, manifest004/approval004/trusted004 e report 002 di 93 righe. Non sono stati riscritti. Lo snapshot contiene il guard004 con SHA `e52324b8c609feb52f15f2134af80c7b9738f50b3daba2ef9356ef708940a649` e gli altri 12 sorgenti corrispondenti al freeze004. I sorgenti correnti sono invece legittimamente versione005: non si dichiara che coincidano ancora tutti con il manifest004.

## Delta minimo

- `guard.py`: nomi freeze/approval e revisione portati a 005. Cinque ENTRIES immutate: prepare_inputs, harness, worker, compare_results, future_regressions. CURRENT resta None; nessuna modifica alle funzioni di bootstrap, approval, loader o comando, né alle guardie di sicurezza. Il literal bootstrap è identico alla 004.
- `future_regressions.py`: i tre wrong-pin probe usano ora **future_regressions, entry ammessa**, anziché un producer già non ammesso dalla vecchia approval. Ogni target usa un run-id nuovo, verificato assente prima e dopo. Prima delle assertion viene salvata una ricevuta con argv, returncode effettivo e stdout/stderr integrali in hex. Si richiedono exit1, stdout vuoto e ultima riga stderr esatta per il pin guard, freeze o approval. Non basta più un generico nonzero dovuto all'entry.
- README aggiornato al gate005 e alla distinzione tra approval delle entry e ammissione dei casi.

Restano **8 metodi**, zero aggiunti; tre sottocasi aggiornati. Tabelle 37 campi negativi e quattro profili sintetici invariate. **10/13 sorgenti byte-identici**, inclusi fixture, worker, harness, comparator, securefs e protocol.py. Nessun cambiamento agli algoritmi di snapshot/hash o allo schema risultati.

## Nuovi pin e verifiche effettive

Percorsi relativi a `tmp/m1/f-hash/`:

| Artefatto | SHA-256 |
| --- | --- |
| history/prepatch-005.zip | `01976fd9545061d45745ab79cbe3a19e2e7d951279d04dea12b06c975ca5450d` |
| source-freeze-005.json | `6db59077e7bdcdcec612b3e51b14ca963a86b8add66e5d3f5a0d760ea8d8e94a` |
| guard.py corrente005 | `ffd274d0342c42fcc2c2a08f98088a3eea7f4398bf507af08f25ed64c3a6615a` |
| trusted-invocation-005.txt | `3e2d0df8b0000e220f5a78488a80471a9595d7ec0641f171c93b54535e920209` |
| delta-freeze004-freeze005.patch | `8f315b96dce094e4f527eb9dd15b537d3749afac918fd49ab8d2b1dffdc4aad5` |

Manifest di **13 fonti**; diff di **93 righe** su tre file. AST valido di 11 moduli e del literal bootstrap. `bash --noprofile --norc -n` su env.sh e sul nuovo template: exit0, senza eseguirli. Verificati namespace0700 e tutti i 13 sorgenti senza bit022; i nuovi artefatti sono stati creati 0600 e chiusi 0400, prima di qualsiasi futuro test. Nessun allargamento dei permessi. Dettagli e conteggi in `static-freeze-005.json`.

Protocollo reale invariato: `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`. Nessuna ricorsione self-hash: il freeze contiene i 13 sorgenti, non sé stesso, approval, esempio o template con i nuovi pin. Il template conserva i byte del bootstrap, fissa guard/freeze005 e richiede `${REVIEW_APPROVAL_SHA256_005:?external review-approval-005 SHA-256 required}`; entry e argomenti arriveranno tramite i parametri della shell, soltanto da un'ammissione separata.

## Handoff, senza autoapproval

**review-approval-005.json manca.** Esiste soltanto `review-approval-005.example.json`, schema1, stato **PENDING_INDEPENDENT_REVIEW_NOT_APPROVED**, nuovi hash e le cinque entry esatte. Non è una ricevuta valida e il candidato non la usa come tale. Dopo review del freeze stabile, solo il coordinatore creerà l'approval reale e ne fornirà il pin esterno.

Approval delle cinque entry non ammette tutti i dataset o comandi. Prima futura ammissione contenuto: soltanto producer empty/abc e harness clean dei due engine, chunk1, in turno assegnato separatamente. Nessun 16/128 MiB o performance; compare e ulteriori casi richiedono la propria ammissione. Nessuna esecuzione parte da questa consegna.

Restano 38 ricette e 11 agreement pianificati, non eseguiti qui; managed256/RSS512 best effort/tmp1GiB e no fallback invariati. Misure 3p/2 warm-up/7 round, bootstrap statistico e cache lookup restano NON IMPLEMENTATI. Prossimo gate: review005, approval del coordinatore, poi eventuale turno contenuto. M1.f resta aperta.
