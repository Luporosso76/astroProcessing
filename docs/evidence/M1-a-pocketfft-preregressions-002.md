# M1.a pocketfft: venti preregressioni eseguite

Data: 26 ottobre 2026. Esecuzione del coordinatore GPT-6.1 Sol; review indipendente degli esiti assegnata a `tests`, non anticipata qui. Nessun override del modello degli agenti.

## Comando ed esito

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/regressions.py preregressions-002
```

Prima del lancio erano assenti sia `preregressions-002` sia l'outer `tmp/m1/fft-pocket/preregressions-turn-002`. Pin e dimensioni delle fonti sono stati confrontati con `followup-manifest-002.json`. Il turno non ha modificato codice, manifest, tolleranze o fixture storiche.

- Exit effettivo: **0**; PID diretto **760304**, raccolto.
- Risultato: **20 metodi**, `success=true`, zero failure/errori nel risultato strutturato.
- Il contenuto di tutte le fonti pinnate è rimasto identico dopo il turno.
- Ricevuta: `tmp/m1/fft-pocket/preregressions-turn-002/receipt.json`.
- SHA-256 ricevuta: `cbe9081c6ca78ea180de7e868d8e39512d8c28a28b09eb7dbf41c99ab3ff4063`.
- Risultati dettagliati: `tmp/m1/fft-pocket/preregressions-002/regression-results.json`; marker `complete.json` nella stessa directory.

## Ambiente e limiti

HOME, TMP e directory XDG del figlio sono private nel nuovo outer del progetto. Il supervisore ha verificato il budget globale prima del lancio con riserva80MiB; il candidato mantiene il proprio preflight. Guard globale1GiB, deadline120s, AS del processo256MiB e file8MiB; core dump disabilitati. Quota fisica finale osservata: **949239808 byte**, inferiore al cap. Il monitor metadata non è una quota atomica del filesystem; non è stata misurata la RSS massima.

I test usano mock di cleanup e discriminatori dell'ordine di pubblicazione; non lanciano worker FFT o compilatori. La raccolta del PID del processo suite non prova la gestione di un albero nativo reale. Nessuna build, nessuno dei232 casi numerici, benchmark, durabilità da blackout, assenza di UB o chiusura M1.a è attestata da questi venti test. I vecchi tempi FFTW non si trasferiscono a pocketfft.
