# M3.a: emendamenti dei contratti per le lacune L1, L4, L6

Stato: **CHIUSO, review PASS al giro 2**. Unità: M3.a, emendamenti di contratto prima del codice (sezione «Lacune dei contratti» di `plans/M3a.md`). Solo testo: nessun codice di prodotto, nessun test scritto o eseguito, nessuna build.

| Campo | Valore |
|---|---|
| Unità | M3.a / emendamenti L1, L4, L6 |
| Scopo | Chiudere nei contratti le lacune L1 (confine di lettura degli input tipizzato FITS), L4 (metadati di F-IO-04 non forniti dal decoder RAW) e L6 (prova di T-M3A-PACK-02 senza un operatore del prodotto) prima delle unità U0, U4 e U10 |
| Comando | `codegraph explore` su `read_verified_input`, `reverify_input`, `open_fits_input`, `read_input_bytes`, `check_path_names`, `check_unchanged`, `fits_plan`; `grep` su `src/core/`, `tests/unit/`, `tools/check_job_types.py`, `/usr/include/libraw/libraw_types.h`; `tools/review.sh m3a-amend-contract1 tmp/m3a-amend-facts.md`, `tools/review.sh m3a-amend-contract2 tmp/m3a-amend-facts.md`; `python3 -S tools/evidence.py docs/evidence/M3a-emendamenti.md` |
| Input | Codice al commit `311fb7a`; `plans/M3a.md` (non tracciato, copiato dal checkout principale); `docs/contracts/BASE-confini.md`, `docs/contracts/ADR-08-decoder-pack.md`; piano attivo riga 1815 (F-IO-04); header di LibRaw 0.21.5 installata |
| Versioni | Worktree da `311fb7a`; LibRaw 0.21.5 (`libraw_types.h` di sistema) |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Emendamenti in `BASE-confini.md` (L1) e `ADR-08-decoder-pack.md` (L4, L6); stato delle lacune L1, L4, L6 in `plans/M3a.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Solo L1, L4, L6. Fuori: L2 e L5 (contratto di F-IO-03, altra unità), L3 (F-IO-08) |
| Esito | PASS al giro 2 (giro 1 FAIL con 2 MED, corretti). Vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Che cosa cambia

| Lacuna | Contratto | Cambio | Test nuovi |
|---|---|---|---|
| L1 | `BASE-confini.md` §1, §2.5 (OB-11 «Emendamento L1», OB-12), §3.1 punto 1, §3.2, §4 | Formato d'ingresso dichiarato, mai dedotto: costante del core per formato (prefisso dei codici, limite, piano degli header facoltativo), elenco unico delle dichiarazioni, formato per ruolo nel registro. Protocollo di apertura unico con la dichiarazione (`open_fits_input` resta come chiamata FITS); `read_verified_input` con piano facoltativo, nessun byte interpretato prima del digest per i RAW; `reverify_input` con il formato del ruolo. Stesso proprietario (lettura degli input); stato MANCANTE per i formati diversi da FITS, debito di U0 | T-M3BASE-20 (limite, file non regolare, permesso, assente, per formato), T-M3BASE-21 (lettura verificata senza piano degli header), T-M3BASE-22 (formato del ruolo uguale al registro). T-M3BASE-11 omette solo il caso «header cambiato» per i ruoli senza piano degli header; nessun altro caso cambia |
| L4 | `ADR-08-decoder-pack.md` §2.6, §2.9, §7 | Sensore, dimensione del pixel, scala pixel e WCS assenti per i RAW: chiavi `null` con `absent_reason = "not_provided_by_decoder"` nel record del decoder e nell'export di F-IO-04; mai stimati da focale, makernote, GPS o tabelle | T-M3A-RAW-13 |
| L6 | `ADR-08-decoder-pack.md` §3.4 («Prova in M3.a»), §6 | T-M3A-PACK-02 in M3.a con un `branch_files_job` del test composto da `run_branches` sullo store SQLite; il job del test non entra nel registro; la conferma con l'operatore del prodotto spetta a M3.p | Nessuno: precisazione di T-M3A-PACK-02 |

## Limiti

- Solo lettura del codice al commit `311fb7a`: nessun test eseguito, nessuna build. I test nuovi sono pianificati e non sono in `docs/test-matrix.md`.
- L1: il limite RAW di 1 GiB resta una stima non misurata (ADR-08.b §2.1); la prenotazione dei byte del file nel piano del job RAW resta di OB-08 e di U4.
- L4: «sensore» è letto come caratteristiche del sensore diverse dalla temperatura; i campi `SensorWidth`/`SensorHeight` di alcune makernote non sono verificati né letti.
- L6: M3.a non prova la chiamata di G-PACK nel preflight dell'operatore del prodotto né la sede dell'arresto (OB-01 o OB-02); spetta a M3.p.
- I blocchi generati dei record precedenti che citano `BASE-confini.md` e `ADR-08-decoder-pack.md` (`M3-base-*`, `M3-entry-ADR08b`) restano gli hash delle versioni rivedute allora: non si riscrivono.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-m3a-amend-contract1-20261030-1208.md` | primo giro, testo intero | FAIL | 2 MED, 1 LOW: osservatore dei `openat` di T-M3A-PACK-02 non delimitato (il runner apre `artifacts/` dello store); fatto errato su `digest_input`, che legge il contenuto; LOW: U0 e U4 di `plans/M3a.md` non allineati («gemella», T-M3A-01, T-M3A-RAW-13) |
| 2 | `.omc/artifacts/ask/codex-m3a-amend-contract2-20261030-1212.md` | `tmp/m3a-amend-round2.diff` | PASS | I 2 MED corretti. Fuori delta: il LOW del giro 1 sul piano; tabella della review vuota nel record al momento del giro (compilata qui) |

Punto aperto (LOW del giro 1, non corretto perché il mandato limita le modifiche del piano alle righe delle lacune): in `plans/M3a.md` U0 ammette ancora una funzione «gemella» di `open_fits_input`, che il contratto emendato vieta se ricopia il protocollo; U0 elenca T-M3A-01 invece di T-M3BASE-20…22; U4 non elenca T-M3A-RAW-13. Si allinea prima di U0 e U4.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-08-decoder-pack.md` | `281b2a5ae80092dca1b534ee7dd6044e0dd877e3d57484ebfc02892c802fb169` |
| `docs/contracts/BASE-confini.md` | `573fba6402d89abb5a8df3566792f36a85602519ff6525da755d7d5c0472eccb` |
| `plans/M3a.md` | `17d4bf7f5f78a124f63a068bad3e2e54ade49c91bbc3310f11efbb35186e61a8` |

<!-- evidence:end -->
