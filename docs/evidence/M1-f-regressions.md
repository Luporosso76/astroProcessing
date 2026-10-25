# M1.f: turno regressioni freeze004 fermato al preflight

Esito: **FIRSTFAIL_PREFLIGHT_STOP**, codice di uscita **1**. Il bootstrap non è stato avviato. **0/8 metodi eseguiti**; nessun PASS dinamico, nessun retry. Nessuna modifica a sorgenti, approval, freeze, bootstrap, protocollo o permessi originali.

Autore del verbale: GPT-6.1 Sol, OpenAI, nome dichiarato dal runtime. Costi/token non esposti. Letti regressioni299, guard167, harness114, securefs301, review23 e invocazione16, senza rileggere il piano completo. Nessuna delega.

## Primo errore effettivo

Il controllo esterno ha letto prima il freeze. Aprendo `guard.py`, ha rifiutato il bit di scrittura del gruppo, prima di calcolarne il digest nel preflight iniziale:

```text
Traceback (most recent call last):
  File "<stdin>", line 45, in <module>
  File "<stdin>", line 27, in read
AssertionError
```

La riga fallita combina identità FD/entry e `not s.st_mode&0o022`. La diagnosi successiva ha verificato identità coerente, file regolare, UID 1000, nlink=1 e **mode 0664**; quest'ultimo basta a rendere falsa l'assertion. Non è un nuovo vincolo del test: il bootstrap letterale verificato rifiuta già `s.st_mode&0o022` prima di eseguire guard. La funzione `guard.read_at` applica lo stesso controllo ai sorgenti, all'approval e al protocollo reale.

La diagnosi successiva, senza avviare codice candidato, ha trovato **tutti i 13 sorgenti congelati**, `review-approval-004.json` e `plans/M1-spikes.md` con mode **0664**. Anche correggere il solo guard lascerebbe altri blocchi di preflight. Non viene attribuita una causa a questi permessi: nessuna evidenza dimostra quando o da chi siano stati impostati. Il namespace f-hash è stato verificato **0700**, UID 1000; la sua privacy non annulla il controllo esplicito sui file.

## Pin verificati in diagnosi

I byte coincidono con i pin fidati; il blocco riguarda i metadati, non una divergenza di hash:

| Artefatto | SHA-256 effettivo |
| --- | --- |
| source-freeze-004.json | `f880d0c7d23838f068254874bff96d04d81e0aae048e80a9755c1e035e7c8e91` |
| guard.py | `e52324b8c609feb52f15f2134af80c7b9738f50b3daba2ef9356ef708940a649` |
| trusted-invocation-004.txt | `3b1d6e8ea3111ba059058741a0a2c02da7daa760fc063460c7fce0d2c653ca29` |
| review-approval-004.json | `d61cd9a8a8c0d77cddef3332a9813ebbbc092e16f59bf942856e8414733fdcc1` |
| plans/M1-spikes.md | `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba` |

Confrontati **13/13 sorgenti** con il freeze. Approval di **269 byte**, schema1/status APPROVED, allowed_entries esattamente `future_regressions`. Non è stata riscritta o aggiornata. Bootstrap catturato identico al literal, SHA-256 `afde76dc61b10f2cc260f6bcc9acee39a329fb107c2b9fc1805f6eaa1f155c7e`.

Snapshot003 **18/18** e snapshot004 **30/30** verificati; digest rispettivamente `57596b8446b959f975ed364d778863eff8f9162a5c676e1913570e86ad15b89d` e `bb8ca037a087d473d964da01acf55e76cf8b6da5daa637d91d3d7fa50df7faf3`. Freeze003, report storici e manifest001/002 restano byte-identici. Nessuna storia precedente riscritta.

## Comando e prove conservate

Nuovo scratch esclusivo: `tmp/m1/f-hash/turns/freeze004-regressions/`. Il comando realmente avviato è stato soltanto il preflight esterno:

```text
env -i PATH=/usr/bin:/bin TMPDIR=/opt/workspace/astroProcessor/tmp PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -I -B -S -
```

Stdin effettivo conservato in `preflight-executed.py.txt`, **3835 byte**, SHA-256 `bbb6f77ed321af2742896b8dd2574b7188f28705da7ee89821e43a092c369a09`. Non è stato rieseguito. L'ambiente del Python conteneva esclusivamente PATH, TMPDIR e PYTHONDONTWRITEBYTECODE sopra indicati; nessun HOME o userconfig importato dal Python isolato. Runtime esatto e argv sono nella ricevuta.

`trusted-invocation.captured.txt` conserva i **1623 byte** del comando candidato verificato, con il suo pin originale. `receipt.json` registra separatamente l'argv candidato previsto, con approval pin fornito dal coordinatore, marcato **NOT_EXECUTED**. Non esiste un ambiente candidato o un PID candidato da dichiarare eseguito.

- `FIRSTFAIL.json`: SHA-256 `e30095885a7462b123cc4b7cacf11cea51d17d93019f77251f883a74a6083422`.
- `firstfail.stdout`: **0 byte**.
- `firstfail.stderr`: **124 byte**, SHA-256 `73b6016276188578bd54ca24ee783d66ff4df3d518617cc7017db12ae5c46d36`.
- `receipt.json`: SHA-256 `0c9088e5c37b207fbdb6ab53c9f75b66524be8bcf61bf3bdf4f86fb440264754`.

FIRSTFAIL e ricevute sono creati senza overwrite, fsync e mode **0400**, dentro directory **0700**. Questa è conservazione operativa in sola lettura, non un attributo immutable del kernel contro il proprietario o root. Le diagnosi successive hanno letto soltanto metadati e piccoli artefatti; non hanno ritentato il candidato.

## Copertura e risorse effettive

Otto metodi confermati tramite AST, sette originali più quello sulla contabilità link. **Zero metodi, sottocasi, assertion di regressione o skip eseguiti**: il solo errore osservato è l'assertion del preflight esterno. Il test agreement non ha raggiunto né ERR_RESULT_SCHEMA generation né la whitelist; la policy link non è stata esercitata. Non si attribuisce loro un esito.

Nessun figlio di test/Popen candidato creato: zero PID candidati da raccogliere. Nessun worker di contenuto, producer, CLI OpenSSL, digest di dataset empty/abc o dataset grande avviato. Le stringhe/digest sintetici dei test restano preparazione, non risultati di engine. I target link simulati non sono stati creati o modificati. Directory dei test `regressions/freeze004-regressions`, run sintetico agreement e datasets risultavano assenti durante la diagnosi.

Il preflight iniziale non ha raggiunto il controllo quota. Prima di scrivere le sole prove, la diagnosi ha misurato tutto tmp via metadati NOFOLLOW, includendo blocchi di directory e dei link stessi: **859406336 byte**, con riserva finita **16777216 byte**, entro **1073741824 byte**. Nessuna scansione del contenuto o dei target di link esterni. Managed **256 MiB** e RSS **512 MiB best effort** restano limiti richiesti, ma non hanno una misura runtime candidata perché il candidato non è partito.

Restano non eseguite le **38 ricette hash** e gli **11 agreement**. Misure 3p/2 warm-up/7 round, bootstrap statistico e cache lookup restano **NON IMPLEMENTATI**. Questo turno non chiude M1.f né M1.

## Proposta di correzione, non applicata

Il coordinatore deve gestire con autorizzazione separata il difetto dei permessi di consegna, senza allentare il controllo o ricalcolare i pin per aggirarlo. La correzione minima proposta è rendere non scrivibili da gruppo/altri i file sottoposti alla guardia, con identità e byte verificati. Anche il protocollo reale richiede valutazione esplicita: modificarne i permessi era vietato in questo turno e non è stato fatto.

Serve una nuova consegna verificata, review indipendente e nuova ammissione prima del prossimo tentativo. Un eventuale cambio della policy o del codice richiede nuova versione congelata e review. Nessun chmod automatico, approval refresh o retry qui. Empty/abc restano fuori dall'ammissione.
