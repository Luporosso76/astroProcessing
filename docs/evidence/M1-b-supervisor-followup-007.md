# M1.b: preparazione007 del supervisore

Stato della consegna: **STABLE_PREP_ONLY, review indipendente PENDING**.
Nessun PASS runtime o autoapprovazione. Modello dichiarato dal contesto:
`openai/gpt-6.1-sol`, senza override di modello o failover.

## Ripresa verificata

La delega precedente è terminata con `getaddrinfo EAI_AGAIN api.openai.com`.
Non era attiva. Aveva modificato parzialmente `run.py` e predisposto
`supervisor_regressions.py`; manifest007 e questo report erano assenti.
Hash e dimensioni iniziali sono registrati in `preparation-checks-007.json`.

Verificati i 19 pin006 contro lo snapshot preesistente
`snapshots/before-supervisor-followup-007/sources-006.zip`: 33 membri conformi
per hash e mode. Le 551 voci protette dal receipt restano conformi.
FIRSTFAIL006/005, sorgenti scientifici, protocollo169 e baseline sono invariati.
Percorsi e conteggi delle letture iniziali sono nel record dei controlli.

## Delta preparato

Le letture children/status dei soli discendenti registrano ENOENT/ESRCH come
osservazioni perse. Errori del leader, EACCES e altri errno restano fatali.
RSS mancante produce `None`/JSON null, non zero. Anche lo zombie senza RSS
rimane un'osservazione mancante. Il receipt esplicita dati assenti o incompleti;
un valore parziale non dimostra il rispetto del cap RSS.

La raccolta conserva own Popen e WNOWAIT, verifica ownership e SID/PGID prima
del segnale, poi esegue wait sul proprio figlio. Ownership persa o metadati
negati impediscono il segnale. Errori di cleanup producono STOP e diagnosi
separata, senza attestare reaping riuscito o assenza di residui.

Il raw006 non contiene traceback: questa preparazione non dimostra quale
lettura abbia prodotto l'ESRCH storico. Nessun fallback o limite cambia.

## Controlli effettivi

Ambiente: Linux x86_64, Python 3.14.4, GCC 15.2.0 del relativo runtime Python.
Comando eseguito dalla radice:

```bash
/usr/bin/python3 -I -B -S - < tmp/m1/b-viewer/prepare-static-007.txt
```

Solo stdlib, AST/hash/diff; nessun import del progetto. Nove AST validi;
sei funzioni storiche di `run.py`, incluso `main`, mantengono AST identico.
Il piano conserva integralmente il prefisso006. Preparati **27 test mock**,
compresi receipt e cleanup: **NOT_RUN**, non validati dall'AST.
Directory runtime007 assenti anche come symlink. Nessuna build, GUI, suite,
misura, rete, installazione, Git, indice, credenziale o delega.

## Pin della consegna

Percorsi relativi a `tmp/m1/b-viewer/`, SHA-256:

```text
run.py                         d6f9b22c6044c3e518f116eed2a640e1d384ddd1bd100e13b3c36664ab7fb949
supervisor_regressions.py       08fd3995023ed9f224a1211b90e43ae5d58f068874f6dda2bf4ffb8bf0bff7e0
source-hashes-007.json          5ce8b07c9e04de58b4d3cf305b25018f7e8bf0cd69908684731825fadff68a3d
source-diff-007.patch           0bfe25dbe4887af70a383e7f32ab829367ed30a59f1f32258634a319943028b1
preparation-checks-007.json     a3780c5b48839a6353661c276d4abaf0431cbf17a6e503a1dcb44161b903722f
```

`handoff-007.json` lega anche questo report. Prossimo passo: review indipendente
del diff effettivo. Test e build richiedono turni distinti successivamente
ammessi; nessun comando runtime viene avviato automaticamente.
