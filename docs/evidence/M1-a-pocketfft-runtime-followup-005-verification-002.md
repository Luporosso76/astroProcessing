# M1-a pocketfft: verifica statica follow-up 005, tentativo 002

## Esecuzione reale

Un solo avvio del driver, senza retry. Destinazioni del tentativo e di questo report assenti prima dell'avvio, inclusi eventuali symlink.

Directory: `/opt/workspace/astroProcessor`.

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-005/supervisor-attempt-002.py
```

SHA-256 del driver verificato prima dell'avvio: `8bc91f49c98bce5f5f02092b825b552e022ee5a3aa3af9a6b45ed6d9337dddf2`.
La lettura integrale del driver e dell'entry `verify.py` ha preceduto l'esecuzione.
SHA-256 dell'entry autenticata dal driver: `303545d4263bfda66f5245e1b6c04228a6ae93a753c4318c8af61eeffb9d67a9`.

Il driver ha verificato il comando preregistrato e avviato:

```sh
/usr/bin/timeout --signal=TERM --kill-after=5s 55s /usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-005/verify.py
```

Intervallo UTC: `2026-10-26T11:51:57.834916+00:00` a `2026-10-26T11:51:58.033343+00:00`.
Esito driver: `PASS_STATIC_VERIFICATION`, exit code `0`.
Esito entry: `PASS_STATIC_RELATIONS_AND_LOCAL_CHECKSUMS_ONLY`.
Durata totale osservata dal driver: `0.1984443289693445` secondi, senza valore di benchmark.

## Copertura raggiunta

- Autenticazione di 48 input prima e dopo l'esecuzione; byte e stat di tutti i 48 input invariati.
- Binding dei risultati storici di 43 test distinti: 5 positivi, 38 negativi, zero errori, fallimenti o skip. Nessun test rieseguito in questo turno.
- Ricostruzione dai raw 003 di 6 oggetti, 13 archi NEEDED e 13 gruppi consumer/provider.
- Controllo di 54 nodi di versione richiesti contro le definizioni salvate dei provider.
- Verifica SHA-256/stat e corrispondenza MD5 locale per i soli 5 file host allowlisted.
- Zero esecuzioni del candidato, zero casi FFT e zero rilanci di strumenti di metadati.

I cinque file host sono `ld-linux-x86-64.so.2`, `libc.so.6`, `libm.so.6`, `libgcc_s.so.1` e `libstdc++.so.6.0.35`, nei percorsi canonici fissati da `verify.py`.

## Limiti operativi ed ambiente

Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Host anonimizzato, Linux `7.0.0-38-generic`, architettura `x86_64`.
Modello dichiarato dal runtime di questa sessione: `openai/gpt-6.1-sol`; nessun override. Costi, token e failover non esposti.

Il driver ha mantenuto umask `077`, directory private del tentativo, ambiente privato e limiti preregistrati: CPU 30 s, address space 268435456 byte, output regolare 1048576 byte per stream, wall 55 s, grace 5 s, raccolta esterna 62 s e totale 120 s. Cap input 16777216 byte; cap namespace allocato 8388608 byte.

Budget allocato `tmp/`: 960520192 byte prima e 960565248 dopo. Namespace: 262144 byte prima e 307200 dopo. Gate di spazio superati con cap globale 1073741824 byte, riserva 67108864 byte e margine 16777216 byte.

Il modello filesystem resta cooperativo con writer esclusivo, come autorizzato nel mandato del turno. Non offre isolamento contro sostituzioni ostili degli antenati, incluse ABA. Nessun chmod eseguito sugli antenati; i gate più restrittivi su `fft-pocket` e BASE sono rimasti attivi.

PID e gruppo posseduti: `996639`. Il driver ha raccolto il processo e verificato l'assenza finale del gruppo: `child_collected=true`, `owned_group_absent_final=true`, `slot_free=true`.

La review indipendente ammessa dal mandato è `ses_e87f3a404ffeMaNTVsUHiFIZWo`. Il campo storico `review_session` del driver riporta invece `ses_e8803a229ffeTcwTFNRjsrl3xr`; questo turno non lo ha modificato né usa quel campo come prova della nuova review.

## Artefatti e limiti probatori

Directory artefatti: `tmp/m1/fft-pocket/runtime-inventory-followup-005/verification-attempt-002/`.

- `receipt.json`: SHA-256 `7fd44020b969d1a9989f043f75f4684ca742ba2759bdde45090cd79dbaf0f992`.
- `stdout.raw`: 689 byte, SHA-256 `5f25054e196d2604617acd094875bc715db5290ba7a5ebd1b3798c2f62ac5a49`.
- `stderr.raw`: 0 byte, SHA-256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

Questo stderr vuoto appartiene al tentativo 002. Lo stderr originale di failure001 resta mancante; la sua diagnosi resta una trascrizione.

La prova copre la verifica statica richiesta per MED003. Non dimostra live loader binding, risoluzione delle relocation o definizioni dei simboli. Il database MD5 locale non è una firma indipendente né un'approvazione di licenza. L'autenticazione riguarda gli input nominati, non tutta la storia.

Nessun worker FFT, ldd, loader, build, import del candidato, accesso rete, comando Git, installazione o delega eseguito. Codice, pin e receipt preesistenti non modificati; nessun aggiornamento di progress o indici.

Nessun gate statico del driver resta incompleto. Il primo limite probatorio non coperto è il live binding, fuori dal perimetro autorizzato; questo report non chiude gate di licenza o FFT.
