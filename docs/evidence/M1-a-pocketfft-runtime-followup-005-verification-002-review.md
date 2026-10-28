# M1.a pocketfft: review della verifica statica 005, tentativo002

Data: 26 ottobre 2026. Esito: MED003 chiuso nel solo ambito statico.

## Review indipendente

Questo verbale registra la review conclusa nella sessione `ses_e87f0b913ffeGe30dpmqTtOHgW`, comunicata dal coordinatore. Il modello del revisore e il conteggio delle righe lette non sono esposti; non vengono inferiti. Il revisore non ha ricalcolato gli SHA-256 né rieseguito test.

La review conferma la ricostruzione dai raw di 6 ELF, 13 archi NEEDED, 13 gruppi consumer/provider e 54 nodi di versione richiesti. Conferma inoltre il confronto MD5 locale dei 5 file host allowlisted, nei limiti della prova primaria. In `verify.py`, righe 222-247 e 265-278, il confronto usa i record esatti ricostruiti dai raw, non soltanto i conteggi. I negativi coprono omissioni e duplicati anche a cardinalità invariata.

La ricevuta documenta 48 input autenticati prima e dopo, invariati, e il binding dei 43 risultati storici `ok`: 5 positivi, 38 negativi, zero failure, errori o skip. Non sono 43 test rieseguiti durante verification002. Secondo receipt, il turno termina con exit 0, processo raccolto, gruppo posseduto assente e zero retry.

## Prova primaria e ambiente

Fonte stabile: `docs/evidence/M1-a-pocketfft-runtime-followup-005-verification-002.md`, con comandi, budget e limiti completi. Comando del turno storico, eseguito dalla root del progetto:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/runtime-inventory-followup-005/supervisor-attempt-002.py
```

Il report primario registra Python 3.14.4, GCC 15.2.0 e Linux 7.0.0-38-generic x86_64. Riporta `PASS_STATIC_VERIFICATION` per il driver e `PASS_STATIC_RELATIONS_AND_LOCAL_CHECKSUMS_ONLY` per l'entry. Il modello filesystem è cooperativo con writer esclusivo, non isolamento contro sostituzioni ostili degli antenati.

## Controllo separato del coordinatore

Il coordinatore ha verificato quattro record con aperture fd-relative `NOFOLLOW`: file regolari del proprietario corrente, singolo hard link, dimensione inferiore a 1 MiB, `fstat` stabile e SHA-256. Questo controllo è distinto dalla review e non equivale a una riverifica indipendente dei 48 input o dei cinque runtime host.

I percorsi raw seguenti sono relativi a `tmp/m1/fft-pocket/runtime-inventory-followup-005/verification-attempt-002/`.

| Record | SHA-256 verificato dal coordinatore |
| --- | --- |
| `docs/evidence/M1-a-pocketfft-runtime-followup-005-verification-002.md` | `ca1213afd8ef5241498970bc6a759e3f65c5cfd6c04cd5119bd2e18b35d14dd8` |
| `receipt.json` | `7fd44020b969d1a9989f043f75f4684ca742ba2759bdde45090cd79dbaf0f992` |
| `stdout.raw` | `5f25054e196d2604617acd094875bc715db5290ba7a5ebd1b3798c2f62ac5a49` |
| `stderr.raw` (vuoto) | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |

Il comando esatto del controllo separato non è disponibile nel mandato di questo verbale. Gli hash riportano il controllo comunicato dal coordinatore; l'autore documentale non li ha ricalcolati.

## Limiti e conservazione

Il LOW sullo stderr originale001 mancante resta aperto: lo stderr vuoto del tentativo002 non lo sostituisce. Restano mancanti tre negativi opzionali, non bloccanti per questa chiusura statica: Offset non esadecimale, Link non numerico e header all'ultima riga.

La verifica non prova live loader binding, relocation, definizioni dei simboli o IFUNC. Il database MD5 locale non è una firma indipendente né una prova di conformità distributiva. Nessun PASS licenze, qualifica dei 232 casi FFT o PASS M1 deriva dalla review.

Restano conservati il fallimento004 e lo STOP_PREFLIGHT di verification001, senza PASS retroattivo. I gate precedenti store47, viewer41, FFT36, BUILD003 e hash30/38 non cambiano. Stato corrente e prossimo gate hanno una sola sede: `docs/progress.md`.

Autore del verbale: agente documentale, modello dichiarato `openai/gpt-6-astra`. Fonti lette integralmente: `docs/progress.md` (57 righe), `docs/evidence/README.md` (160), report verification002 (69), verification001 (65) e tests005 (122). Nessuna lettura del piano esteso o dei file dell'autore; nessun nuovo test o controllo host svolto per questo verbale.
