# Contratti minimi M2

Parti degli ADR usate da M2.a–e. Errori: modello comune in ADR-05. ID di test `T-M2<unità>-NN`: etichette di lavoro, da riportare in `docs/test-matrix.md` quando i test esistono.

- [ADR-03-pixel-maschere.md](ADR-03-pixel-maschere.md): immagine tipizzata, bit di invalidità, metadati, lettore e scrittore FITS con limiti (M2.a).
- [ADR-04-persistenza.md](ADR-04-persistenza.md): store SQLite, scrittura atomica, recovery, schema ignoto, migrazione con backup, snapshot, undo e redo, export con manifest (M2.c).
- [ADR-05-dag-provenance.md](ADR-05-dag-provenance.md): modello d'errore comune, identità degli input, nodo e chiave, generazione, provenance (M2.b).
- [ADR-06-budget-scheduler.md](ADR-06-budget-scheduler.md): budget automatico e manuale, pool, job, cancel, checkpoint, spill (M2.d).
- [ADR-13-offline.md](ADR-13-offline.md): core senza API di rete, CFITSIO solo da memoria, verifica su simboli e syscall (M2.f).
- [ADR-16-stati-master.md](ADR-16-stati-master.md): stati dei master, compatibilità, ricetta `L − D`, `Q`, maschere, ledger e report (M2.e).
- [ADR-17-varianza.md](ADR-17-varianza.md): sorgenti di varianza e propagazione al primo ordine (M2.e).
