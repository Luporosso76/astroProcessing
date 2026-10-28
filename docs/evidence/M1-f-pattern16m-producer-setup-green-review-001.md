# M1.f: review del GREEN setup

## Metadati del record

- Unità: M1.f.
- Scopo: registrare la review del singolo GREEN setup senza estenderla alla qualifica del producer.
- Comando: comando primario in «Comando e risorse», già eseguito dall'esecutore; comando hash della precedente redazione in «Fonti e limiti della redazione». Nessuno dei due è rieseguito in questa correzione di formato.
- Input: percorsi e hash in «Fonti e limiti della redazione»; pin del controller riportato in «Prova e perimetro», non letto dal namespace in scrittura.
- Versioni: Python registrato in «Comando e risorse»; versione sha256sum e librerie non disponibile. Compilatore di un nuovo binario non applicabile: nessun binario creato. Piano attivo v2.2.1, senza nuova autenticazione.
- Ambiente: Linux, radice `/opt/workspace/astroProcessor`; kernel, CPU, RAM, GPU/driver e filesystem non disponibile.
- Risultato: exit0 del test dalle ricevute storiche, descritte in «Prova e perimetro»; exit0 del comando hash dalla precedente redazione. Nessuna nuova esecuzione del test.
- Output: questo verbale; report primario, control/final-audit e RUN indicati in «Comando e risorse» contengono le prove storiche, non nuovi output di test.
- Ambito: review del risultato storico e autenticazione documentale; copertura dettagliata in «Prova e perimetro».
- Esito: PASS nel solo perimetro PASS_SCOPED sotto indicato; M1 resta aperta.
- Limiti: «Prova e perimetro» e «Fonti e limiti della redazione»; nessuna qualifica producer o ammissione futura.
- Autore e data: subagente documentazione, 28 ottobre 2026; modello runtime non disponibile. Review indipendente attribuita alla sessione sotto indicata; costi non disponibile.

28 ottobre 2026. Review indipendente `ses_e8070867dffe1fWCXZ651O041W`: **PASS_SCOPED** sull'unico regression test setup. Il test è byte-identico al RED e termina naturalmente con `Ran 1 test`, `OK`, exit0. Nessun retry o PASS retroattivo del RED. Diciannove baseline e test avversi restano0 RUN; producer0 RUN.

## Prova e perimetro

Il coordinatore comunica l'autenticazione di109 file unici:108 voci SUMS più il file SUMS, e il controllo di87 file protetti attuali. Ha confermato l'invarianza AST delle19 funzioni non-main e del resto del modulo rispetto a outer002, esclusi gli adattamenti autorizzati RUN/COMMAND/main. Non ha rieseguito il test.

La ricevuta del figlio PID260405 registra exit0, reap riuscito, reserved=false, slot_blocked=false, nessun segnale e tre ledger esterni vuoti. Si tratta di record dell'esecuzione, non di una nuova osservazione OS. `CHECKS_COMPLETE` e `final_success_attested=false` precedono intenzionalmente la persistenza; exit0, PASS su stderr e final-audit completano l'esito.

Il GREEN prova la visibilità della causa tramite eccezione. Non asserisce separatamente tipo e attributi esatti di `SetupDiagnosticError`, identità del report o `receipt_persisted=False`. I test avversi sono soltanto in preparazione; review ed esecuzione restano distinte e da ammettere. La review chiarisce il LOW ipotizzato nel primario: con diagnostic che solleva nell'handler, il tratto dopo finally non viene eseguito; primary None non è raggiunto su quel percorso. Nessun placeholder è stato adottato e non si dichiara una nuova prova dinamica del ramo.

Controller congelato nel report: SHA-256 `9e58d6732248b4ffaceac151ced1ce0f68761d8164ccb003074245d940ceb8d6`,35953 byte. Ceiling fisico/evidenza NULL, ready_run false e producer run_count0 restano invariati. La redazione non ha letto il namespace producer o quello dei test avversi in scrittura.

## Comando e risorse

Comando eseguito nel turno primario, non ripetuto qui:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-producer-setup-green-test-001/outer.py
```

Report primario: `docs/evidence/M1-f-pattern16m-producer-setup-green-001.md`. Controllo e final-audit: `tmp/m1/f-pattern16m-producer-setup-green-test-001/`; RUN: `tmp/m1/f-pattern16m-producer-setup-green-001/`. Ambiente registrato: Linux, Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`. Cap del solo interprete: wall55 s, CPU50 s, AS268435456, FSIZE1048576, NOFILE64, CORE0; ambiente privato, log bounded.

Snapshot finale U937877504 byte, tail147456; crescita fase più tail221184 entro4 MiB e totale372736 entro9 MiB. I192512/344064 byte del report appartengono al post esterno precedente: non sono errata. Cap globale1 GiB, riserva64 MiB e margine16 MiB restano separati. Questi snapshot non attestano capienza futura, prenotazione, durabilità o enforcement producer.

## Fonti e limiti della redazione

Letto integralmente il report primario (153 righe), cercati nel final-audit i quattro valori finali; ricalcolati nella precedente redazione gli hash con questo comando, exit0 dalla radice del progetto. Le ultime tre fonti riguardano il distinto record source-license:

```sh
sha256sum -- docs/evidence/M1-f-pattern16m-producer-setup-green-001.md tmp/m1/f-pattern16m-producer-setup-green-test-001/SHA256SUMS tmp/m1-fft-tools/pocketfft_hdronly.h tmp/m1-fft-tools/LICENSE.md tmp/m1/fft-pocket-benchmark/driver.cpp
```

| Fonte | SHA-256 |
| --- | --- |
| `docs/evidence/M1-f-pattern16m-producer-setup-green-001.md` | `b44946dd51aca8dc54d0663fcdd0b876d0fa267cb488ad3c3214b78489d505a7` |
| `tmp/m1/f-pattern16m-producer-setup-green-test-001/SHA256SUMS` | `ba14218e8a5f8d1355be3800bb06d30472aebd12f0cf4ef42ece2a6c8ff3a5c5` |

Per `tmp/m1/f-pattern16m-producer-setup-green-test-001/final-audit.json`, consultato tramite ricerche mirate, SHA-256 della redazione non disponibile. Non viene attribuita alla redazione la verifica estesa del coordinatore.

Review e autenticazione estesa sono attribuite al handoff indipendente, non rieseguite dalla redazione. Nessun test, probe, import candidato, producer, native o unlink. Primario, raw, SUMS e sorgenti invariati. Versione utility e costi non rilevati. Il PASS non chiude M1 né autorizza altra esecuzione; la review documentale di questo verbale resta successiva alla consegna.
