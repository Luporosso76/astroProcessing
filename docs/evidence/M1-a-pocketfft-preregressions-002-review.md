# M1.a pocketfft: review delle venti preregressioni

Data: 26 ottobre 2026. Review indipendente dell'agente `tests`, sessione `ses_e88f11b0affewIIvJMATMUr1fm`; verbale persistito dal coordinatore. Nessun override del modello. Il risultato della delega non espone un'identità di modello verificabile o costi/token: non vengono inferiti.

**PASS limitato al turno preregressioni.** Il revisore ha verificato i sette artefatti del turno richiesti e `followup-manifest-002.json`, senza importare candidati o rieseguire test.

- Hash e dimensioni delle cinque sorgenti coincidono con manifest e pin precedenti all'esecuzione.
- `receipt.result` coincide con `regression-results.json`; `complete.json` lega risultati e manifest.
- Log completo: venti test distinti, tutti `ok`, zero skip, errori o failure; tredici test cleanup e sette barriere/pubblicazione.
- Receipt: exit0, PID760304 dichiarato raccolto, nessuno stop; stdout e stderr vuoti con hash corrispondenti.

| Artefatto | SHA-256 verificato |
| --- | --- |
| Receipt | `cbe9081c6ca78ea180de7e868d8e39512d8c28a28b09eb7dbf41c99ab3ff4063` |
| Manifest | `76784fa8e727d254411295f534150ee93f54521776cadb050fd0cad75642494a` |
| Risultati | `96f0596b15141bc0e1e00345989793f33924f174d69a87f153c70fe764739091` |

`real_children: 0` è dichiarato dal runner, non un censimento indipendente. La review del ciclo PID poggia su mock e precedente analisi statica; il revisore non ha riosservato il processo. Il manifest conserva il precedente stato di preparazione; gli artefatti successivi attestano questo turno, senza riscrivere lo storico.

La prossima fase eleggibile è una build separatamente ammessa. Restano futuri i232 casi FFT, la gestione di un albero nativo reale, durabilità da blackout e qualificazione prestazionale. Nessuna chiusura complessiva M1 o adozione di backend.
