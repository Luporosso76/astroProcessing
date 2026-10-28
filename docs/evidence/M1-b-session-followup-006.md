# M1.b: profilo006, inoltro opaco di XAUTHORITY

Stato: **PREP ONLY**. Nessun import di candidato, build, GUI, test, connessione,
frame o benchmark eseguiti. F24 rimane l'unico runtime. Nessuna delega o modifica
a modelli/failover, servizi, Git, indici, dipendenze o autorizzazioni desktop.
Modello autodichiarato: `openai/gpt-6.1-sol`.

## Evidenza iniziale e limiti

Letti run.py:312-463, manifest005 di 29 righe, coda della preregistrazione:223-258 e
`docs/evidence/M1-b-session-metadata-006.md` di 18 righe. Nessuna lettura del piano
baseline di 4.5k righe, altri namespace o progress.

MAIN documenta XAUTHORITY presente ma non inoltrata al figlio005, e percorso
libxcb-cursor presente. È un'omissione concreta compatibile con il fallimento
di autorizzazione, **non una dimostrazione della causa unica o del futuro successo**.
Non sono stati ripetuti i controlli ambientali di MAIN, né letti valori di
XAUTHORITY, percorsi auth, cookie o configurazioni personali in questo turno.
Nessun impedimento alla preparazione statica: il delta tratta la variabile come
stringa opaca per Qt, senza preflight del file o della credenziale.

## Snapshot precedente alla prima modifica

Manifest005 verificato su tutte le 19 fonti/preregistrazione:
`8a2b8b284c6083ef3f8dd1a8b95fe20f8439cce7a19199999f4c004ff3e958df`.

Snapshot: `tmp/m1/b-viewer/snapshots/before-session-followup-006/sources-005.zip`.
SHA-256: `94cdc990b43753f33ae38131d7b953602dce5ce010311344e91aee123ee5ad84`.
Receipt SHA-256:
`4e7162511c94d069dcb90754fc7ffc05b8afeee259a87f1367ff0b91f53c5e73`.

32 membri con byte/mode conservati: 19 fonti/preregistrazione005, manifest/diff/
check/handoff005, report preview174 righe, evidenza MAIN, FIRSTFAIL005, raw e
receipt, provenance e vecchi binari. ZIP0600 in directory0700: il raw storico
contenente la diagnosi nativa non è stato stampato, alterato o reso pubblico.

**509 voci protette conformi PRE/POST** per hash/mode/nlink o identità dei link.
Build005, DSO/host, preview005, FIRSTFAIL/raw/receipt e report174 restano immutati.
Il vecchio SIGABRT xcb resta FAIL; nessuna correzione retrospettiva o retry.
Tutti i 19 pin005 sono verificabili dal ZIP. 17 fonti correnti hanno ancora lo
stesso hash005; tutti i 19 mode sono invariati.

Protocollo padre di 169 righe invariato:
`1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.

## Delta minimo effettivo

`tmp/m1/b-viewer/run.py`: **cinque righe aggiunte**, nessuna rimossa.

- Righe 411-413: predicato Qt+xcb e STOP diagnostico se XAUTHORITY è assente o
  vuota. Il guard precede creazione ICC e qualsiasi figlio della fase preview.
- Righe 433-434: nel solo ramo GUI Qt+xcb, inoltro da `os.environ` al dizionario
  `env` passato a Popen. Il valore non entra in argv, manifest o receipt.

Il codice non apre/stat/risolve il percorso auth e non legge cookie. Non usa
xauth/xhost, non copia credenziali e non modifica permessi. Solo Qt può usare
la sessione esistente tramite la normale autenticazione desktop.
Parser ICC separato, build/oracle, GTK e Qt Wayland non ricevono questa nuova
variabile. Non viene selezionata alcuna piattaforma alternativa. HOME/TMP/XDG/
cache privati restano identici. Un wrapper futuro deve conservare XAUTHORITY
nel solo ambiente del supervisore preview, senza registrarne il valore.

`plans/M1-b-viewer.md`: solo append del profilo006 e comandi futuri; prefisso005
di 258 righe byte-identico. README non modificato: la preregistrazione basta.
Tutte le altre funzioni Python conservano lo stesso AST. Nessuna modifica a
watch/limiti/nofallback, core/ABI2/kernel, C++/HPP/QML, oracle/GT Fraction,
cache/ICC, paint/export/cleanup, pan o A11y.

Il nuovo guard non dimostra presenza/accessibilità/validità del file auth né
successo della connessione. Queste proprietà non sono state indagate qui.
Il relativo esito runtime richiederà un turno ammesso; nessun segreto va letto
dal coordinatore per farlo passare. Se manca la variabile, il run si ferma.

## Controlli eseguiti e pin006

Solo 8 AST Python SYNTAX_OK, 4 blocchi bash con `bash --noprofile --norc -n`,
confronti esatti del delta e hash/mode/snapshot. Nessun modulo dello spike
importato e nessuna simulazione eseguita del guard XAUTHORITY.
Runtime dei controlli: Python
`3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`.
Nessuna quota globale ricampionata nello slot F24; i confronti restano nel solo
perimetro autorizzato. Il numero di versione Python non è una nuova prova GCC.

Artefatti sotto `tmp/m1/b-viewer/`:

- `source-hashes-006.json`, 19 pin:
  `ab9994ebc5c73325bac8ba0331d2a8d26fc870fa3f0d98179ff7127ceabbdb7d`.
- `source-diff-006.patch`, 67 righe su run.py e piano:
  `452f242726c27f6147e7bacb63f8de2bb36964b1bac73016c346fd082a5becc2`.
- `preparation-checks-006.json`:
  `f43c3ade8fff6bf00fc118472eb97d7d8f0425938fb77edec3cc88e7e7cd0c8e`.

Build-006, runs/build-006 e runs/widgets-first-frame-xcb-dpr1-006 assenti,
anche come symlink. Nessun nuovo binario o output runtime prodotto.

## Ordine e comandi FUTURI, non eseguiti

Prima review indipendente del delta006; poi ammissione BUILD006 separata.
Anche se cambia solo l'ambiente launcher, serve una build fresca per la nuova
provenance: non adottare build005. Dalla radice `/opt/workspace/astroProcessor`:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase build --build-id build-006 --run-id build-006 --exclusive-admitted
```

Solo se la DSO fresca è bit-identica004
`7f76d4d04ae5a0855dc38dd53d6a4349520fe0171eba2636391ca97c12e00f0a`:
riferimento esplicito oracle004 per quel solo binario core, non per il nuovo host.
DSO diversa: STOP e nuova review, nessuna adozione o oracle automatico.
Poi nuova ammissione preview006, pin/quota e ambiente privato; XAUTHORITY
ereditata opaca, senza valori nel comando o nei log ambientali:

```bash
/usr/bin/python3 -I -B -S tmp/m1/b-viewer/run.py --phase preview --build-id build-006 --run-id widgets-first-frame-xcb-dpr1-006 --candidate qtwidgets-raster --platform xcb --dpr 1 --qt-scale-factor 1 --preview-mode first-frame-export-exit --core-oracle-reference oracle-004 --fault none --exclusive-admitted
```

Ricetta e limiti005 invariati. Nessun oracle006 dichiarato o avvio automatico.
Xcb rimane UNQUALIFIED rispetto a X11/XWayland; renderer interno antifallback
INCOMPLETE, GUI/frame/export/cleanup, colore/A11y/performance non provati da
questo delta. La consegna è statica, non READY runtime o PASS GUI.
