# M1: manutenzione dello storage sintetico

Esito eseguito: **STORAGE_PASS**, exit 0. Non è un benchmark né una modifica al
codice di prodotto. Autorizzazione: incarico in chat del coordinatore, con
approvazione architetturale indipendente della copia sparse verificata. Il cap
fisico resta 1 GiB; nessuna fixture o serie viene rimossa o ridimensionata.

## Comandi effettivi

Dalla radice `/opt/workspace/astroProcessor`:

```bash
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1-maintenance/sparse_copy.py --run-id inventory-001
PYTHONDONTWRITEBYTECODE=1 OPENSSL_CONF=/dev/null /usr/bin/python3 -I -B -S tmp/m1-maintenance/sparse_copy.py --apply --run-id apply-001
```

Il primo comando termina con `INVENTORY_PASS`, exit 0, senza conversioni. Il
secondo termina con `STORAGE_PASS`, exit 0, dopo 53 conversioni. Entrambi hanno
riletto e confrontato con i manifest tutti i 168 SHA finali autorizzati.
Runtime Python, piattaforma Linux e UID 1000 sono registrati nei report; nessuna
installazione, rete, modifica di sistema, Git o processo estraneo terminato.
Modello dichiarato dal runtime dell'autore: `openai/gpt-6.1-sol`.

Una ricognizione precedente, senza modifiche alle fixture, era terminata con
`KeyError: 'inverse_source_sha256'`: quel campo non esiste nei casi. Lo script
usa invece il SHA completo già presente per lo stesso percorso `input`/`gt`.
Le 48 referenze `inverse_source` appartengono tutte alla stessa allowlist.
Non si attribuisce a quella ricognizione l'esito di una conversione.

## Integrità e perimetro

Allowlist chiusa: soltanto `input`, `gt`, `inverse_source` dei due
`tmp/m1/a/series-{001,002}/cases.json`, con digest coerente e parent esattamente
`fixtures/` di una delle due serie. Sono 168 percorsi unici. Ogni componente di
directory è aperto fd-relative con `O_NOFOLLOW`; ogni file deve essere regolare
e avere `nlink == 1`. Nessun dato utente o fixture di altro spike è convertito.

Inventario e SHA precedono le conversioni; l'ordine privilegia il maggior
risparmio misurato. Una copia per volta, sibling `O_EXCL/O_NOFOLLOW`, stesso
filesystem. Solo chunk composti interamente da byte `0x00` diventano seek e
`ftruncate`: nessun confronto float, quindi nessuna normalizzazione di `-0`.
Tutti i chunk non nulli usano write-all. Ogni copia supera SHA, dimensione e
confronto byte per byte prima di `os.replace`; seguono fsync directory e nuovo
SHA del percorso definitivo riaperto.

Per tutte le 53 conversioni risultano invariati size, mode, uid/gid, atime e
mtime nanosecondi, e mappa SHA degli xattr. Tutti gli inode e ctime sono cambiati,
come registrato per file. Birthtime non è esposto da questo runtime. Il controllo
xattr/ACL non ignora errori: in queste 168 fixture non sono presenti xattr o ACL
estese; il caso ACL non vuota non è stato eseguito.

Prima di replace, un errore lascia il percorso originale non sostituito e può
lasciare il sibling diagnostico. Dopo replace e prima di fsync directory,
l'esito a power-loss non è dimostrato. Nessuna conversione alternativa o
riparazione automatica. Non sono stati iniettati crash durante la manutenzione.

## Allocazione fisica

Metrica: somma radicata in `tmp/` di `st_blocks * 512`, con inode unici
`(st_dev, st_ino)`; symlink conteggiati senza seguirne il bersaglio.

| Quantità, apply-001 | Byte |
| --- | ---: |
| Allocazione prima | 1 015 832 576 |
| Allocazione dopo, prima del report finale | 730 882 048 |
| Risparmio delle sole fixture convertite | 285 192 192 |
| Dimensione logica delle 168 fixture, prima e dopo | 847 288 776 |
| Picco radicato osservato | 1 024 385 024 |
| Cap invariato | 1 073 741 824 |

Eseguiti 690 controlli di allocazione. Prima di ogni copia si prenota il suo
worst case interamente allocato, arrotondato al fragment size, più 5 MiB per
metadati/log e attività indipendenti. Controlli intermedi ogni MiB attraversato,
poi dopo fsync e replace. La scansione condivisa è osservazionale: non impedisce
scritture di altri autori. Le differenze fra delta globale e risparmio delle
fixture includono report e attività indipendenti, non compressione dei dati.

## Continuità delle serie precedenti

I 127 file stabili rilevanti, inclusi report delle due serie, manifest, sorgenti
stabili `tmp/m1/src/`, binari `tmp/m1/build/`, ambiente e protocollo, hanno lo
stesso SHA prima e dopo. Anche l'insieme dei loro percorsi resta identico.
Nessun vecchio report, source hash, campione o timing viene riscritto.

La serie 002 corrisponde a tutti i suoi SHA storici di sorgenti/binari/protocollo.
La serie 001 aveva già sette differenze rispetto ai file live: protocollo,
binari C++/Rust e quattro sorgenti aggiornati nella serie successiva. Restano
documentate, non corrette o rietichettate. I sei sorgenti conservati nel suo ZIP
sono verificati contro i rispettivi SHA storici; lo ZIP stesso resta invariato.
Il vecchio protocollo e i vecchi binari non sono contenuti in quello ZIP:
questa manutenzione non ne inventa una ricostruzione.

## Artefatti e hash

Tutti gli hash sotto sono SHA-256:

- `tmp/m1-maintenance/sparse_copy.py`:
  `fac082645fc01e2370b52cb439be5899f4aee601b329559c4e1d302e1b45d15d`.
- `tmp/m1-maintenance/apply-001/report.json`:
  `bb725b34c38a70c17c97714d5505bdbcc25fa76fb10f2cba6070d1f04408d2cc`.
- `tmp/m1-maintenance/apply-001/events.jsonl`:
  `56ce66b28b656d4894f6a2f97648a49370afada5e21ab1902f1a8882067a91bc`.
- `tmp/m1-maintenance/apply-001/inventory.json`:
  `2e5e0399b3d60c27b2d7db005107d37672c75cf571306dea3571aa07fe457706`.
- Protocollo corrente `plans/M1-spikes.md`:
  `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.

Copertura delle letture: AGENTS 52 righe; protocollo comune 24 righe; README c
239 righe; due manifest sorgenti da 11 righe; stato serie 001 da 12 righe.
L'harness di manutenzione legge integralmente i due casi JSON, rispettivamente
1742 e 2174 righe, i 168 file allowlist e i 127 file di continuità. I sorgenti
attivi ownership/d/fault non sono letti; la contabilità ne usa solo metadati
filesystem. Script di manutenzione: 443 righe, AST verificato prima dell'uso.

## Limiti e passo successivo

L'identità di contenuto non dimostra equivalenza delle prestazioni I/O.
Le letture hanno scaldato la cache; le fixture convertite sono ora sparse.
Qualunque misura futura deve dichiararlo e aprire la propria serie appropriata.
Lo spill M1.d da 256 MiB dovrà essere fisicamente pieno, mai sparse.

La validazione M1.c può partire soltanto dopo un nuovo controllo del budget di
192 MiB con cap invariato. Esito e tentativi sono registrati separatamente in
`M1-c-validation.md`. Nessun benchmark comparativo o fase measure è autorizzato
da questo record.
