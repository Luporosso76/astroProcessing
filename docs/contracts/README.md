# Contratti minimi M2

Parti degli ADR usate da M2.a–e. Errori: modello comune in ADR-05. ID di test `T-M2<unità>-NN`: etichette di lavoro, da riportare in `docs/test-matrix.md` quando i test esistono.

- [ADR-03-pixel-maschere.md](ADR-03-pixel-maschere.md): immagine tipizzata, bit di invalidità, metadati, lettore e scrittore FITS con limiti (M2.a); estensione ADR-03.b: origine `decoder` e stato del decoder RAW, RGB da debayer con classe del campione e portata della varianza, coverage, bit `support_incomplete`, maschere fotografiche distinte, alpha derivato per l'export, capability di misura, righe della calibrazione `.b` nella tabella delle operazioni (M3).
- [ADR-04-persistenza.md](ADR-04-persistenza.md): store SQLite, scrittura atomica, recovery, schema ignoto, migrazione con backup, snapshot, undo e redo, export con manifest (M2.c); estensione R4: report di recovery persistito, fase sicura, workflow e resume, report di export, regola hardlink, modello di perdita di energia.
- [ADR-05-dag-provenance.md](ADR-05-dag-provenance.md): modello d'errore comune, identità degli input, nodo e chiave, generazione, provenance (M2.b).
- [ADR-06-budget-scheduler.md](ADR-06-budget-scheduler.md): budget automatico e manuale, pool, job, cancel, checkpoint, spill (M2.d).
- [ADR-13-offline.md](ADR-13-offline.md): core senza API di rete, CFITSIO solo da memoria, verifica su simboli e syscall (M2.f).
- [ADR-16-stati-master.md](ADR-16-stati-master.md): stati dei master, compatibilità, ricetta `L − D`, `Q`, maschere, ledger e report (M2.e); estensione ADR-16.b: analisi G-CAL, otto combinazioni con gli assi dark-flat e flat, catene e default (D-16), P-LOST, stato del decoder nei master da RAW (M3).
- [ADR-17-varianza.md](ADR-17-varianza.md): sorgenti di varianza e propagazione al primo ordine (M2.e); estensione ADR-17.b: sensibilità sulle sorgenti, covarianza light/flat e bias in due catene, master comuni fra light, rappresentazione P0 (diagonale e blocco fra canali memorizzati, fuori diagonale per riferimento), resampling `R C Rᵀ`, combinazione di frame, Jacobiano del non lineare, ingressi `marginal`, offset del modello di rumore per i RAW (M3).
- [C-02-master.md](C-02-master.md): generazione dei master, slice minima: metodi, rejection, normalizzazione del flat, varianza per metodo, tolleranze di R2 (C-02.a); estensione C-02.b: analisi del set candidato con esclusioni e gruppi, media pesata, mediana, winsorized e `sigma_clip` con il piano di varianza di ciascuno, proposta a priori secondo `n`, flat CFA per canale, report di frame usati e scartati (M3); estensione C-02.b-flat: normalizzazione `median_ratio` dei flat per canale con riferimento armonico e metodi robusti sui flat normalizzati, con il fattore globale dimostrato o sotto ipotesi dichiarate (M3).
- [ADR-07-cpu-simd.md](ADR-07-cpu-simd.md): percorsi `cpu_scalar` e `cpu_avx2`, dispatch nel preflight, ambiente in virgola mobile, tolleranze di R5 (ADR-07.a).

Contratti di M3.entry (ID di test `T-M3<slice>-<tema>-NN`):

- [ADR-08-decoder-pack.md](ADR-08-decoder-pack.md): decoder RAW con LibRaw da memoria (§9.6), catalog pack (schema, manifest, G-PACK, pack mancante, corrotto o fuori copertura), cataloghi scelti, regola NFR-LIC-01.d (ADR-08.b).
- [ADR-09-algoritmi.md](ADR-09-algoritmi.md): algoritmi P0 (ADR-09.b): una sola formulazione P0 per famiglia di M3, regola delle soglie del campo 9 (classi B, E, D, S, P, R, C; D-09.b), verifica dei campi 9 di C-01.b e C-02.b, normalizzazione e metodi robusti dei flat assegnati a F-CAL-01.b; ADR-09.a resta nella parte `.a` di ADR-16.
- [C-01-calibrazione-light.md](C-01-calibrazione-light.md): calibrazione del light, forma completa (C-01.b): nero del decoder mai sottratto e usato come sentinella, voce `light_offset` e residuo dichiarato del percorso parziale (G-OFFSET), dominio del light, coverage attraverso la calibrazione, metriche e test; C-01.a resta nella parte `.a` di ADR-16.

Base comune di M3 (ID di test `T-M3BASE-NN`):

- [BASE-confini.md](BASE-confini.md): obblighi trasversali dei job `OB-01`…`OB-19` (arresto e provenance, ambiente FP, cancel, budget e pool, identità del nodo, verifica e sola lettura degli input, nessun fallback, nessuna pubblicazione su errore, checkpoint e resume, temporanei, offline), ciascuno con un solo confine proprietario e lo stato nel codice; suite di conformità sul registro dei tipi di job; debiti.

Contratti delle slice di M3 (ID di test `T-M3<slice>-NN`):

- [F-IO-03-classificazione.md](F-IO-03-classificazione.md): classificazione batch (M3.a): proposta da `IMAGETYP` e stato «da classificare» per i RAW, revisioni append-only con la correzione dell'utente che prevale, gruppi di light, scelta fra più candidati dello stesso ruolo, proposta di sole ricette ammissibili, binning e filtro espliciti, ripartizione del mismatch dopo la scelta fra M3.a, M3.b e M3.c (`CL-01`…`CL-12`).
- [F-IO-08-ricollegamento.md](F-IO-08-ricollegamento.md): originali mancanti, spostati e ricollegamento (M3.a), estensione di ADR-04: stato degli originali solo sui percorsi registrati e solo a richiesta, nessuna ricerca né ricollegamento automatico, verifica del candidato per dimensione e SHA-256 nel confine, posizione corrente unica con storia append-only, slot bloccati con motivo e sblocco senza ricalcolo, undo, redo e recovery invariati (`RL-01`…`RL-08`).
