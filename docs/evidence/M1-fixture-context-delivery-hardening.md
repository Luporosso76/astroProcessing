# Contesto store: correzione della directory di consegna

26 ottobre 2026. Esito: **PASS_METADATA_ONLY**, non esecuzione della suite.

Il primo turno è rimasto `STOP_NOT_RUN`, 0/47, come registrato in
`M1-fixture-context-validation.md`. Il coordinatore ha corretto il modo della sola
directory preparata dagli agenti, senza modificare sorgenti, ammissione o gate.

## Operazione eseguita

Interprete: `/usr/bin/python3 -I -B -S -`, script stdlib del coordinatore, exit0.
Attraversamento fd-relative con `O_DIRECTORY|O_NOFOLLOW` da questa workspace;
verifica UID e modo prima di `fchmod`, poi `fsync` del directory descriptor.

| Campo | Prima | Dopo |
|---|---|---|
| Directory | `tmp/m1/fixture-store/context-lifecycle-tests/` | identica |
| UID/GID | 1000/1000 | 1000/1000 |
| Device/inode | 66306/29137873 | 66306/29137873 |
| Modo | 0775 | 0700 |
| ctime ns | 1793002847475653520 | 1793003558194823828 |

Verificati prima e dopo dodici file pinned: tre voci della PREP, sei preservati,
PREP stessa, ammissione001 e record del primo STOP. SHA, device/inode, UID/GID,
nlink, modo, dimensione e mtime di questi file restano identici. Tutti sono file
regolari owned single-link. `context-001` resta assente.

Pin PREP: `f5e166cf8b24f6695eb7f21219aab538358f0b09357c79c1242da2b565a62133`.
Pin ammissione001: `7bf7c0e02237d0edc4cf1a811a8f507aad3e1de462e2213401d17481e67490bf`.
Pin del primo STOP: `9f6e4973676634e3960adc61d507f3910921deaa9596d88f7d227294794877e7`.

## Limiti e prossimo gate

Nessun import candidato, test, worker, ZIP o payload scientifico. Nessuna nuova
osservazione di RSS, spazio globale o comportamento della suite. La correzione
rimuove una causa del preflight; non sostituisce i controlli non raggiunti.
Serve review indipendente del delta metadata prima di ammettere un nuovo turno.
Il precedente fallimento resta conservato, senza retry automatico.
