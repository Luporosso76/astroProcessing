# M1.a pocketfft: preregressioni 003 non avviate

## Esito

STOP_NOT_RUN. Nessun import di codice del progetto, mock, compilatore, worker o
FFT eseguito. Il processo candidato non è stato creato: nessun PID candidato da
raccogliere e slot libero. Nessun PASS36, ricevuta complete o pin post-candidato.
Il turno non ammette una build.

Il record separato è `tmp/m1/fft-pocket/authentication-stop-003.json`.
Le chiamate tool della sessione conservano i comandi inline completi e i loro
output. Non esiste un driver completo salvato su disco: il suo salvataggio era
previsto dopo l'autenticazione che si è fermata.

## Due errori del verificatore, distinti dai sorgenti

Il primo tentativo aveva cercato una copia dello snapshot usando la chiave
originale invece del campo `snapshot_file`. Lo stop precedente riportava:

```text
FileNotFoundError: /opt/workspace/astroProcessor/tmp/m1/fft-pocket/snapshots/pre-followup-001/docs
```

Il coordinatore ha autorizzato la ripresa con i tre layout espliciti. La prima
autenticazione della ripresa ha concluso con exit 0 e
`AUTHENTICATION_PREFLIGHT_PASS_NO_CANDIDATE_LAUNCHED`: 97 percorsi autenticati e
8/22/29 copie verificate nei tre snapshot. Il controllo ha usato stdlib,
apertura relativa tramite directory descriptor con O_NOFOLLOW, confronto di
byte/SHA-256 e stat prima/dopo lettura. Nessun import di sorgenti del progetto.
Ha incluso frozen, upstream, sorgenti correnti e 26 file storici invariati del
pre-build-budget, comprese le ricevute old20 e il FIRSTFAIL della build.

I pin corretti verificati in quella chiamata erano:

```text
static-build-budget-003.json
a672e61f81dee288c88a14f811d49e633c20d33ff6ac3239bcebd5fc44be25fa
followup-manifest-003.json, 6720 byte
9e66b7ed388813da15d0abe7cb04ce6f704af02a64f29640ad6639523c807a75
```

Il secondo comando, destinato ad autenticare nuovamente e poi lanciare il
candidato, conteneva invece questa costante trascritta male nel verificatore:

```text
9e66b7ed388813da15d0abe7cb04ce6f70402a64f29640ad6639523c807a75
```

Si è fermato con exit 1 prima di mkdir, import del progetto o Popen:

```text
AssertionError: tmp/m1/fft-pocket/followup-manifest-003.json
```

Questo errore non dimostra una modifica del manifest. La costante errata
apparteneva al verificatore inline, non ai file del candidato. Dopo questo
secondo inatteso non sono stati eseguiti correzione, retry o nuovo controllo.
Nessun sorgente, piano o manifest è stato modificato.

## Comandi e preflight realmente eseguiti

Entrambe le chiamate Python della ripresa usavano, dalla root del progetto:

```sh
/usr/bin/python3 -I -B -S - <<'PY'
```

La prima eseguiva autenticazione e preflight stdlib; la seconda eseguiva il
driver inline mediante `exec(compile(body,'<preregressions-turn-003-driver>','exec'))`.
Il comando candidato previsto nel driver, **non eseguito**, era:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/regressions.py preregressions-003
```

La prima chiamata ha verificato l'assenza dei namespace `preregressions-003`
e `preregressions-turn-003`. Il secondo comando si è fermato prima di crearli.
HOME/cache/TMP privati e raccolta bounded erano predisposti nel testo del driver,
ma non sono stati creati né esercitati. Nessuna attestazione di cleanup reale o
di comportamento OS viene ricavata dai mock, che non sono partiti.

Misure di preflight, non misure del candidato:

- tmp globale: 956923904 byte allocati, 5380 inode distinti; sola lettura dei
  metadati, hardlink deduplicati, symlink non seguiti.
- Quota 1073741824 byte, riserva 67108864, margine 16777216; residuo 32931840 byte.
- Spazio filesystem disponibile: 125336424448 byte.
- MemAvailable: 18987094016 byte.
- Hard ereditati AS/RSS/FSIZE/CORE tutti illimitati; soft CORE 0.
  Nessun setrlimit candidato eseguito.
- SIGCHLD default verificato. Conteggio AST: Cleanup 13, DirectoryBarriers 7,
  LimitBudgets 16. Sono 36 metodi pianificati, zero eseguiti.

Non esistono conteggi runtime di skip/errori attesi o risultati di 36 metodi:
non si converte NOT_RUN in PASS. Nessun figlio candidato è stato avviato.

## Ambiente e review

Python osservato: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`,
eseguibile `/usr/bin/python3`. Linux `7.0.0-38-generic`, macchina `x86_64`.

Il coordinatore ha riferito READY_STATIC36 dalla review indipendente
`ses_e8861dc34ffeJhiLMCzYYtZN9h`. L'ambito riferito è testuale/statico, non una
attestazione SHA indipendente né un'esecuzione dei mock. L'autenticazione
byte/stat descritta sopra è separata ed eseguita da questo agente.
Il sottocaso facoltativo getrlimit post-set fallito non è stato aggiunto.

Modello dichiarato dall'ambiente: GPT-6.1 Sol, `openai/gpt-6.1-sol`.
Nessun override, delega, Git, rete, installazione o indice. Nessuna modifica a
progress/index. I nuovi file di questo turno documentano soltanto lo stop.
