# Pocketfft: primo tentativo di compilazione

26 ottobre 2026. **FIRSTFAIL_STOP**, non qualifica numerica o di prestazioni.
Esecuzione del coordinatore, successiva alle venti preregressioni confermate in
`M1-a-pocketfft-preregressions-002-review.md`. Nessun altro candidato in esecuzione;
gli autori dei namespace viewer e confronto hash lavoravano soltanto in PREP.

## Comando e risultato

```bash
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/supervisor.py build build-followup-001 preregressions-002
```

Supervisore PID845929 raccolto, exit2; compilatore exit1. Diagnosi conservata:

```text
virtual memory exhausted: Cannot allocate memory
```

Il supervisore imposta `RLIMIT_AS=256MiB` anche per la compilazione, oltre a
RSS512MiB best effort e ai controlli scratch. La diagnosi è compatibile con quel
limite di spazio virtuale; non misura il fabbisogno reale del compilatore, non
dimostra OOM di sistema e non è un errore numerico pocketfft. Il campione RSS
3059712 byte nel risultato non certifica il picco dell'intero albero del compilatore.

Nessun worker prodotto, nessuna FFT, nessuna modifica di fonti, tolleranze o casi.
Nessun retry o altro backend. Restano futuri tutti i232 casi di qualifica.

## Provenienza e controlli

- Supervisore byte-pinned prima/dopo:
  `977f3704e9a168f8246495f8c12dbff273c665ab562dc2a70b2b62e18ce8f366`.
- Preregistrazione169 byte-identica:
  `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.
- Scratch fisico iniziale954667008 byte, più64MiB riservati e16MiB di margine
  inferiore al cap invariato1073741824 byte. Fotografia non atomica, non quota OS.
- HOME/TMP/XDG privati nel progetto, nessuna rete o installazione. Comando GCC,
  profilo e risultato sono in `build-followup-001/manifest-before-build.json` e
  `build-result.json`; primo errore in `FIRSTFAIL.json`.
- Ricevuta esterna:
  `tmp/m1/fft-pocket/build-turn-followup-001/execution.json`;
  stderr SHA `2e72cb48345032a005672ae38c834ebb32d714b31f05d75ea32d2876d4681393`,
  stdout vuoto. Durata esterna2062697834ns: diagnostica, non benchmark.

Prossimo passo: esaminare separatamente i limiti tecnici della fase compilazione
e quelli del candidato. Un eventuale nuovo profilo va dichiarato e revisionato
prima del prossimo tentativo; nessuna evidenza precedente diventa un PASS.
