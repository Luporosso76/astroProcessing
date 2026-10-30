# Verbale D-09.b: soglie degli operatori P0

Stato: **APPROVATO (review `adr09b-round2` PASS, `.omc/artifacts/ask/codex-adr09b-round2-20261030-0709.md`)**, 30 ottobre 2026; record in `docs/evidence/M3-entry-ADR09b.md`. Approvazione autonoma secondo `AGENTS.md` (decisioni tecniche delegate). `VERIFICATO` al gate M3 con T-M3-ADR09-01 e con i test del campo 9 dei contratti C-NN di M3 passati nell'esecuzione finale.

Piano attivo: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. Registro: D-09.b riga 3461, aggregato D-09 riga 3459. Testo di D-09, riga 2875: opzioni «Soglia unica, soglia per algoritmo, nessuna soglia»; raccomandazione «Definire per algoritmo, con ground truth e dataset, prima dell'implementazione»; stato «APERTA tecnica»; decide il «Coordinatore dopo prove e review numerica indipendente»; tempo «Prima di ogni funzione». ADR-09.b `decided_by` D-09.b, riga 4092. D-09.a: verbale [M2-entry](M2-entry.md), riga 8.

## Decisione

| Campo | Contenuto |
|---|---|
| Domanda | Con quale regola si fissa, prima della misura, la soglia di ogni operatore P0 di M3 |
| Opzioni del piano | Soglia unica; soglia per algoritmo; nessuna soglia (riga 2875) |
| Decisione | Soglia per algoritmo, secondo [ADR-09.b](../contracts/ADR-09-algoritmi.md#estensione-adr-09b-algoritmi-p0-di-m3) §b.3. Ogni soglia del campo 9 ha una classe: B binaria; E oracle esatto (0, regola I di ADR-17.b estesa a ogni grandezza, o bound analitico con fonte); D discretizzazione con verità analitica; S statistica (`α = 10⁻³` per test con Bonferroni, distribuzioni esatte o maggiorazioni senza ipotesi di forma, potenza `β ≤ 10⁻³` per un criterio che è l'unica prova di una proprietà); P requisito di prestazione (riferimento derivato per margine dichiarato); R dati pubblici; C caratterizzazione senza soglia, solo per un limite dichiarato. Il contenuto del campo 9 è in ADR-09.b §b.4 |
| Vincoli | Soglia nel contratto approvato prima della prima misura che la usa; seed, `K` e `P` nel contratto o in un protocollo preregistrato committato prima della misura. Nessun allentamento dopo una misura, nemmeno cambiando seed, `K`, `P`, fixture o casi esclusi. Una soglia irraggiungibile per un errore della derivazione è un'assunzione fallita: il gate della funzione resta FAIL, nessun declassamento. Una soglia si stringe solo con revisione e su realizzazioni nuove. I dati del gate non scelgono metodi, parametri o soglie (§13.15) |
| Perimetro | Il campo 9 di ogni contratto C-NN di M3, compresi quelli decisi da altri ADR. I test `.a` di M2 restano con le soglie di D-09.a |
| Alternative scartate | **Soglia unica**: l'errore ammissibile dipende dallo stimando e dal metodo. Per esempio `RMS ≤ 0.1 σ` non copre bias, flusso, code, ringing e arrotondamento (§8.11, riga 1133). **Nessuna soglia**: un gate senza soglia non può fallire, mentre §15.4 (riga 2382) chiede una tolleranza per operatore. **Approssimazioni asintotiche** (normale, Wilson–Hilferty) nei criteri `.b`: la copertura non è garantita a `K` finito, e ADR-09.b §b.3 ammette al loro posto distribuzioni esatte, maggiorazioni senza ipotesi di forma o proporzioni con intervallo binomiale esatto. **Soglia fissata dopo una prima misura esplorativa sugli stessi dati**: è leakage della selezione (§13.15, punto 6) |
| Evidenza | ADR-09.b §b.3-§b.5; verifica dei campi 9 di C-01.b e C-02.b (ADR-09.b §b.5). Nessun test eseguito |
| Test che la esercitano | T-M3-ADR09-01 (ADR-09.b §b.7) e i test del campo 9 di ogni contratto |
| Limiti | I margini delle soglie P sono scelte, non fatti. La probabilità di falso fallimento è controllata per test, non per gate. Decisione non verificata finché i test non passano |
