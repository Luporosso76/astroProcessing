# M1.a pocketfft: consegna PREP, nessuna qualifica eseguita

Data: 2026-10-25. Modello dichiarato dal runtime: OpenAI
`openai/gpt-6.1-sol`. Nessuna delega. Costi e token non esposti dal runtime,
quindi non stimati. Nessuna scelta irreversibile della licenza del prodotto.

## Esito effettivo

Preparati adapter C++ CPU header-only, worker isolato, supervisore/oracle stdlib,
manifest e preregistrazione separata in `plans/M1-a-pocketfft.md`. Verificati
hash e byte delle fonti upstream immutate, AST Python e corrispondenza statica
dei flag al manifest. Queste verifiche NON sono una prova numerica, una review
indipendente o un audit completo di sicurezza/licenze.

NON ESEGUITI: compilatore, incluso syntax-only C++; import di codice proprio;
worker; FFT; fixture producer; test; benchmark; build; link; inventario dei
runtime effettivi. Nessuna installazione, rete, Git, indice o sorgente di prodotto.
I soli programmi Python eseguiti sono snippet stdlib per AST, hash e ricevute.
Le funzioni preparate nel supervisore non sono state eseguite o importate.

I vecchi confronti C++/Rust con FFTW 3.3.10 restano validi e immutabili. Non
misurano pocketfft. Nessuna pretesa di tre linguaggi misurati col nuovo backend.
La presente preparazione riduce il rischio di dipendere da un'unica opzione
GPL, ma NON dimostra adottabilità. Un eventuale binding Rust resta lavoro futuro.

## Fonti lette e confine di riuso

Letture semantiche effettive, 1238 righe distinte complessive:

| File | Righe lette |
|---|---|
| `docs/evidence/M1-fft-licence-candidates.md` | 1-94, completo |
| `PLANS.md` | 1-46, completo |
| `ISTRUZIONI_OPERATIVE.md` | 1-164, completo |
| `docs/progress.md` | 1-50, completo al momento della lettura |
| `tmp/m1-fft-tools/README.md` | 55-204, 150 righe |
| `tmp/m1-fft-tools/LICENSE.md` | 1-25, completo |
| `tmp/m1-fft-tools/pocketfft_hdronly.h` | 1-199, 280-329, 3025-3109, 3340-3389, 3500-3684: 569 distinte |
| `tmp/m1/a-fault-contract-v3/validate.py` | 1-140, solo interfaccia d'ingresso e controlli |

Le letture dell'header hanno sovrapposto una volta la riga 130: 570 righe
restituite, 569 distinte. Inoltre grep ha restituito 25 occorrenze per individuare
macro, tipi e funzioni; listing di directory e metadata quota non sono letture
scientifiche di dataset. AGENTS e vincoli del task erano forniti dal runtime.
Nessuna rilettura del vecchio piano integrale o dei circa 847 MB di fixture.

L'entrypoint v3 letto importa NumPy, moduli del vecchio confronto e fixture
storiche. Non è stato riusato: non offre l'interfaccia stdlib/procedurale qui
richiesta. Nessun input stabile modificato, importato o guard aggirato.
L'implementazione nuova non condivide algoritmi con il precedente candidato,
salvo il solo header upstream verificato. Nessun sorgente Siril o FFTW acquisito.

## Pin upstream e licenze

Commit: `4d865cba24359b05b37da4e75aee24498c3b78e7`.

| Artefatto immutato | Byte | SHA-256 verificato |
|---|---:|---|
| `tmp/m1-fft-tools/pocketfft_hdronly.h` | 121769 | `518a95c552aeaee352aa30122137cd9e135d23c91e2863d9272aaa758744c86d` |
| `tmp/m1-fft-tools/LICENSE.md` | 1498 | `a85ca13fdf90160b64a0698215868c13b74d835ad0a4e2ba44713b8c5058a056` |

Gli hash rileggono tutti i byte, non attestano una lettura semantica integrale
dell'header né autenticità indipendente dell'autore. Nessuna nuova acquisizione
o verifica della firma. Conservare LICENSE BSD a tre clausole e intero preambolo
dell'header, incluse attribuzioni delle porzioni interne. Non estrarre codice
FFTW generale dalla dichiarazione upstream di permesso per una sua porzione.

GCC, libstdc++, libgcc, libm, libc e loader sono categorie da verificare sul
binario futuro, non dipendenze binarie già accertate qui. Licenze e condizioni
effettive vanno inventariate separatamente. L'header BSD non certifica quelle
licenze e non decide la licenza del prodotto, riservata al proprietario.

## Sorgenti e preregistrazione consegnati

| File nuovo | Righe | Byte | SHA-256 |
|---|---:|---:|---|
| `tmp/m1/fft-pocket/adapter.hpp` | 93 | 4548 | `089232eaca3c3d6f73655952db64ed3683ebfbf456066ad46ff893bba38ad090` |
| `tmp/m1/fft-pocket/worker.cpp` | 126 | 5440 | `4d36601333df052a452a87558e8d142a172958da0207d5c81487797b71ba6333` |
| `tmp/m1/fft-pocket/supervisor.py` | 397 | 18314 | `b0c43e78eae0f7791abe5388d9551d951da6647b0416f8d25ac7037273a6f7a7` |
| `plans/M1-a-pocketfft.md` | 205 | 11889 | `3baab2ff630dce9b3660f2bc5afc9e11531d17295b0f8d78f9e1563c93a3368a` |
| `tmp/m1/fft-pocket/preparation-manifest.json` | 129 | 3300 | `0b089419091d8940f854a7c34e7a1002f0bef451532bb9c1601e35b8e0598e6e` |
| `tmp/m1/fft-pocket/static-checks.json` | 34 | 1050 | `c8cca572b01ce827007651e40ec3d1a6dc20da60b822ea401d603e7da9e35a54` |
| `tmp/m1/fft-pocket/static-checks-final.json` | 54 | 1454 | `5fb84a6a3557e53f3f406e84e82d393a975e51ef563e459e5c1fb9f34271eaf0` |

Totale prima di questo report: 1038 righe, 45995 byte. Il primo record AST
conserva i pin precedenti al piccolo irrigidimento del conteggio quota
(propagazione errori os.walk e inclusione delle directory/symlink nei metadata).
Non è il record del sorgente finale. Il manifest di configurazione è stato
finalizzato prima di qualsiasi dato; la ricevuta finale verifica i pin attuali.
Il report stesso è escluso dal manifest per evitare un hash circolare.

Il piano è la sede del contratto scientifico e delle soglie: questa ricevuta
non le duplica. La matrice futura conta 192 finiti, 14 errori base, 24 non finiti,
due collisioni. Tutti restano PLANNED, zero eseguiti. Fixture procedurali una alla
volta, inverse da spettro analitico e tolleranza ZERO in entrambe le componenti
su tutti i bin. Rappresentabilità dei valori non garantisce esattezza della FFT.

## Verifiche statiche realmente eseguite

Ambiente: Linux x86_64 fornito dal runtime. Interprete rilevato con
`python3 -I -B -S --version`: Python 3.14.4. Il record finale conserva la stringa
completa `sys.version`; il GCC lì citato riguarda la build dell'interprete,
NON una versione del compilatore del candidato verificata in questo turno.

Comandi shell effettivi: test di assenza dei nuovi percorsi, `mkdir` con umask
077 per il namespace, `du -s -B1 tmp`, `wc -lc`, `sha256sum` sui soli sorgenti,
manifest e fonti upstream. Tutti conclusi senza errore. Nessun comando Git.

Gli snippet `python3 -I -B -S - <<'PY'` hanno letto testo e JSON, senza exec,
eval, compile di codice proprio o import del candidato. Operazioni decisive:

```python
tree = ast.parse(source_path.read_text(), filename=str(source_path))
for name, expected in manifest['sources'].items():
    assert pin(root / name) == expected, name
assert pin(root / 'tmp/m1-fft-tools/pocketfft_hdronly.h') == manifest['upstream']['header']
assert pin(root / 'tmp/m1-fft-tools/LICENSE.md') == manifest['upstream']['license']
assert set(imports) <= sys.stdlib_module_names
assert flags == manifest['recipe']['flags']
```

`imports` e `flags` derivano solo dall'AST; `ast.literal_eval` legge il letterale
lista dei flag. Esito: AST PASS, import dichiarati tutti stdlib, quattro pin
sorgente PASS, due pin upstream PASS, flag/manifest PASS. Controllo C++ solo
lettura statica: sintassi, template, link e comportamento NON verificati.

Quota allocata globale osservata prima della preparazione: 864325632 byte.
Osservazione successiva con `du -s -B1 tmp`: 866557952 byte, ancora sotto 1 GiB.
La differenza include altri autori attivi: non è attribuita a questa unità.
Nessun dataset FFT materializzato o prenotazione runtime consumata. Il futuro
supervisore rifà i guard; queste osservazioni non prenotano spazio futuro.

## Limiti e handoff

Nessuna review indipendente eseguita da questo autore. Restano aperti confine
di aliasing interno upstream con GCC, verifica effettiva dei limiti, pubblicazione
atomica, cleanup e diagnosi sotto fault. Sono codice preparato, non proprietà
dimostrate. RSS e quota periodica sono best effort, non garanzie kernel contro
picchi non osservati o scrittori concorrenti. Nessun processo figlio avviato.

Il coordinatore deve leggere file effettivi, manifest e contratto, assegnare il
turno esclusivo e chiudere review PREP prima del comando build indicato nel piano.
La validazione richiede poi inventario runtime e review della build. Nessun
benchmark prima di correttezza e sua review. Nessun epsilon retuning se ZERO
fallisce, nessuna adozione di output parziale e nessun fallback.

Scritti soltanto il namespace nuovo, il piano e questo record; nessuna modifica
a progress, matrice centrale, storico, upstream o unità di altri autori.
