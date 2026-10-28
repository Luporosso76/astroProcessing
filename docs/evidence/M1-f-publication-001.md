# Pubblicazione delle prime prove di hash e persistenza

2026-10-25. Commit e push effettivi:
`1bf335249f2492ad1cdf74ae570f32e7e9f44771`, branch `work/m0-m1`, origin `https://github.com/Luporosso76/astroProcessing.git`.

Allowlist: gli esatti20 documenti verificati dalla review indipendente,905 righe. Quattro record `M1-a-error-validation{,-review}.md` e `M1-c-measurement{,-review}.md`; sedici record M1.f di preparazione, guard, regressioni e primo contenuto tiny, presenti nel diff del commit. Binding canonico della lista ordinata `path + NUL + SHA256 + newline`:
`d08d617596cf7518ea4134e96caf07d67906cb28816f6e62769d7abdee2ce9c5`.

Review read-only `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`: lettura integrale905 righe,20/20 hash e binding confermati, nessun segreto/HOME personale/URL con credenziali rilevato. PASS limitato a questi documenti, non a codice, raw o artefatti futuri. Costi/token non esposti.

Prima dello staging il coordinatore ha verificato branch/origin, indice vuoto, file regolari nlink1 e binding invariato. Dopo staging: esatti20 percorsi, blob uguali ai byte revisionati, `git diff --cached --check` riuscito. Commit con identità automation limitata al comando, hook disabilitati per quel comando; nessuna configurazione globale modificata.

`git push origin work/m0-m1` exit0. Successivo `git ls-remote` sul repository ufficiale: ref remoto uguale all'intero HEAD sopra. Nessun force push, credenziale letta o nuova autenticazione configurata.

Nessun sorgente di prodotto, binario, fixture, storico utente, `tmp/`, piano attivo o file in scrittura pubblicato. I raw e il codice corrente F restano locali: il solo commit non rende completamente riproducibili questi test dal clone. Numerica v3 non eredita i tempi002; F rimane4/38 ricette e2/11 agreement, non M1.f o M1 complessiva PASS. Nessuna indipendenza crittografica dedotta dal possibile OpenSSL condiviso.
