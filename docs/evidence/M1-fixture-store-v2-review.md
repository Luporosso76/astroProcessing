# Store fixture v2: review del contesto operativo

Data: 26 ottobre 2026. Review indipendente dell'agente configurato `reviewer`, sessione `ses_e88edd651ffexUJuuZVSYdl3bo`; verbale persistito dal coordinatore. Il revisore ha autodichiarato `openai/gpt-6-astra`, non attestato indipendentemente. Nessun override del modello o della configurazione; costi/token non esposti.

**READY STATIC soltanto per future regressioni minute. Nessuna fase archivio autorizzata.**

Il precedente finding MED sugli helper mutanti è corretto nel codice esaminato: dieci ingressi operativi iniziano con `need_context`; `remove_copies` richiede archivio verificato e, per release, risultati durevoli, ricontrollando il contesto prima dell'unlink. Emissione dopo ammissione/pin/lease, revoca prima della chiusura.

Resta una **copertura MED mancante**: i13 nuovi test controllano contesto assente/non emesso e bootstrap senza ammissione. Non raggiungono i rami successivi per un contesto emesso poi scaduto, fase errata, lease chiusa/sbloccata/sostituita o binding/path alterati (`v2/store.py:414-455`; test161-204). Questi casi sono richiesti prima di qualificare operazioni reali; nessun nuovo bug concreto attestato dal delta.

Copertura riferita: report186 righe, receipt221 e quattro fonti v2 di881/208/124/140 righe letti integralmente; confronto diretto v1:350-626 e135 righe dei vecchi test. Undici test conservati,24 predisposti, zero eseguiti. Il revisore ha confrontato SHA riportati ma non li ha ricalcolati: questa review non attesta ex novo i byte dei manifest o dei13 pin storici.

I test aggiuntivi sono assegnati separatamente a `tests`, senza modificare la v2 già recensita. Archive, verify, retire, materialize e release restano cinque ammissioni separate, ciascuna preceduta dai propri gate. Nessun import candidato, lettura payload847MB, ZIP, rimozione o altra fase reale è stata eseguita dalla review.
