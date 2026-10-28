# Review indipendente del lotto24+4

**PASS del perimetro eseguito**, non chiusura M1.f. Agente `tests`, sessione `ses_e88f11b0affewIIvJMATMUr1fm`, routing e sostituti configurati senza override. Modello effettivo del revisore non esposto nel ritorno; costi non esposti. Autore dichiarato dai raw: GPT-6.1 Sol/OpenAI, non attestato.

## Riscontri dai raw

- Quattro clean: chunk1048576/8388608 per entrambi gli engine; quattro positivi aggiuntivi: sealed-write-attempt/reopen-unchanged, chunk4096, entrambi gli engine.
- Sedici negativi: truncate/trailing `ERR_LENGTH`; declared-size/stale-generation `ERR_MANIFEST`; mutate-unread/reopen-mutated `ERR_CONTENT`; read-error `ERR_READ`; engine-error `ERR_ENGINE`. Ciascuno per due engine, con workerexit1, harness `EXPECTED_ERROR` exit0, marker errore validi e nessun risultato pubblicato.
- Ventotto primari exit0, otto worker positivi exit0, nessun skip, retry o inatteso registrato. Contatori selezionati coerenti, altro backend sempre0; tentativi contenuto hashlib7/openssl7, non14 risultati pubblicati.
- Quattro confronti `AGREEMENT_ONLY`, versione e ricetta non-backend coerenti, `expected_sha256=null`. Nessuna indipendenza crittografica dedotta dall'accordo.
- Originale generation1 riletto: tutti4097 byte uguali a `(73*i+19)%256`, zero mismatch, hash/metadati preservati.

## Integrità verificata

| Artefatto | SHA-256 ricalcolato |
|---|---|
| `receipt.json` | `f89da0930f693941e7eff4a9d0ba20c2c23a90895cd46bc0e79ea15dd93675a8` |
| `postflight.json` | `e9e8c4f2421f290f5e5224c0157e09e6c456e4862528d2d8151aa624726c4bdd` |
| Schedule canonico | `0b722ebd1e6c977613cb644faee1b087b6d937cee75dbd5c9d41d5ecde9de786` |

Namespace: `tmp/m1/f-hash/turns/freeze005-pattern4097-completion-001/`. Hash/metadati dei480 artefatti, report e ricevute controllati;13 sorgenti, freeze005, guard, template, approval e protocollo coerenti.

La conservazione dei461 file storici resta attestazione del turno, non nuova rilettura indipendente. I ruoli PID concordano con gli argv campionati, incluse transizioni pre-exec; non è un censimento universale. Raccolta dei worker attestata dai controller. Nessun import candidato, test rieseguito, comando hash o qualifica prestazionale da questa review.

**Totali confermati:30/38 ricette,7/11 agreement.** Restano gli otto clean su16/128MiB e quattro agreement, oltre a misure e cache lookup non implementati. Nuovi dataset o turni richiedono preflight e distinta ammissione; nessuna estensione automatica.
