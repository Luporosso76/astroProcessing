# M2.g: contratti minimi

28 ottobre 2026. Autore: agente su Claude Opus 5.5. Review indipendente: agente `critic` su Claude Sonnet 5.5.

## Consegna

`docs/contracts/`: ADR-03 (pixel, maschere, FITS minimo = ADR-08.a), ADR-04 (persistenza, snapshot, undo/redo, export), ADR-05 (DAG, provenance, modello d'errore), ADR-06 (budget e scheduler), ADR-13 (offline), ADR-16 (stati dei master e ricetta minima = ADR-09.a), ADR-17 (varianza), più indice. ID di test di lavoro: T-M2A, T-M2B, T-M2C, T-M2D, T-M2E, T-M2F.

Decisioni del coordinatore: F-PRJ-04 e F-PRJ-07 assegnati a M2.c; ADR-13.a aggiunto; M2.a legge e conserva RGB e multi-plane, M2.e calibra solo mono e CFA; dark all'esposizione del flat accettato come `dark_flat` solo se dichiarato nella ricetta; dipendenza transitiva libcurl di CFITSIO accettata con verifiche di comportamento.

## Review

Prima verifica: FAIL, un bloccante (nomi degli artefatti per digest con `RENAME_NOREPLACE`: stop di sicurezza su output identici legittimi) e otto MED (undo/redo e `artifact_id`, PRAGMA che modificano DB estranei e file in sola lettura, traversal in `verify_export`, limiti di memoria del parser, tolleranza f64 non maggiorante, circolarità di `N`, bound di varianza chiamato bound, casi di errore dello scheduler). Ricalcolati dal revisore: condizione di esattezza e errore massimo di `C` in f32/f64, numeri di riga del piano.

Seconda verifica: PASS con riserva. Residuo MED: sei arrotondamenti relativi con `N` a conteggio pari, quindi `γ_6` e non `γ_5`. Corretto dal coordinatore come prescritto, con i tre LOW (conteggio di `Fc ≤ 0` in `N` nel report, secondo processo in sola lettura in T-M2C-08, flag `iid_assumption = not_verified` sopra una dispersione dichiarata).

Deviazione accettata: lo snapshot è (`slot_name`, `node_key`) senza generazione, perché un undo incrementa sempre la generazione.

## Esito

PASS. I contratti diventano fonte per M2.a–e; ogni clausola ha un ID di test da implementare nell'unità che la usa.
