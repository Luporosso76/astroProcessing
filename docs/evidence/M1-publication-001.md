# Pubblicazione delle prime evidenze M1

Data: 25 ottobre 2026. Coordinatore: GPT-6.1 Sol, dichiarato dal runtime; nessuna attestazione indipendente.

- Commit: `fce97fd2bd3609ec9c8452d29c64200341e5a2be`.
- Repository: `https://github.com/Luporosso76/astroProcessing.git`.
- Branch: `work/m0-m1`.
- Ambito: esattamente 22 file Markdown, selezionati esplicitamente; nessun sorgente, binario o campione in `tmp/` pubblicato.
- Gli otto documenti storici locali, incluso il draft di provenienza incerta, restano non tracciati.

Review indipendente `security`, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`: PASS limitato ai 22 file, 1464 righe esaminate. Modello autodichiarato `openai/gpt-6-astra`. Nessun segreto, URL con credenziali o percorso home personale rilevato; preparazioni, prove eseguite e limiti distinti. La review non ripete i test numerici o le verifiche degli artefatti citati.

Prima del commit il coordinatore ha ricontrollato la stessa allowlist, la corrispondenza fra blob staged e file letti, l'assenza di altri percorsi staged e `git diff --cached --check`. Digest SHA-256 della lista ordinata `percorso + NUL + SHA256(file) + newline`: `34499c9e249ac4e58ea54aaf8353ac43ec8886f5245f8564344169bc7c246176`.

Comandi effettivamente eseguiti:

```sh
git -c user.name='AstroProcessor Automation' -c user.email='automation@astroprocessor.invalid' commit --no-gpg-sign -m 'docs: registra misure M1 e review indipendenti'
git push origin work/m0-m1
git ls-remote https://github.com/Luporosso76/astroProcessing.git refs/heads/work/m0-m1
```

Push con exit 0; hash remoto identico a HEAD locale. Credenziali già disponibili usate da Git, senza ispezionarne i file. Nessun force push, riscrittura della storia o modifica a branch protetti.

La lista esatta dei 22 percorsi è il diff del commit. Il PASS riguarda la pubblicazione documentale, non il gate complessivo M1. Le prove locali citate dai report non sono tutte ricostruibili dai soli file pubblicati.
