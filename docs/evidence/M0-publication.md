# Pubblicazione iniziale della base operativa

Data: 25 ottobre 2026. Autore: coordinatore GPT-6.1 Sol autodichiarato dal runtime. Ambito: commit documentale, non codice di prodotto o esito dei benchmark M1.

## Selezione e verifiche

Staged 19 file espliciti: `.gitignore`, `AGENTS.md`, `PLANS.md`, piano 2.2.1, mandato corrente, `ISTRUZIONI_OPERATIVE.md`, due ExecPlan, progress, matrice test, due documenti decisioni, sei record/indice delle evidenze M0 e `doc_old/README.md`. Il conteggio esatto è riproducibile con `git ls-tree -r --name-only fc3d3d8`.

Il controllo Python ha confrontato l'insieme staged con l'allowlist, ogni blob staged con il file verificato e l'hash del piano con il verbale di review. Pattern cercati: chiavi private, token GitHub, credenziali nelle URL e percorsi home personali. Nessun match. Esclusi gli otto input archiviati, il draft di provenienza incerta, `.git/` e tutti i temporanei. Copertura della review indipendente in [M0-ops-review](M0-ops-review.md); non equivale a un audit del software ancora inesistente.

`git diff --cached --check` ha segnalato **sette** spazi terminali preesistenti nei due mandati immutabili, alcuni usati come hard break Markdown. Il controllo sugli altri 17 file è PASS. I mandati del proprietario non sono stati modificati per eliminare questi avvisi. Il controllo globale non viene presentato come privo di avvisi.

## Operazioni eseguite

```bash
git -c user.name='AstroProcessor Automation' -c user.email='automation@astroprocessor.invalid' commit --no-gpg-sign -m 'docs: avvia esecuzione Linux e preregistra gli spike M1'
env GIT_TERMINAL_PROMPT=0 GIT_ASKPASS=/bin/false SSH_ASKPASS=/bin/false git push --set-upstream origin work/m0-m1
git ls-remote origin refs/heads/work/m0-m1
```

Commit radice: `fc3d3d8b612c50dbf97674568a8109a2b6e33c93`, 19 file, 5997 righe aggiunte. Commit e push hanno restituito exit **0**. Un controllo successivo ha confrontato `git rev-parse HEAD` con il riferimento remoto: **identici**.

Repository: `https://github.com/Luporosso76/astroProcessing.git`. Nessun force push, riscrittura della storia, branch protetto, nuovo servizio o credenziale creata. L'identità di commit è dichiaratamente automatizzata e limitata al comando; nessuna configurazione personale è stata letta per ricavarla. L'autorizzazione al push deriva dall'ultima istruzione del proprietario e supera il precedente divieto nelle istruzioni conservate.

## Limiti

La copia remota iniziale non contiene gli otto documenti storici locali. I vecchi comandi di audit che li richiedono non sono ripetibili dal solo clone pubblico finché quegli input non vengono forniti lecitamente. Il manifest locale ne preserva gli hash; non si dichiara una riproducibilità pubblica completa. Nessun eseguibile, dato catalogo, fixture privata o licenza di prodotto è stato pubblicato. Questo record è successivo al commit descritto e verrà incluso in un commit seguente.
