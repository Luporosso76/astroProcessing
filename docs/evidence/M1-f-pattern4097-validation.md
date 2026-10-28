# M1.f: pattern4097, turno001

**PASS del perimetro ammesso:** producer, due clean chunk4096 e confronto, tutti exit0; due worker exit0. Incremento **2/38 ricette, 1/11 agreement**; totali distinti **6/38, 3/11**. Nessun retry, fallback o FIRSTFAIL nuovo. M1.f resta aperta.

Autore: GPT-6.1 Sol, OpenAI, dichiarato dal runtime, non attestato. Nessun override del modello/delega; costi/token non esposti. L'interruzione precedente non aveva creato outer/report: assenza ricontrollata prima dell'avvio.

## Esecuzione e binding

Outer: `tmp/m1/f-hash/turns/freeze005-pattern4097-validation-001/`. Finestra UTC: **2026-10-26T07:19:23.908933747Z / 07:19:25.282216+00:00**, non misura comparativa.

Quattro Popen tramite `/bin/bash --noprofile --norc -c`, payload trusted005 catturato/verificato; entry/argv assoluti completi in ciascun `invocation.json`, timestamp in `launch.json`. Pin esterno in REVIEW_APPROVAL_SHA256_005; HOME/TMP/XDG privati, Python isolato, OPENSSL_CONF=/dev/null.

| Fase | Argomenti | PID raccolto |
|---|---|---|
| prepare_inputs | `--case pattern4097` | 769598 |
| harness | `--engine hashlib --case pattern4097 --chunk 4096 --scenario clean --run-id freeze005-pattern4097-hashlib-001` | 769603 |
| harness | `--engine openssl --case pattern4097 --chunk 4096 --scenario clean --run-id freeze005-pattern4097-openssl-001` | 769608 |
| compare_results | due `runs/freeze005-pattern4097-<engine>-001/worker/result.json`; output nel run hashlib, `agreement-openssl.json` | 769615 |

Preflight: target nuovi assenti, namespace0700, NOFOLLOW/FD-entry, UID1000/nlink1/no022. **13 sorgenti e 398 file preesistenti invariati** per byte, inode, mtime, ownership/modi. Binding completi e hash in `preflight.json`:

| Pin | SHA-256 |
|---|---|
| freeze005 | `6db59077e7bdcdcec612b3e51b14ca963a86b8add66e5d3f5a0d760ea8d8e94a` |
| guard005 | `ffd274d0342c42fcc2c2a08f98088a3eea7f4398bf507af08f25ed64c3a6615a` |
| template | `3e2d0df8b0000e220f5a78488a80471a9595d7ec0641f171c93b54535e920209` |
| approval | `d85df7bb7ad27312d191d20b64b45950ac5ecbadda0c99a0c1c152a9ebde443c` |
| protocollo | `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba` |

## Oracle e limiti

Verificati **tutti i 4097 byte**, zero mismatch con `(73*i+19)%256`. Generation1, originali/configurazione in sola lettura0400 preservati. Due memfd sealed-v1, size4097 e `F_GET_SEALS`=15 prima del digest; identità trace/cache/ricetta coerenti e distinte: `928676bbfe65407ea3e8e0e1aa0ce95a`, `230b7db59c594b14b3a4a5885fde7ec1`. CLI osservata su `/proc/self/fd/7`; FD numerici non persistiti nell'evento del worker.

SHA osservato comune: **`6dea11fd585c26a1d50de04c9fc6c9b3f6918df71bcd53eda777069cd5512243`**. `expected_sha256=null`: non viene inventato un GT crittografico. Agreement reale stesso caso/generazione/ricetta, **AGREEMENT_ONLY**; contatori contenuto hashlib1/openssl1, nessun cambio engine.

Python 3.14.4/_hashlib, OpenSSL 3.5.5 27 Jan 2026; CLI default esplicito, provider hashlib NOT_INTROSPECTED. Possono condividere OpenSSL: nessuna indipendenza crittografica. Chunk interno CLI non controllato.

Monitor/cleanup qualificati invariati via AST; osservatore nuovo `d8ff87a148f36ce3ea27bd1b21cd967358ffbdfe5a52541bafc640a613b75270`. Worker osservati 769604/769610, raccolti dagli harness; nessun adottato/errore. Il PID769609 è l'harness OpenSSL, non il suo worker. Attribuzione corretta dopo review dei raw, senza modificare gli artefatti. Campionamento non esaustivo. Stderr primari/osservatori e log worker vuoti.

Tmp massimo campionato 949780480 B, riserva16MiB/tetto1GiB. RSS aggregato 67039232 B/512MiB best-effort. Managed prenotato 34607105 B/256MiB, distinto da RLIMIT_AS256MiB per processo; nessun contatore managed aggregato. Deadline180s, RLIMIT_FSIZE2MiB. Nessun benchmark o prova di sandbox OS.

## Handoff

`postflight.json`: **`b4399ca860632fc6d7ff4e558a9726b664d49959b8dd9551c97325fd4f7840e7`**; hash/metadati di61 artefatti, result/agreement/snapshot/PID. Nuove ricevute O_EXCL/fsync; nessuna modifica a fonti/piani/approval/permessi preesistenti, Git o altri namespace.

Prossimo incompleto: **pattern4097 clean chunk1048576**, senza rigenerare il dataset. Review indipendente e nuova ammissione prima dell'ampliamento. Slot libero all'handoff; misure/cache lookup ancora NON IMPLEMENTATE.
