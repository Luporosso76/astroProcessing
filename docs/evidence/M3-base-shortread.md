# M3 base: fine file prematura su un input riscritto durante la lettura

| Campo | Valore |
|---|---|
| Origine | Esecuzione finale della base comune, preset release (`tmp/base-final-rel.log`): `Types/Conformance.T_M3BASE_11_inputs_verified_before_use/master_combine_unica` fermato con `io` `input.short_read` invece di un codice `integrity` |
| Causa | Il caso riscrive l'input durante i task; un lettore concorrente trova la fine file mentre il file è troncato, e quando `unexpected_eof` esegue `fstat` la dimensione è già tornata quella aperta. Il confronto guardava solo la dimensione |
| Correzione | `unexpected_eof` (`src/core/fits.cpp`) chiama `fits_detail::check_unchanged`: dimensione cambiata `input.size_changed`, mtime o ctime cambiati `input.modified`; solo con identità invariata resta `io` `input.short_read` |
| Contratto | `docs/contracts/ADR-05-dag-provenance.md`, tabella degli errori |
| Test | `Fits.T_M2A_short_read_after_a_rewrite_stops_as_modified`, `Fits.T_M2A_short_read_after_a_truncation_stops_as_size_changed` (autore diverso dall'autore del codice); invariato `Fits.T_M2A_short_read_with_unchanged_size_stops_as_io` |
| Mutazione | `check_unchanged` tolto da `unexpected_eof`: 3 test su 52 rossi (i due nuovi e `T_M2A_05_file_changing_size_after_fstat_stops`); RILEVATA |
| Review | Codex `gpt-6-astra`, `tools/review.sh shortread-round1`: PASS senza rilievi (`.omc/artifacts/ask/codex-shortread-round1-20261030-1237.md`) |
| Limite | Una riscrittura che conserva dimensione, mtime e ctime (per esempio con i tempi ripristinati da chi scrive) resta `io` `input.short_read`; la verifica del digest prima della pubblicazione (OB-11) la ferma comunque |
