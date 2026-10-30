# M3.entry, ADR-09.b e D-09.b: algoritmi P0 e soglie

Stato: **APPROVATO** (review `adr09b-round2` PASS). Unità: M3.entry, riga «D-09.b, ADR-09.b» di `plans/M3.md`. Solo testo: nessun codice di prodotto, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / ADR-09.b, D-09.b |
| Scopo | Fissare prima della misura la regola delle soglie degli operatori P0 di M3 e una sola formulazione P0 per famiglia; verificare il campo 9 di C-01.b e C-02.b; decidere la casa della normalizzazione e dei metodi robusti dei flat |
| Comando | `python3 -S tools/trace.py ids --stage M3.entry` (50 ID); `python3 -S tools/evidence.py docs/evidence/M3-entry-ADR09b.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §8.11 (righe 1129-1137), §8.13 (1153-1172), §10.1 (1335-1353), §10.2-§10.16 campi 4 e 9 (1355-1670), §10.20 (1766-1780), §13.14-§13.17 (2203-2253), §15.4 (2376-2389), F-CAL-01…03 (1840-1842), §21.5 (2677-2703), ADR-09 (2570, 2592), D-09 (2875), appendice D righe 3405-3407, 3426-3450, 3459-3461, 4056, 4057, 4074, 4075, 4090-4092, 4100-4130. `plans/M3.md` (tabella M3.entry, slice, questioni aperte). Contratti ADR-16 (`.a` e `.b` §b.12-§b.14), ADR-17.b §b.10, §b.12-§b.14, C-01.b, C-02.a §9, C-02.b, ADR-07.a; verbali M2-entry, D-16, D-17.b |
| Versioni | Worktree da `bada26f` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Nuovo `docs/contracts/ADR-09-algoritmi.md` (ADR-09.a resta in ADR-16; estensione ADR-09.b §b.1-§b.9); verbale `docs/decisions/D-09-soglie.md`; C-01.b §6, §9 e limiti allineati; C-02.b §b.9, §b.14, §b.15 e intro aggiornati; 1 ID pianificato `T-M3-ADR09-01` in `docs/test-matrix.md`; questione dei flat tolta da `plans/M3.md` e unità `C-02.b-flat` nella riga M3.b; righe in `docs/contracts/README.md` e `docs/decisions/README.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Contratto e decisione. Fuori: codice; numeri delle soglie dei contratti non ancora scritti; scelta del metodo in C-03, C-04, C-08; formulazione delle famiglie decise da altri ADR (C-06, C-09, C-10.b, C-11, C-16…C-19) |
| Esito | Review PASS al giro 2. Vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Decisioni

- Formulazione P0 di una famiglia: stimando e modello, metodi P0 definiti per intero, regola che fissa il metodo prima dell'esecuzione. Più metodi solo con lo stesso stimando e lo stesso modello (C-02, C-07). Dove il piano fa dipendere la scelta da una misura (C-03, C-04, C-08), confronto preregistrato su fixture di sviluppo disgiunte da quelle del gate e un solo metodo P0.
- Formulazioni P0 fissate per le undici famiglie `decided_by` ADR-09.b, fra cui: debayer lineare a pesi fissi; detection su immagine filtrata con kernel fisso e fit ai minimi quadrati pesati; fondo lineare nei parametri; denoise à trous con soglia `k σ_j` dal modello di rumore (MAD adattiva non P0).
- Regola delle soglie (D-09.b): classi B, E (0, regola I estesa a ogni grandezza, bound analitico con fonte), D, S (`α = 10⁻³` con Bonferroni, distribuzioni esatte o maggiorazioni, potenza `β ≤ 10⁻³` per l'unica prova di una proprietà, altrimenti controllo di coerenza), P (riferimento derivato per margine dichiarato), R, C. Nessun allentamento dopo una misura; una soglia irraggiungibile per errore di derivazione è un'assunzione fallita.
- C-01.b e C-02.b conformi, con modifiche minime: classi nel campo 9; soglia non dyadic della varianza di C-01.b rinviata a T-M3C-VAR-01; testi su ADR-17.b allineati.
- Flat: normalizzazione e metodi robusti appartengono a F-CAL-01.b (unità `C-02.b-flat`), non sono un limite del P0, perché F-CAL-01 (riga 1840) e §10.3 campo 1 (riga 1375) li chiedono.

## Questioni aperte

Chiuse:

- normalizzazione dei flat e metodi robusti sui flat (tolta da `plans/M3.md`);
- ADR-09.b e D-09.b per C-01.b e C-02.b (§b.15 di C-02.b).

Aperte, per il coordinatore: nessuna nuova. L'unità `C-02.b-flat` va pianificata prima del codice dei flat di M3.b.

## Limiti

- I margini delle soglie P sono scelte, non fatti.
- Probabilità di falso fallimento controllata per test, non per gate.
- I test `.a` di M2 restano con le soglie di D-09.a.
- Bound in ulp delle funzioni di libreria: nessuna fonte ancora scelta.
- Riferimenti di ADR-09.b §b.8 non riletti. Nessun test eseguito.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr09b-round1-20261030-0701.md` | `tmp/adr09b-round1.diff` | FAIL | 5 MED: linearità e covarianza del fondo C-08 senza la condizione su selezione e pesi indipendenti dal rumore; margine deterministico assente nei criteri S di T-M3B-C02-03 e 04; `μ_4` di 04(b) per quadratura senza maggiorazione dimostrata; T-M3-ADR09-01 e criteri di §b.4 limitati ai contratti `decided_by` ADR-09.b mentre D-09.b vale per tutti i C-NN di M3; seed, `K`, `P` rinviati al record senza sede preregistrata verificabile |
| 2 | `.omc/artifacts/ask/codex-adr09b-round2-20261030-0709.md` | `tmp/adr09b-round2.diff` | PASS | Rilievi del giro 1 corretti; nessun rilievo nuovo |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-09-algoritmi.md` | `6c14b9ddb98276fbbed5fe168f90a4360f02f26ce82ebd75ab4d686309e68121` |
| `docs/contracts/C-01-calibrazione-light.md` | `063e14eacdbf25c92b31cf78c4bb6e19934d39fada23a06ba0c24d7fe8212162` |
| `docs/contracts/C-02-master.md` | `f69280df31fc2aa05a1e5cb4108e870c4eafaa3bd0038ddc02aeff43c0f3798d` |
| `docs/contracts/README.md` | `e6e3d69e1cb45d91fa30ffb57df91d776291866b3b41ba6bc060acc4c8da004b` |
| `docs/decisions/D-09-soglie.md` | `b906fe06799f1f250f954734d54410aa443391d2efd07d8ccad6006a39d29d76` |
| `docs/decisions/README.md` | `860b4a0b676f845260ed4a326d228c44b1b89ac58910261949fb640d354cad9b` |
| `docs/test-matrix.md` | `874634156790b236282d204d61aba926a9217275b0003241a1c8317dee75ac20` |
| `plans/M3.md` | `a7ec9173fdb713afe26941008cc5c88033bca8d28c694cf90c391ce3cb54a95d` |

<!-- evidence:end -->
