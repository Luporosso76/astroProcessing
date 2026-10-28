# Pubblicazione delle prime prove Vulkan

Data: 25 ottobre 2026. Record del coordinatore, successivo alla review e al push effettivi.

## Review e binding

Lo specialista security, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`, ha letto integralmente i diciannove documenti selezionati: 1453 righe. Esito PASS pre-pubblicazione limitato all'allowlist, nessun finding bloccante di sicurezza o privacy. Nessun nuovo audit degli artefatti runtime o certificazione di titolarità legale; costi e token non esposti.

L'allowlist comprende `plans/M1-e-vulkan.md`, i sedici documenti `M1-e-*` presenti nel commit, `M1-publication-001.md` e `M1-reproducibility-review-001.md`. La lista esatta è verificabile con:

```bash
git show --format= --name-only cbe836b63365e60f170c561e4b610279978d23c9
```

SHA-256 della concatenazione ordinata di `percorso relativo + NUL + SHA256(file) + newline`: `d7fc3de8ac08c2ac967528a75f331278091dac0f384ff2e97baf8c043deaec3a`.

## Comandi ed esito effettivo

Il coordinatore ha confrontato il binding prima dello staging, richiesto staging inizialmente vuoto, aggiunto soltanto quei diciannove percorsi, verificato uguaglianza byte per byte fra indice e worktree e superato `git diff --cached --check`.

Commit eseguito con identità tecnica locale al comando, senza cambiare configurazioni globali:

```bash
git -c user.name='AstroProcessor Automation' -c user.email='automation@astroprocessor.invalid' commit --no-gpg-sign -m 'docs: pubblica prove CPU e GPU Vulkan'
git push origin work/m0-m1
git ls-remote --heads origin refs/heads/work/m0-m1
```

Tutti i comandi hanno avuto exit 0. Commit `cbe836b63365e60f170c561e4b610279978d23c9`, diciannove file e 1453 inserimenti. La ref remota letta dopo il push coincide con HEAD; origin è `https://github.com/Luporosso76/astroProcessing.git`. Usate soltanto credenziali già disponibili, senza prompt interattivo, force push o riscrittura della storia.

Non sono pubblicati da questo commit sorgenti, binari, trace voluminose, namespace attivi o gli otto storici locali. La pubblicazione di documentazione non rende riproducibile dal solo clone l'intera serie, non adotta uno stack e non chiude M1. Nessun nuovo benchmark è stato eseguito per questa pubblicazione.
