# Contratti minimi M2

Parti degli ADR usate da M2.a–e. Errori: modello comune in ADR-05. ID di test `T-M2<unità>-NN`: etichette di lavoro, da riportare in `docs/test-matrix.md` quando i test esistono.

- [ADR-03-pixel-maschere.md](ADR-03-pixel-maschere.md): immagine tipizzata, bit di invalidità, metadati, lettore e scrittore FITS con limiti (M2.a); estensione ADR-03.b: origine `decoder` e stato del decoder RAW, RGB da debayer con classe del campione e portata della varianza, coverage, bit `support_incomplete`, maschere fotografiche distinte (M3).
- [ADR-04-persistenza.md](ADR-04-persistenza.md): store SQLite, scrittura atomica, recovery, schema ignoto, migrazione con backup, snapshot, undo e redo, export con manifest (M2.c); estensione R4: report di recovery persistito, fase sicura, workflow e resume, report di export, regola hardlink, modello di perdita di energia.
- [ADR-05-dag-provenance.md](ADR-05-dag-provenance.md): modello d'errore comune, identità degli input, nodo e chiave, generazione, provenance (M2.b).
- [ADR-06-budget-scheduler.md](ADR-06-budget-scheduler.md): budget automatico e manuale, pool, job, cancel, checkpoint, spill (M2.d).
- [ADR-13-offline.md](ADR-13-offline.md): core senza API di rete, CFITSIO solo da memoria, verifica su simboli e syscall (M2.f).
- [ADR-16-stati-master.md](ADR-16-stati-master.md): stati dei master, compatibilità, ricetta `L − D`, `Q`, maschere, ledger e report (M2.e).
- [ADR-17-varianza.md](ADR-17-varianza.md): sorgenti di varianza e propagazione al primo ordine (M2.e).
- [C-02-master.md](C-02-master.md): generazione dei master, slice minima: metodi, rejection, normalizzazione del flat, varianza per metodo, tolleranze di R2 (C-02.a).
- [ADR-07-cpu-simd.md](ADR-07-cpu-simd.md): percorsi `cpu_scalar` e `cpu_avx2`, dispatch nel preflight, ambiente in virgola mobile, tolleranze di R5 (ADR-07.a).

Contratti di M3.entry (ID di test `T-M3<slice>-<tema>-NN`):

- [ADR-08-decoder-pack.md](ADR-08-decoder-pack.md): decoder RAW con LibRaw da memoria (§9.6), catalog pack (schema, manifest, G-PACK, pack mancante, corrotto o fuori copertura), cataloghi scelti, regola NFR-LIC-01.d (ADR-08.b).
