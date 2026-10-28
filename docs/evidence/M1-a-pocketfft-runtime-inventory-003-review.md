# M1.a pocketfft: review dell'inventario runtime 003

## Esito e attribuzione

Esito: PARZIALE. La review indipendente della sessione
`ses_e8828fcf4ffeiTrPzgs5tRZ8md` è conclusa: conferma la coerenza manuale
dell'inventario statico, con un rilievo MED e uno LOW aperti.
Questo record trascrive la consegna del coordinatore; non è una nuova review
dei raw né una nuova esecuzione del verificatore.

- Unità: M1.a, dipendenza pocketfft.
- Scopo: distinguere coerenza statica, copertura del verificatore e autenticazione dei record.
- Comando: review documentale senza comando di esecuzione. Il coordinatore riferisce
  una successiva verifica con `/usr/bin/python3 -I -B -S -`; il corpo stdin
  completo non è disponibile nella consegna, quindi la sola riga non permette di ripeterla.
- Input: report `M1-a-pocketfft-runtime-inventory-003.md` e artefatti del namespace
  `tmp/m1/fft-pocket/runtime-inventory-003/`; sei SHA-256 nella tabella seguente.
- Versioni: per l'acquisizione originaria il report dichiara Python 3.14.4,
  readelf 2.46, ldconfig/glibc 2.43 e dpkg-query 1.23.7. Versioni della verifica
  successiva del coordinatore non disponibili.
- Ambiente: progetto `/opt/workspace/astroProcessor`; il report originario dichiara
  Linux `7.0.0-38-generic`, x86_64. Ulteriori dati dell'ambiente della review
  e della verifica successiva non disponibili.
- Risultato: coerenza manuale confermata; autenticazione successiva PASS per sei
  record, riferita dal coordinatore. Codice di uscita della verifica non disponibile.
- Output: questo record; percorso di stdout/stderr della verifica del coordinatore non disponibile.
- Ambito: inventario statico e autenticazione circoscritta; esclusi relocation,
  live binding, qualifica numerica e conformità distributiva.
- Limiti: rilievi e separazione delle responsabilità descritti sotto.
- Autore e data: redazione documentale, 26 ottobre 2026. Modello dichiarato al
  redattore dal runtime: `openai/gpt-6-astra`; nessun override. Modelli effettivi
  di revisore e coordinatore non disponibili nella consegna; costi non disponibili.

## Review indipendente

Il revisore ha riscontrato coerenza manuale per cinque componenti runtime,
sei ELF incluso il worker e 13 archi NEEDED. Ha confrontato PT_INTERP,
cache e nodi di versione. Il loader ha sette definizioni di versione:
le sonde STAP non sono definizioni di versione. Il confronto resta statico;
non verifica le singole relocation né il binding di un processo avviato.

### MED: copertura del verificatore

Secondo la review, `verify.py:128-140` non ricostruisce indipendentemente dai
raw gli archi, i requisiti e i confronti MD5. I 379 controlli comprendono
132 controlli sui metadati protetti: non sono 379 verifiche indipendenti
della chiusura runtime. Mancano negativi per omissioni. Il controllo manuale
non ha rilevato omissioni concrete, ma non chiude il limite del verificatore.

### LOW: conservazione della prima diagnosi

`verification-attempt-001-failure.json` contiene una diagnosi trascritta,
non lo stderr raw. La prima esecuzione fallita resta documentata, ma non
si può dichiarare conservazione integrale della sua diagnostica.

## Autenticazione successiva del coordinatore

Il revisore non disponeva di strumenti hash/stat: gli hash erano soltanto
dichiarati nel suo perimetro. Successivamente il coordinatore ha riferito
PASS su sei record usando sola stdlib, letture relative a descrittori con
NOFOLLOW, file regolari di proprietà dell'utente, nlink 1, dimensione inferiore
a 1 MiB, fstat stabile e SHA-256.

I percorsi in tabella sono relativi a `tmp/m1/fft-pocket/runtime-inventory-003/`,
tranne il report indicato dalla radice del progetto. Gli hash sono i risultati
comunicati dal coordinatore, non ricalcolati dal redattore.

| Record | SHA-256 verificato dal coordinatore |
|---|---|
| `handoff.json` | `aaf3f34ff24de5c49d273d20b6bb05be4e2d6d9901207c3ccf118c9bae5538d0` |
| `docs/evidence/M1-a-pocketfft-runtime-inventory-003.md` | `5b392f4c9ecd3951a1100702d53fbad7345a6df1cce40a08038acc52c4618c51` |
| `artifact-manifest.json` | `6732d90ebef708d6fb1e3e4fba1b559b540fda907b9cbfcd38ca25b5a61316ef` |
| `verification-002/inventory-verified.json` | `48d47c8f77ba18b6644dcf7157fc23e37c0879f615d5751afa9781c8073cb901` |
| `verification-002/verification.json` | `98824c16b2dc2fcb66e5b51f2614c8ef186b2f53235dd845ad4d043b01545b44` |
| `final-checks.json` | `6cbf3cf52ffe9b10b30c9ae940faa789dff24e934410b6999ec353d3339f3e0a` |

Questa verifica chiude soltanto l'autenticazione dei sei record. Non
riautentica i runtime host o tutta la storia e non chiude il rilievo MED.
Il report e gli artefatti 003 restano immutati; il loro PENDING descrive
la consegna precedente a questa review.

## Limiti e condizioni di una futura validazione

Non sono stati eseguiti worker, import di candidati, test o build in questa
attività documentale. Le licenze raccolte non dimostrano conformità distributiva
né provenienza a granularità di file. Adozione e licenza del prodotto restano aperte.

Una futura validazione locale richiede verifica dell'identità runtime corrente,
dell'ambiente, del budget e dei prerequisiti, oltre a una modalità esplicita
di osservazione del live binding. Non può trasformare retroattivamente
l'inventario statico in un PASS runtime. Stato operativo e prossima azione
sono in `../progress.md`.

Letture del redattore: `docs/progress.md` 55 righe,
`docs/evidence/README.md` 155 righe, report originario 264 righe e
`tmp/m1/fft-pocket/runtime-inventory-003/handoff.json` 120 righe.
Il namespace 004 in scrittura non è stato letto.
