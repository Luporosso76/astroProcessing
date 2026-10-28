# M1.e batch: entry directory dei sidecar, revisione 004

Correzione concreta del finding MED della review003. Solo preparazione:
**nessun import di moduli preparati/candidato, test, mock, worker, engine,
benchmark o build eseguito**. Nessuna chiusura runtime o auto-PASS.

## Snapshot003 prima degli edit

Conservate quindici copie byte/hash/mode sotto
`tmp/m1/e-vulkan-batch/snapshots/003/`: dieci fonti, tre ricevute precedenti,
diff003 e report di 152 righe. Manifest SHA-256:
`23e0d3b734a145f6450e087be14db46fd61465bca65ce60e4283390147af9657`.
Snapshot ricontrollato dopo gli edit; vecchie ricevute, diff e report invariati.
Il piano di 169 righe resta invariato: si corregge l'attuazione del contratto
NOFOLLOW già esistente, senza ampliare lo scope scientifico.

## Delta concreto

In `verify_success_sidecar`, ogni entry di `dirs` viene verificata anche quando
os.walk non la visiterà: stat relativo al dirfd con follow_symlinks=False,
tipo directory reale/non symlink, apertura O_DIRECTORY|O_NOFOLLOW e confronto
device/inode dell'handle aperto. Il descriptor figlio si chiude in finally.
Symlink, sostituzione dell'entry o errore producono STOP prima del successo o
dell'adozione, senza leggere/listare il target per verificarne il contenuto.

Aggiunta una regressione predisposta: `cache` punta a una directory non vuota
creata soltanto nel namespace OWN. Durante la chiamata al guard, le sentinelle
su scandir/open vietano accessi al target. Il test richiede il rifiuto esplicito
e l'assenza della ricevuta finale. Il test e la sua fixture **non sono stati eseguiti**.

Nuova revisione sorgente `sidecar-entry-004` e preregistrazione
`sidecar-preregistration-004.json`, inclusa fra gli hash richiesti prima del
lancio. Lo schema serializzato dell'ambiente resta batch-envelope-003; fonte e
manifest nuovi cambiano il digest della ricetta batch, senza modificare vecchi
lock o baseline. Non è un'ammissione a eseguire.

## Diff, hash e copertura statica

Diff effettivo da snapshot003, senza Git:
`tmp/m1/e-vulkan-batch/sidecar-004.diff`, **+50/-2** su quattro file.
Il codice/test costituisce +29/-1; il resto versiona manifest e preregistrazione.
SHA-256 diff:
`b3bc13c20694ca6f1be10f30cf00279245e3a8d178690c32f3c6c2b15c5aebe4`.

- Controller, 717 righe: `6a1f007683a89f3df9d33ad7fe80f0164584b63643fb7f55a7f7a11a15a06738`.
- Test cleanup, 196 righe: `d237f6f7af4321a7af03ed767c60b61d2474c10e18c94550abc218cd3f26467d`.

Ricevuta statica **batch** `tmp/m1/e-vulkan-batch/preparation-004.json`:
`3ddf683d35ad57e7883a94b6885016d6d58d4702370ed41f3d7cb199e4cdc13a`.
Non è il PREP004 congelato sotto e-vulkan, che rimane immutato.

Eseguito `/usr/bin/python3 -I -B -` con stdin standard library per snapshot,
ast.parse, confronto AST dei cinquanta metodi preesistenti, bytehash/mode e
difflib.unified_diff. Exit 0. **AST 8/8; 50 metodi conservati, uno aggiunto:
51 predisposti, zero eseguiti.** I file dei venti mock e dei diciassette test
follow-up precedenti restano identici a byte. Hash delle undici fonti nella ricevuta.

Le 66 tuple e i limiti sono identici: 42 forward, 24 inverse, batch 1 f32 da
42 casi. Ricontrollati a bytehash PREP004/source8/engine/shader3/indice/protocolli;
nessuna scrittura su baseline, GT, lock o uscite. Nessun output batch-001 creato.
Bound **286240768 B** invariato e preflight globale ancora non superato;
retention resta analisi separata. Nessun campione o statistica prodotto/adottato.

Autore `openai/gpt-6.1-sol`, dichiarato dal runtime. Python osservato 3.14.4;
token/costi non esposti. Nessuna delega, rete, installazione, Git, indice o
scrittura in altri namespace.

Il delta è consegnato per stabilità e review. Primo futuro turno runtime:
**soltanto suite OWN**, qualificato dal coordinatore dopo review, non ammesso ora:

```bash
/usr/bin/python3 -I -B -m unittest discover -s tmp/m1/e-vulkan-batch -p 'test_*.py' -v
```

Nessuna ammissione alle 264 fasi scientifiche o al whole-batch/M1.e/M1.
