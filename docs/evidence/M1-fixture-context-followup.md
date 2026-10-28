# M1: follow-up PREP di fixture materialize e supervisore

Stato: `PREP_ONLY_NOT_RUNTIME_PASS`, review indipendente ancora necessaria.
Il reviewer ha segnalato due MED nella PREP precedente: fixture materialize incapace
di arrivare all'emissione e attese di raccolta senza timeout nel launcher.
Sono difetti di fixture/harness, non vulnerabilità dimostrate dello store v2.
Questa consegna modifica solo il delta context-lifecycle, il suo piano e questo nuovo record.
Il report precedente da 139 righe resta immutato, come i sei pin già registrati.

## Snapshot prima degli edit

Cinque copie integrali: test da 271 righe, launcher 197, receipt 47, piano 65 e
report 139. Snapshot: `tmp/m1/fixture-store/context-lifecycle-tests/history/followup-001/`.
La ricevuta associa ogni copia al path originale, byte, righe e SHA-256.
SHA della snapshot receipt:
`777b5c13f431f116e15d7f900490387bc4852ae6660e3066d443020a19707679`.
Copie e ricevuta 0444, directory snapshot 0555; file e directory sincronizzati,
hash delle copie verificati prima di modificare i sorgenti. Non sono immutable flag OS.
I sei pin sono verificati prima della copia e dopo le correzioni.

## MED fixture: closure materialize prima dell'emissione

`admitted_phase` chiama `closure` prima di `_phase_bindings`: PINS vuoto non copriva
INDEX e avrebbe causato KeyError. La fixture ora sostituisce anche la closure
scientifica, esclusivamente per materialize, con `['keep.bin']`. Dopo l'emissione
verifica una chiamata esatta con inventario sintetico, case `synthetic`, direzione
`forward`. Non salta materialize e non crea manualmente un contesto nel registro.

Questo prerequisito è sintetico come inventory, PINS, review dichiarativa e binding
delle ricevute precedenti. Non prova le due dipendenze scientifiche reali né closed168.
Restano reali SHA/identità dell'ammissione, emissione, copia e sigillo dei binding,
lease e fdinfo, revoca, gate successivi e diagnosi specifica dei rifiuti.
Le verifiche positive di contesto emesso precedono ancora ogni fault della fixture.
I 18 corpi test originali sono identici via AST alla snapshot, senza skip o casi rimossi.
Le fotografie dei piccoli file e le asserzioni prima/dopo i rifiuti restano invariate.

## MED supervisore: due attese finite, stato pending esplicito

`supervise` registra l'oggetto Popen proprio prima di attenderlo. Il budget parte
prima di Popen: 120 secondi per le attese di controllo, inclusi al massimo 5 secondi
di raccolta dopo kill. La prima wait usa il residuo meno la riserva cleanup; la
seconda usa il minimo fra 5 secondi e il residuo. Entrambe passano `timeout`, anche
con residuo zero. Non esistono wait finali illimitate, loop di retry o contextmanager
Popen che attenda implicitamente all'uscita. Kill e wait usano solo l'oggetto originario.

Se il child resta irrisolto, la ricevuta è `STOP`, con `PENDING_UNREAPED`,
`own_child_reaped: false`, exit nullo, `ownership_reserved: true` e
`slot_release_allowed: false`. Log/result possono ancora cambiare: non vengono
autenticati come conclusi e non viene eseguita la scansione globale post-run.
La piccola ricevuta parent viene scritta O_EXCL/NOFOLLOW e fsync nel budget metadati
già prenotato, senza richiamare il guard globale mentre il child può scrivere.

Il registro mantiene il riferimento Popen finché il parent vive. PID del parent e
child sono diagnostici: la ricevuta non autorizza segnalazioni o raccolta da un altro
processo usando quel numero. Se il parent termina, il riferimento non è trasferibile;
slot e prenotazione restano bloccati finché la situazione non viene realmente risolta.
Non esiste un comando automatico di recupero basato su PID o discovery di processi.
Una raccolta riuscita registra `REAPED`; anche allora la ricevuta non dichiara libero
lo slot, che richiede ancora review del coordinatore.

Non si promette un hard deadline OS: D-state, scheduling e I/O come fsync possono
oltrepassare il budget di controllo. SIGKILL non garantisce che un child sia subito
reapable. La correzione elimina le attese esplicitamente infinite e rende visibile
il caso irrisolto, senza fingere una garanzia che il sistema operativo non offre.

## Regressioni preparate e schema futuro

Cinque nuovi metodi del supervisore usano solo test double, clock deterministico e
nessun processo reale: successo senza kill, timeout con raccolta riuscita, timeout
di cleanup con identità conservata, errore di segnalazione e deadline già esaurita
con errore di controllo. Asseriscono timeout finiti, numero di wait/kill, stato di
raccolta, riserva dell'oggetto e blocco slot. Non dipendono da misure temporali o log.
Queste regressioni non sostituiscono una futura qualificazione con processi reali.

Conteggio: **24 test v2 invariati + 18 lifecycle invariati + 5 supervisore = 47**.
Il launcher impone i tre conteggi e `failfast=True`; source_hashes copre ancora
esattamente store v2, test v2, nuovo test e nuovo launcher. Nel worker ammesso viene
esposto il modulo launcher già caricato ai test del supervisore, senza avviare main.
Schema di ammissione 2 e scope restano quelli della PREP precedente, ma
`tests_expected` deve essere **47**, non 42. Review e source_hashes devono usare i
nuovi byte. Nessun ADMIT è stato creato o riutilizzato.

Budget invariato: 16 777 216 byte, più margine globale 16 MiB entro cap 1 GiB.
Le cinque regressioni nuove non creano file o child. Restano il limite 128 fixture,
12 nodi e 4096 byte/file, 4 MiB metadati, log e cache sotto ROOT, nessun cleanup.
Nessuna fase reale o archivio viene ammesso da questa suite.

## Verifica statica effettiva e hash

Solo letture, snapshot autorizzato, patch, AST e SHA-256. Nessun import dei moduli,
test, worker, candidato, ZIP, payload, fase, Git, rete, indice o delega. F24 resta
attivo e fuori perimetro. I soli interpreti eseguiti elaborano testo/AST/hash.
Python `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`, Linux.
Modello self-report `openai/gpt-6.1-sol`; costi/token non esposti.

Comando effettivo, exit 0:
`PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S -`
con heredoc AST/hash. Verifica cinque copie, sei pin, equivalenza AST dei 18 corpi,
cinque nuovi metodi e due sole wait, entrambe con keyword timeout. Output:

```text
STATIC_ONLY: snapshots=5; preserved_pins=6; lifecycle_bodies_unchanged=18; supervisor_tests=5; waits=2_timeout_only; planned=47; executed=0
```

| File | Righe | SHA-256 |
| --- | ---: | --- |
| `test_context_lifecycle.py` | 363 | `7d68a0d37d4236d0e1324a134679c03910417bec6d517942f6c04365e14bccb5` |
| `run_suite.py` | 260 | `7b0bef90fbe061003cbc3284f13020945193b8262e3eed618944f64d95c0d504` |
| `plans/M1-fixture-context-tests.md` | 73 | `2843b1b55d4738ae9727e8cca069fd84741b089b4860c3f488e4878139b3bbe1` |

I primi due path sono relativi a `tmp/m1/fixture-store/context-lifecycle-tests/`.
La receipt PREP aggiornata registra gli stessi hash, i 47 metodi e zero esecuzioni.
Il record precedente da 139 righe conserva SHA
`b512ad208ae52971f179c8648b173aff54f05fdbef1b4f40f3e33cf4b3a7c3ac`.
I sei pin v2/storici restano quelli nella receipt e nella snapshot, senza aggiornamenti.

## Gate ancora aperti e prima azione

Test eseguiti: **0**. Nessuna prova RED/GREEN disponibile o simulata.
Restano da qualificare dinamicamente emissione materialize, flock/fdinfo e cleanup
con child veri. Le attese finite e la closure sintetica hanno solo riscontro statico.
Prima azione minima: review indipendente dei file effettivi e della receipt aggiornata.
Solo dopo review, chiusura F24 e distinta ammissione pinned dei 47 test si potrà
usare il comando futuro della PREP precedente. Nessuna esecuzione parte da questo follow-up.
