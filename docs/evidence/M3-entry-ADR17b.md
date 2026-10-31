# M3.entry, ADR-17.b e D-17.b: modelli di covarianza P0

Stato: **APPROVATO per arbitraggio del coordinatore dopo il giro 3** (vedi «Arbitraggio»). Unità: M3.entry, riga «ADR-17.b, D-17.b» di `plans/M3.md`. Solo testo: nessun codice di prodotto, nessun test eseguito.

| Campo | Valore |
|---|---|
| Unità | M3.entry / ADR-17.b, D-17.b |
| Scopo | Fissare prima del codice i modelli di covarianza P0 (light/flat e bias in due catene, master comuni, resampling `R C Rᵀ`, Jacobiano del passo non lineare), la loro rappresentazione e la regola che ammette una misura con incertezza |
| Comando | `python3 -S tools/trace.py ids --stage M3.entry`; `python3 -S tmp/adr17b_algebra.py`; `python3 -S tools/evidence.py docs/evidence/M3-entry-ADR17b.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): §8.5–§8.8 (righe 976-1104), §8.12–§8.14 (1139-1188), §10.7 C-06 (1439-1453), §10.10 C-09 campo 6 (1496), §10.13 C-12 campo 6 (1577), F-CAL-06 (1845), F-REG-03 (1855), testo ADR-17 (2578, 2602), D-17 (2883), appendice D righe 3189-3191, 3423-3425, 3469-3471, 3734, 4066, 4067, 4076, 4077, 4106, 4107, 4119. `plans/M3.md` (tabella M3.entry, questioni aperte). Contratti ADR-17.a con le aggiunte di M2.e e R2, ADR-16.b, ADR-03.b §b.3, §b.4, §b.13, C-02.a §6 e §9; C-01.b in un altro worktree, solo letto |
| Versioni | Worktree da `463ccf9` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo |
| Risultato | Sezione «Estensione ADR-17.b» in `docs/contracts/ADR-17-varianza.md` (§b.1–b.14); verbale `docs/decisions/D-17-covarianza.md`; 11 ID pianificati `T-M3C-VAR-01…11` in `docs/test-matrix.md`; righe aggiornate in `docs/contracts/README.md` e `docs/decisions/README.md`; rinvii ad ADR-17.b in ADR-16.b §b.10 e §b.14 e in ADR-03.b §b.11 |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Contratto e decisione. Fuori: codice, test, incertezza della geometria come covarianza fotometrica (limite), pesi e rejection (C-07), scelta del kernel e drizzle (C-06), master combinati con mediana (C-02.b) |
| Esito | Review FAIL al giro 3; APPROVATO per arbitraggio del coordinatore. Vedi «Review» e «Arbitraggio» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Decisioni

- Modello unico: varianze e covarianze come `Σ_s ρ ρ' v_s` sulle sorgenti indipendenti (light e master), con le sensibilità ai valori osservati; esatto per catene lineari a pesi fissati, al primo ordine altrimenti.
- Calibrazione: tabella delle sensibilità per catena; `B` in entrambe le catene ha sensibilità `(C − N)/(N·Q)` e la varianza diventa una somma di tre termini non negativi. Le ricette con `B` in due catene hanno un modello.
- Errore del primo ordine del rapporto: identità esatta `C − C_lin = −(δFc/Fc)(C_lin − C*)`.
- Master comuni fra light: covarianza sullo stesso pixel, nulla fra pixel diversi; rappresentazione per riferimento (`covariance_sources`). Per i master del prodotto la disgiunzione dei frame sorgente è verificata nell'analisi (`shared_input`).
- Rappresentazione P0 (D-17.b): memorizzati il piano di varianza e, per `rgb`, il blocco fra canali (`CHANCOV`); fuori diagonale per riferimento con righe e pullback; mai matrici dense. Il limite di Cauchy–Schwarz non è ammesso come varianza di una pila.
- Resampling `R C Rᵀ` con righe composte su ingressi `marginal`; varianza interpolata vietata; geometria non propagata e dichiarata.
- Passo non lineare: Jacobiano per pixel, blocco fra canali per l'asinh cromatico.
- Ingressi `marginal` secondo la tabella di §b.8: calibrazione e debayer li rifiutano; resampling e somme su più pixel con incertezza richiedono il riferimento; la combinazione di frame lo richiede solo con master comuni; la curva per canale li accetta; il mescolamento dei canali richiede il blocco fra canali.
- Offset di `noise_model` per i light RAW: scalare dichiarato, mai derivato dal nero del decoder, con diagnostica nel report.
- Regola I: tolleranza degli oracle esatti per aritmetica degli intervalli; dà il bound non dyadic della varianza delle catene `.b`.
- Protocollo MC-lin: χ² esatto sulla parte lineare, margine dal resto calcolato in aritmetica esatta, varianza riportata dal prodotto controllata su una realizzazione senza rumore, probabilità di mancata discriminazione ≤ 10⁻⁶ (potenza ≥ 1 − 10⁻⁶) contro il modello senza covarianza, verificata prima della misura.

## Questioni aperte

Chiuse da questo contratto:

- ricette con `B` in entrambe le catene (`plans/M3.md`, questioni aperte; ADR-16.b §b.10);
- offset scalare di `noise_model` contro il nero `b_L` per posizione (questione aperta da C-01.b): §b.9;
- operatori ammessi su ingressi `marginal` (delega di ADR-03.b §b.4): §b.8;
- bound non dyadic della varianza delle catene `.b` (rinviato qui da C-01.b §6): regola I di §b.10.

Lasciate ad altri contratti, con il vincolo fissato qui:

- costo misurato delle covarianze per riferimento: benchmark di M3.h;
- errore del primo ordine vicino ai punti non derivabili: C-09;
- forma stazionaria della NPS per il rumore per scala: C-12.

## Verifiche dell'autore

- `python3 -S tmp/adr17b_algebra.py`: verifica in aritmetica razionale, su 5000 casi pseudo-casuali con seed, di quattro identità: l'identità del resto del primo ordine; la sensibilità di `B` in entrambe le catene; l'equivalenza fra la forma a termini non negativi e quella con `Cov(Lc, Q) = Var(B)/N` (e la forma di ADR-17.a); la covarianza fra due light con gli stessi master. Esito: `verifica algebrica ADR-17.b: ok`. La verifica ha corretto il segno della sensibilità di `B` in entrambe le catene prima della review. È una verifica della derivazione, non un test del prodotto.

## Limiti

- H-IND non verificabile sul rumore correlato del sensore; disgiunzione dei frame non verificabile per i master esterni.
- `N` costante: correzione `1 − Q/n_valid` stimata e omessa.
- Primo ordine senza bound vicino ai punti non derivabili del passo non lineare.
- Righe a pesi congelati con errore non misurato qui.
- Incertezza della geometria non propagata.
- Costo per riferimento stimato, non misurato.
- Un'immagine esportata e riletta perde il riferimento.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-adr17b-round1-20261030-0501.md` | `tmp/adr17b-round1.diff` | FAIL | 3 BLOCCANTE: `Var(B)` contata due volte in `Var(Fc)` per `q_unreliable` con `B` nella sola catena del flat; `variance_scope` conservato dopo il mescolamento dei canali; disgiunzione dei frame controllata solo dentro una calibrazione e non fra i master di frame combinati. 6 MED: blocco fra canali senza semidefinitezza né coerenza con `independent`; operatori che non mescolano i canali con `rgb` importato senza blocco; MC-lin che non legge la varianza riportata dal prodotto; oracle di T-M3C-VAR-06 costruito sulle righe del prodotto; requisito di potenza imposto ai controlli; `f_A` con denominatore nullo. 1 LOW: «potenza ≤ 10⁻⁶» |
| 2 | `.omc/artifacts/ask/codex-adr17b-round2-20261030-0509.md` | `tmp/adr17b-round2.diff` | FAIL | Rilievi del giro 1 corretti nella formula; 5 MED: test al confine di `q_unreliable` assente; semidefinitezza controllata solo con tre canali validi; varianze nulle senza regola per `det R`; normalizzazione `c/sqrt(v_c v_d)` con overflow e underflow intermedi; esito di T-M3C-VAR-11 incoerente con l'invariante per ingresso `independent` |
| 3 | `.omc/artifacts/ask/codex-adr17b-round3-20261030-0513.md` | `tmp/adr17b-round3.diff` | FAIL | Rilievi del giro 2 chiusi; 1 MED: la frase «nessun overflow né underflow intermedi» di §b.4 è falsa per `(c / sqrt(v_c)) / sqrt(v_d)` con `v_c = 2⁶⁰⁰`, `v_d = 2⁻⁶⁰⁰`, `c = 2⁻¹⁰⁰⁰` (la prima divisione va in underflow a zero). Fuori delta: il riepilogo «ingressi `marginal` ammessi solo dove esiste il riferimento» di questo record è più stretto di §b.8 |

Esito: tre giri esauriti con FAIL; l'unità si ferma e passa all'arbitraggio del coordinatore (`AGENTS.md`, ciclo di un'unità, punto 4). Il verbale D-17.b restava `PROPOSTO` fino all'arbitraggio.

## Arbitraggio

Decisione del coordinatore dopo il giro 3: applicare la correzione minima proposta dall'autore, senza un quarto giro di review. Delta: `tmp/adr17b-arbitrato.diff`, rispetto allo stato rivisto al giro 3.

- §b.4, «Semidefinitezza»: tolta l'affermazione «nessun underflow intermedio». Ora il testo dice che il primo quoziente può andare in underflow, fino a zero, quando `|ρ|·sqrt(v_d) < 2⁻¹⁰²²`. L'errore assoluto su `ρ` resta ≤ `2⁻⁵³⁸` (underflow graduale, `sqrt(v_d) ≥ 2⁻⁵³⁷`), molto sotto il margine `ε`, e l'esito del controllo non cambia. T-M3C-VAR-05 aggiunge il caso del revisore (`v_c = 2⁶⁰⁰`, `v_d = 2⁻⁶⁰⁰`, `c = 2⁻¹⁰⁰⁰`), con blocco atteso semidefinito.
- Rilievo fuori delta (LOW): il riepilogo delle decisioni di questo record segue ora la tabella di §b.8.
- Stato: D-17.b `APPROVATO`, chiuso per arbitraggio del coordinatore dopo il giro 3. `VERIFICATO` al gate M3.

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/contracts/ADR-03-pixel-maschere.md` | `ebd000bd2f7ce1ca59fb85ff7cf7494d7379253495257fdd9955d9d440053ccb` |
| `docs/contracts/ADR-16-stati-master.md` | `e443567e3fbb04693c761fd3dd7243a96a20806a49aa76cb42d63c26a43479fc` |
| `docs/contracts/ADR-17-varianza.md` | `44c307f32b9bb529c052f17f4e7154e45f809e6c46e81a7e23f41dd44cd688b8` |
| `docs/contracts/README.md` | `353e714178f05198679290fc445edac14ca3aa881f4f1d4e91c44d7c8df00b7c` |
| `docs/decisions/D-17-covarianza.md` | `8b00d0073612808ecb91d7731d60ab2ae30513536fdd8a04cc4ae9d81c41368d` |
| `docs/decisions/README.md` | `860b4a0b676f845260ed4a326d228c44b1b89ac58910261949fb640d354cad9b` |
| `docs/test-matrix.md` | `4ada80591d89089787f9bc30488017eedde76da63012d34a14980ad01f000fe0` |

<!-- evidence:end -->
