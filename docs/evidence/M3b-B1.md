# M3.b, unità B1: protocollo preregistrato dei test statistici

Stato: **chiusa** il 9 novembre 2026. Solo documento. Review `M3b-B1-round1` PASS con 1 LOW, corretto.

| Campo | Valore |
|---|---|
| Unità | M3.b / B1 (solo documento) |
| Scopo | Per ogni test statistico `K`, `P`, seed, parametri, `α = 10⁻³` con Bonferroni sulle `m` statistiche, margini deterministici `τ_k`; regola di derivazione di `K` e `P` per T-M3B-C02-04(b) |
| ID | C-02.b §b.9; C-02.b-flat §f.9; ADR-09.b §b.3, §b.4; T-M3-ADR09-01 |
| Comando | nessuno: nessun codice, nessuna build. SHA-256 della versione corrente: `sha256sum docs/evidence/M3b-protocollo.md` |
| Commit | `f24d1bb` (versione 1). Le versioni 2 e 3 sono nel commit `483b671` (B5) |
| Versioni | non applicabile (documento) |
| Ambiente | non applicabile (documento) |
| Autore e data | Coordinatore. Review: Gemini 4 Argon. 9 novembre 2026 |

## Protocollo

`docs/evidence/M3b-protocollo.md`, preregistrato prima del primo record di misura.

| Versione | SHA-256 | Cambio |
|---|---|---|
| 1 | `83e300300952752e049536db3c3d792db83860ebddfafe8a851f24ae6e86fe00` | testo di B1 |
| 2 | `4b06acb685f79ce9599037efb5b70e82b11fc5a6cf563686fff118c67238e8a9` | `τ_k` come ampiezza dell'intervallo e criterio di inclusione (`docs/decisions/M3b-lacune.md`, «Decisioni successive (B5)»); strettamente più severo; nessun record di misura registrato con la versione 1 |
| 3 | `e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7` | nota sull'implementazione della regola I nell'oracle (§1), criteri e valori invariati; coincide con `sha256sum` del file nell'albero di `eef7fcf` |

Decisione di B1 sul perimetro: la mediana con `n = 101` e winsorized (101, 50) escono da T-M3B-C02-04(b) e passano a C02-01 e C02-05; statistiche `m = 30`, livello per statistica `3.333 · 10⁻⁵` (`docs/decisions/M3b-lacune.md`, «Decisioni successive (B1)»; motivo: `K · P` minimo circa 1.4 · 10¹⁰ per la mediana a `n = 101`).

## Limiti

- Il protocollo è stato emendato due volte dopo il commit di B1 (v2, v3), sempre prima di ogni record di misura e con decisione scritta; il record di ogni test cita il commit e lo SHA-256 della versione usata.
- `K`, `P` e seed non sono ripetuti qui: stanno nel protocollo (§2 riepilogo, §3).

## Review

- Giro 1 PASS (`.omc/artifacts/ask/review-M3b-B1-round1-argon-20261109-0748.md`). LOW (valori indicativi di `K · P` con `m = 32` o `m = 30` fra protocollo e decisione): corretto.

## Blocco generato

Assente. Hash dei file, conteggi dei junit ed esito di `trace.py check` li scrive `tools/evidence.py` da un file `.evidence`, al run completo del gate (`docs/evidence/M3-b.md`); non si scrivono a mano. Il run completo (`ctest --preset dev-full` e `release`, stesso commit) è del gate: qui valgono solo le verifiche d'unità dichiarate sopra.
