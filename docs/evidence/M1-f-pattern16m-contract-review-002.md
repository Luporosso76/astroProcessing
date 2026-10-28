# M1.f: review del contratto16M, turno002

27 ottobre 2026. Review indipendente `ses_e80e2302bffe1T8sBPBsoT5oQx`: **PASS_SCOPED**. Confermati otto mock launcher, poi quattro unit del contratto budget sette-step. Ogni fase ha avuto un solo lancio, exit0 naturale, senza skip o retry. I raw riportano rispettivamente `Ran 8 tests` e `Ran 4 tests`, entrambi `OK`: i conteggi sono8+4, non quelli di eventuali refusi nel riepilogo della review.

Il perimetro è lifecycle mock mirato e `CONTRACT_JSON_NOT_ENFORCEMENT`. Non qualifica enforcement del producer, fault reali del kernel o funzionamento generale del launcher. STOP001 prima di Popen resta storico, senza PASS retroattivo. Lo stato corrente ha casa in `docs/progress.md`.

## Esiti e autenticazione separata

Il coordinatore comunica una verifica indipendente stdlib con FD relativi, `O_NOFOLLOW` su tutti i componenti, `O_NOATIME`, file regolari posseduti, nlink1, limite1 MiB e fstat stabile escluso atime. Ha verificato tutte le26 voci di `SHA256SUMS`, SUMS stesso, report e outer originale; poi17 PREP, sei file launcher, quattro STOP001 e dieci FIRSTFAIL contro byte/mode/SHA di `final-audit.json`. Totale deduplicato:63 file unici, nessun mismatch. Sono file, non63 test; i pin esterni005 non sono stati verificati.

Il confronto AST indipendente conferma identiche19 funzioni non-main e il resto del modulo fra bootstrap e launcher, esclusi RUN, COMMAND e main. Otto e quattro metodi unici concordano con raw e ricevute. I17 PREP e i dieci FIRSTFAIL coincidono prima/dopo.

Le ricevute dei figli PID3798825 e3835530 registrano exit0, `reaped=true`, `reserved=false`, nessun segnale e zero errori nei tre ledger primaria/cleanup/persistenza. `producer_RUN=0`. Sono prove registrate, non una nuova osservazione OS della review o di questa redazione.

`CHECKS_COMPLETE` e `final_success_attested=false` sono intenzionali nelle ricevute originali: evitano di dichiarare successo prima della propria persistenza. Exit0, diagnosi PASS su stderr, controlli post e final-audit documentano separatamente l'esito. Non occorre riscrivere quelle ricevute.

## Errata additive al report primario

`M1-f-pattern16m-contract-tests-002.md` e i checksum congelati restano immutati.

- Riga99: stderr mock106 byte, stderr contratto102 byte, non106 per entrambe le fasi. Entrambi gli stdout figli sono vuoti; dimensioni confermate dai raw e dalle ricevute verificati dal coordinatore.
- Riga144: la PREP producer separata esiste ed è NONREADY_RUN. È ancora assente un controller completo sette-step qualificato, non ogni preparazione del producer. Il namespace producer in scrittura non è stato letto in questa redazione; questa rettifica di stato deriva dal handoff del coordinatore.
- Riserva64 MiB più margine16 MiB equivalgono a80 MiB, non82. Restano distinti budget4 MiB per fase e9 MiB complessivi di setup più due fasi; `receipt_tail=147456` byte.
- I valori188416 e299008 byte del report descrivono il post esterno precedente alle ricevute finali. Il successivo final-audit registra200704 byte per delta fase2 più tail e311296 per delta totale più tail. Sono istanti diversi, non errori da cancellare. `U_before_final=937111552` byte resta uno snapshot storico, non disponibilità corrente o prenotazione.

## Comandi e limiti

Comandi effettivi registrati nel report, non rieseguiti dalla redazione:

```sh
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-contract-execution-002/bootstrap.py
/usr/bin/python3 -I -B -S tmp/m1/f-pattern16m-contract-launcher-002/outer.py
```

Il primo avvia il mock launcher; il secondo, solo dopo il PASS della prima fase e la raccolta, esegue `tmp/m1/f-pattern16m-turn-001/test_contract.py`. Ambiente registrato: Linux, Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`. Limiti figli invariati: wall55 s, CPU50 s, AS268435456, FSIZE1048576, NOFILE64, CORE0; ambiente privato e log bounded. I tempi unittest non misurano il wall time completo del launcher.

Le unit JSON non dimostrano un ceiling fisico del producer. La ricerca ext4 non prova che PA64 MiB comporti un addebito locale64 MiB; non costituisce qualifica del ceiling. Nessun producer, nativo, build o unlink ammesso da questo PASS. Restano30/38 ricette e7/11 agreement, con128 MiB, cache e prestazioni pendenti; nessuna adozione di stack o chiusura M1.

## Fonti lette dalla redazione

Questo verbale attribuisce review e autenticazione63 file ai controlli indipendenti comunicati dal coordinatore; non li presenta come una nuova esecuzione propria. Letti report primario145 righe, final-audit386, progress70 e coda indice170-179 (10):611 righe. Non letti il producer in scrittura, candidati o pin esterni005. Nessun import, test, build, rete, installazione, indice, Git o delega.

SHA-256 ricalcolati in questa redazione con `sha256sum`, exit0 dalla radice del progetto:

| Fonte | SHA-256 |
| --- | --- |
| `docs/evidence/M1-f-pattern16m-contract-tests-002.md`,7421 byte secondo final-audit | `58874a3bf942768853a5a488f14f7174d66b1ae6c2d6021cb54ee0c9710dfd5c` |
| `tmp/m1/f-pattern16m-contract-execution-002/SHA256SUMS` | `444c3597fad2f19b1541a9f2a5ec2a102b27fba0f6794c34cca7fa18968fe139` |
| `tmp/m1/f-pattern16m-contract-execution-002/final-audit.json` | `f26845be96dc812f6ef31768d36e278d7dab030e51b9f82ecd015583bb04f81d` |

Versione dell'utility hash e costi non rilevati. Nessun primario, raw, manifest o SUMS modificato.
