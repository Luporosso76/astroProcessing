# M1.a: review della validazione pocketfft 004

## Esito e attribuzione

PASS limitato ai 232 casi della validazione 004. La review indipendente della sessione `ses_e876d0e0dffetWAbXK05FLZrXq` non ha rilevato blocker oltre all'autenticazione, successivamente completata dal coordinatore. Questo verbale registra i due controlli distinti, comunicati dal coordinatore; la redazione non ha rieseguito il candidato né ricalcolato gli hash.

Il report `M1-a-pocketfft-validation-004.md` resta immutato: il suo PENDING descrive la consegna precedente alla review. SHA-256 verificato dal coordinatore: `762af81e51a8a74c023e784f2d1f2c0a4c7d6af9bd076efe581f9df75f463cd9`.

## Risultati verificati

Il revisore ha letto tutti i 232 risultati: ID unici da 0 a 231, tutti con `pass_=true`. Il coordinatore ha confermato separatamente il conteggio di 232 PASS.

| Gruppo | Casi | Uscita attesa e osservata |
| --- | ---: | ---: |
| Finiti | 192 | 0 |
| Errori di base | 14 | 2 |
| Non finiti | 24 | 2 |
| Collisioni | 2 | 2 |

Le sei famiglie finite hanno 32 casi ciascuna. L'oracolo analitico confronta tutti i bin e le componenti con tolleranze assoluta e relativa pari a 0. L'inversa parte da uno spettro analitico indipendente dalla forward; il padding inattivo contiene sentinelle NaN. Gli errori attesi terminano senza output pubblicato.

La ricevuta dell'unico turno registra supervisor PID `1119979`, exit 0, `reaped=true`, `pending_unreaped=false` e `pinpost=MATCH`. La review legge questa ricevuta: non costituisce una nuova osservazione del sistema operativo.

Il caso 088 documenta una mappa di sei file con dispositivo, inode e percorso attesi: worker, loader, libc, libm, libgcc_s e libstdc++. Questa osservazione non dimostra il binding dei simboli né copre tutti i 232 processi.

## Autenticazione separata del coordinatore

Il coordinatore ha verificato 16 file in sola lettura con Python e libreria standard: apertura relativa a descrittori con NOFOLLOW, file regolari del proprietario atteso, `nlink=1`, dimensione inferiore a 1 MiB, `fstat` stabile e SHA-256. Il controllo comprende:

- report, supervisor, driver sorgente e copia `outer.py`, worker, profilo storico 003 e manifest observation;
- gli otto file della tabella alle righe 105-111 del report, compresi entrambi i runtime;
- `build-pin.json`, con verifica del legame semantico a worker e profilo.

I digest dei componenti e degli otto artefatti corrispondono a quelli nel report. Il digest corrente di `build-pin.json` è `d85b577537a3681f5b1ed6df36830626740878fd3a286a6a9934e05cb8a19655`: non è un confronto con un hash storico esterno di quel file.

Il primo script di sola lettura si è fermato soltanto sul percorso abbreviato `runtime-after.json` della tabella. Il coordinatore ha corretto la risoluzione del percorso e completato il controllo. Non ha avviato candidati né ripetuto la validazione. Questa autenticazione non è un nuovo controllo indipendente di tutti i 97 percorsi della build o dei 75 input storici del turno.

## Comando, ambiente e limiti

Comando del turno esaminato, non rieseguito dalla review:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/supervisor.py validate validate-followup-004 build-followup-003
```

Il report registra Linux x86_64, kernel `7.0.0-38-generic`, Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, directory private nel progetto e worker della build 003 senza ricompilazione. I raw e i relativi hash sono nella tabella del report. Il comando integrale dello script di autenticazione non è disponibile nel mandato di redazione.

La qualifica non copre prestazioni, licenze, adozione del backend o chiusura M1. La durata del turno non è un benchmark. FIRSTFAIL, build 003 e prove storiche restano conservati; nessuna tolleranza o serie temporale precedente cambia.

Data: 26 ottobre 2026. Modello del revisore e del coordinatore: non disponibile nel mandato. Redazione: agente docs, OpenAI GPT-6 Astra (`openai/gpt-6-astra`), con lettura di `docs/progress.md` (57 righe), `docs/evidence/README.md` (164 righe) e `docs/evidence/M1-a-pocketfft-validation-004.md` (114 righe). Nessun test, import del candidato o nuova esecuzione durante la redazione.
