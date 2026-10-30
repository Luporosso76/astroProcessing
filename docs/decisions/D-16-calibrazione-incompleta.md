# Verbale D-16: default della calibrazione incompleta

Stato: **APPROVATO (review `adr16b-round3` PASS, `.omc/artifacts/ask/codex-adr16b-round3-20261030-0336.md`)**, 30 ottobre 2026; record in `docs/evidence/M3-entry-ADR16b.md`. Approvazione autonoma secondo `AGENTS.md` (decisioni tecniche delegate): `APPROVATO` quando la review indipendente passa; `VERIFICATO` al gate M3, con T-M3A-CAL-01, 02 e 04 passati nell'esecuzione finale.

Piano attivo: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. Registro: D-16 riga 3468, testo riga 2882 («APERTA sul default, vincolata da NC-21; coordinatore dopo prove ADR-16; M3.entry»); ADR-16.b `decided_by` D-16, riga 4104. Testo di ADR-16 riga 2600: «Default automatico e livello di avviso si decidono autonomamente dopo prove, senza sostituzioni successive al guasto».

## Decisione

| Campo | Contenuto |
|---|---|
| Domanda | Quale ricetta propone il prodotto quando mancano master, o quando alcuni sono esclusi dall'analisi, e con quale livello di avviso |
| Opzioni del piano | Matrice di ricette compatibili e omissioni pianificate (riga 2882); richiedere tutti i master è escluso dal mandato (riga 2577) |
| Decisione | Il default è la ricetta che contiene, per la catena del light e per quella del flat, la correzione di priorità più alta fra quelle ammissibili secondo [ADR-16.b](../contracts/ADR-16-stati-master.md#estensione-adr-16b-matrice-completa-della-calibrazione-m3) §b.5, con i soli fatti verificati dall'analisi (G-CAL) e le dichiarazioni già presenti nel progetto. Non inventa una dichiarazione, non sceglie il percorso parziale della divisione per `Q`, non divide per `Q` se l'offset del light non è rimosso da un master. Priorità e tabella delle otto combinazioni in ADR-16.b §b.6 |
| Livelli di avviso | `info`: correzione non applicabile (per esempio bias già nel dark matched). `warning`: correzione omessa, percorso parziale, ipotesi dichiarata dall'utente. Gli errori sono stop, mai avvisi |
| Vincolo NC-21 | Il default si calcola prima dell'esecuzione e si scrive per intero nella ricetta, come una scelta esperta. Dopo la selezione un prerequisito perso è P-LOST: stop del ramo, nessuna ricetta diversa, nessuna omissione attivata (ADR-16.b §b.8). Il default non è un ripiego: è la proposta di partenza, e una ricetta diversa è una scelta dell'utente fatta prima dell'esecuzione |
| Alternative scartate | Default che include i rami basati su ipotesi non dichiarate (per esempio `F − B` presumendo trascurabile il termico del flat): il prodotto non può verificarle, quindi entrano solo con la dichiarazione dell'utente. Default che divide per `Q` lasciando l'offset del light (`B_res/Q`, §9.3 riga 1238): solo come percorso parziale esplicito. Omettere `L − D'` quando manca il bias: `L − D'` toglie il termico e lascia l'offset come la ricetta senza dark, quindi nessun termine peggiora |
| Evidenza | Contratto ADR-16.b §b.5-§b.6; derivazioni del controllo termico e dei bound in §b.5 e §b.12. Nessun test eseguito |
| Test che la esercitano | T-M3A-CAL-01 (analisi e default su tutte le configurazioni), T-M3A-CAL-02 (proprietà del default), T-M3A-CAL-04 (ledger, motivi e livelli) |
| Limiti | Decisione non verificata finché i test non passano. La priorità fra le catene è una regola, non una misura di qualità: nessun dato pubblico la confronta ancora (T-CAL-08 con dati reali in M3.s). Un candidato per ruolo; la scelta fra più master compatibili è di F-IO-03 e ADR-15 |
