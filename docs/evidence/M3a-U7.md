# M3.a, unità U7: G-CAL parte 2, predicati, ammissibili, default, selezione

Stato: **chiusa**. Test: review PASS al giro 2; test del bit `z` (emendamento di §b.4): giro 2 FAIL (1 MED), arbitraggio del coordinatore, review sul delta PASS. Codice: review FAIL al giro 1 (3 MED: rami senza test), test aggiunti da un agente dei test, review PASS al giro 2 (blocco «Review»).

| Campo | Valore |
|---|---|
| Unità | M3.a / U7 (test e codice) |
| Scopo | `analyze_calibration`, funzione pura degli ingressi: predicati P-DFB, P-DF, stato del flat disponibile (ADR-16.b §b.4); insieme ammissibile senza doppia sottrazione e P-OFFSET con `flat_division_offset` (§b.5); controllo termico di `flat.bias` in aritmetica esatta sui valori f64 (§b.5); default D-16, mai il percorso parziale (§b.6); ricetta fuori dall'insieme → G-STOP senza nodo e parametri non applicabili (§b.9) |
| ID | G-CAL (parte 2), G-OFFSET (P-OFFSET); T-M3A-CAL-01, T-M3A-CAL-02, T-M3A-CAL-06, T-M3A-C01-02 |
| Comando | Integrazione: build pulita, `tools/check.sh` e `ctest --preset release` completo (blocco generato). Mutazioni: `tools/mutate.sh` (patch in `tmp/u7-mut/` e `tmp/u7-r1-mut/` del worktree) |
| Versioni | GCC 15.2.0, CMake 4.2.3, Ninja |
| Ambiente | Linux 7.0.0-38-generic x86_64, 16 CPU; worktree `.claude/worktrees/opus-u7c` sulla base U6 + U8a + U13a |
| Autore e data | Test: agenti Claude Opus 5.5 (abbonamento e API) e Claude Sonnet 5.5 (API, un caso LOW). Codice: agente Claude Opus 5.5 (abbonamento). Review: Claude Opus 5.5 (Argon fuori quota). Coordinatore: Claude Opus 5.5. 5 e 6 novembre 2026 |

## Decisioni del coordinatore

- §b.4 (6 novembre 2026): `z` = 1 se il bias è `usable` in almeno una catena i cui membri sono tutti ammessi; senza flat candidato, o con flat candidato ma `excluded` per qualunque motivo di §b.2 o §b.3, le catene del flat non contano e un bias `usable` solo con il flat ha `z` = 0; lo stato del bias nel verdetto di coppia F–B resta quello della coppia.
- §b.5 (6 novembre 2026): controllo termico non valutabile (canale di normalizzazione senza pixel validi per `N`, oppure `N_c` ≤ 0 o non finita) → solo `flat.bias` del flat `excluded` con `flat_thermal_budget`, l'analisi non si ferma; in esecuzione resta lo stop di ADR-16.a §6.
- §b.9 (6 novembre 2026): un parametro applicabile dichiarato dall'analisi deve essere uguale alla dichiarazione, altrimenti `invalid_input`, `calib.param.<nome>`, contesto `parametro`; un parametro non dichiarato si controlla solo sul dominio; il default non usa mai un parametro non dichiarato.

## Mutazioni

Tutte rilevate con `tools/mutate.sh`.

| Gruppo | Patch |
|---|---|
| Codice, giro 1 (`tmp/u7-mut/`) | `m1-zbit-flat-not-admitted`, `m2-p-offset-without-declaration`, `m3-thermal-f64-compare` (confronto termico in f64 invece che esatto), `m4-thermal-without-beta-plus`, `m5-default-divides-on-partial-offset`, `m6-recipe-with-excluded-master`, `m7-bias-flat-chains-without-flat` |
| Test dopo il giro 1 (`tmp/u7-r1-mut/`) | `thermal-nonpositive`, `thermal-empty-channel`, `default-invents-declaration`, `param-differs`, `param-domain` |

## Limiti

- Il controllo `recipe_from` nel ciclo del light del default è una mutazione equivalente: le dichiarazioni delle catene del light sono già obbligatorie nell'analisi (commento nel codice).
- Report e ledger con β_c, β⁺_c e voci `omitted` sono di T-M3A-CAL-04, fuori da U7.
- Lavoro aperto prima del gate M3.a (fuori delta della review del giro 2): gli stop `calib.param.normalization`, `calib.param.q_min`, `calib.normalization_requires_cfa` con le dichiarazioni termiche presenti non sono nel contratto e non hanno test; `master_excluded` senza un motivo di §b.3 per le catene incoerenti con lo stato del master; test dei domini delle dichiarazioni nell'analisi e di `temperature_abs_tol_c` con policy `not_applicable_declared`.

## Review

- Test, giro 1 FAIL, giro 2 PASS. Test del bit `z`: giro 1 FAIL (1 MED), giro 2 FAIL (1 MED: stato del bias lasciato libero), arbitraggio del coordinatore, review sul delta `tmp/api/u7zbit-arb-review.json` PASS con 3 LOW chiusi.
- Codice, giro 1 FAIL (3 MED: controllo termico non valutabile, default che inventa una dichiarazione, parametro diverso o fuori dominio; tutti senza test), giro 2 PASS con 4 LOW chiusi (testo di §b.5, caso con bound 0, commento dell'header, argomento della mutazione equivalente).

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-16-stati-master.md` | `703ce7ac2ed085f4aca2b6fa716bb528c21ba58d15316e20333fbf37c4994ff4` |
| `docs/decisions/D-16-calibrazione-incompleta.md` | `17d99880649e716815e5a57741c71362fee41710535c0e2b99662a392649abb7` |
| `src/core/calibration_analysis.cpp` | `ef641f3be2d55582cc3096b26ee0d44d8726fbf6d7051b1c5c3f73e81e694e6d` |
| `src/core/include/astro/calibration_analysis.hpp` | `c88647b30b362c843d9bc26a70f69b9caf8713516c0a05fbe84f806cbd0126e9` |
| `tests/unit/test_calibration_selection.cpp` | `afb004a873fabada2c42e16332f6ba5208f8d53ec046fceb0a3a23a345f47cd4` |

- Test release (`tmp/release-junit.xml`): totali 1107, fast 1089, slow 18, falliti 0
- Test dev (`tmp/dev-junit.xml`): totali 1089, fast 1089, slow 0, falliti 0
<!-- evidence:end -->
