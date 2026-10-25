# M1.e: prefisso minimale, conservazione integrale dei pacchetti

Data: 2026-10-25. **Rimosse soltanto dieci copie estratte ridondanti**, autorizzate dal brief. Nessun pacchetto originale, prova, fixture, manifest precedente o report precedente modificato. Nessuna rete, compilazione, esecuzione dei tool shader, GPU compute, benchmark, apt, Git o modifica globale.

Modello dichiarato: OpenAI GPT-6 Astra, `openai/gpt-6-astra`; nessuna attestazione indipendente dell'identità. Nessun subagente, costi/token non esposti. Consultati esclusivamente metadati di filesystem del workspace/prefisso, i 12 artefatti del manifest locale, i due deb, i due binari selezionati e il loro alias, i dieci candidati e il keyring pubblico già registrato. Nessuna lettura del sorgente GPU attivo. Il brief delimita il percorso a glslangValidator e spirv-val: gli extra non ne fanno parte; non è una prova dinamica di compilazione.

## Controlli e comandi effettivi

Dalla root: `du -s -B1 tmp`, `stat -c '%n mode=%a uid=%u gid=%g'` sui componenti root/tmp/prefisso, `id -u`, `id -g`. Script Python inline con `python3 -B` e successivamente `python3 -B -S`, senza file script aggiuntivo. Hash SHA-256 letti in streaming; inventario pacchetto con `dpkg-deb --fsys-tarfile tmp/m1-tools/spirv-tools_2026.1-1_amd64.deb` e `tarfile` in modalità `r|`.

Il primo controllo conservativo ha rifiutato `tmp` perché mode 0775, prima di ogni cancellazione o receipt. Il gestore apport ha stampato un errore secondario su input stdin; nessun payload eseguito. La verifica successiva usa la root/tmp autorizzata come antenato condiviso e apre ogni componente con `open(..., O_DIRECTORY|O_NOFOLLOW, dir_fd=...)`. Controllati proprietario UID corrente e assenza di scrittura gruppo/altri da `m1-tools` in giù (namespace 0700). Nessun chmod. Le cancellazioni usano il directory descriptor già aperto, non una nuova risoluzione degli antenati condivisi. Non è una protezione da un altro processo ostile con lo stesso UID o root.

Prima della rimozione: dieci candidati regolari, non symlink, `nlink=1`, UID corrente; esclusi dai percorsi dei 12 artefatti congelati e dai tre percorsi selezionati. Ogni size/SHA-256 coincide con il membro regolare del deb autenticato. Prima di ogni `os.unlink(name, dir_fd=binfd)`, ricontrollati tipo, nlink, device, inode, size e hash. Nessun altro nome ammesso.

Autenticità ricontrollata: `gpgv --keyring /usr/share/keyrings/ubuntu-archive-keyring.gpg --status-fd 1 tmp/m1-tools/InRelease`, con GNUPGHOME/TMPDIR nel namespace locale, firma valida `F6ECB3762474EDA9D21B7022871920D1991BC93C`. Hash del keyring e InRelease confrontati con acquisizione già congelata. I due deb ricontrollati prima e dopo: glslang `2ad3ea819da071f1eff681415d35bc0af03e076ed5ff3993bffb558095adbb4d`; spirv-tools `24e972ed4f2e92ada6f64b32ff40550fda02038385656736af871ca3dcb2b867`. La precedente catena Packages resta registrata e immutata; non è stata ricostruita da rete. Rimangono i limiti del trust anchor locale e dell'assenza di Valid-Until.

## Copie rimosse e spazio fisico

Tutte sotto `tmp/m1-tools/prefix/usr/bin/`: `spirv-as`, `spirv-cfg`, `spirv-diff`, `spirv-dis`, `spirv-lesspipe.sh`, `spirv-link`, `spirv-lint`, `spirv-objdump`, `spirv-opt`, `spirv-reduce`.

Somma `st_blocks × 512` delle sole copie rimosse: **21729280 byte**. Supera i 16969728 byte richiesti di 4759552 byte. Scratch osservato prima: **817225728 byte**, già inferiore alla fotografia nel brief; non si attribuisce la differenza a questa attività. Dopo receipt finale: **795504640 byte**. Riduzione netta osservata 21721088 byte; il nuovo receipt occupa spazio. Limite invariato **1073741824 byte**. Nessuna cancellazione di risultati write-once, archiviazione di prove o modifica dei criteri di qualità.

## Conservazione e verifica finale

- Manifest originale `tmp/m1-tools/manifest.json`: SHA-256 invariato `7a3723f5ff1f37cdce923c6d390af220a34c672adb3fc817e81aa46b975cdb7f`; **12/12 artefatti** verificati prima e dopo.
- `usr/bin/glslang`: inode **29005524**, SHA-256 `bfcc2479c85bdab08c7f7e20425f35d8b9aad176ab5fbbbad3427166c7a259a5`, invariati.
- `usr/bin/spirv-val`: inode **29005548**, SHA-256 `85367fefdb7e93ae45654255ac2b7f8dc7056b6df78a6fdeb03ce395c7477239`, invariati. **2/2 hash runtime** coincidono col record precedente, senza rieseguire i binari.
- Alias `usr/bin/glslangValidator -> glslang`: inode **29005537**, target e metadati invariati. Device osservato **66306**. Nessuna libreria, dipendenza o notice rimossa.

Receipt nuovo: `tmp/m1-tools/minimal-prefix-receipt.json`, SHA-256 **`7559faae1e62c93e18c6a1ba196bd8136342a3274a3db0a464bc337d2cb5c59f`**. Registra per ogni copia nome, inode/device, byte logici/fisici, SHA-256, deb sorgente e SHA-256, membro esatto e condizioni di conservazione. Il vecchio inventario completo descrive l'estrazione originaria e resta intatto; questo receipt descrive il delta autorizzato.

Ripristino possibile byte per byte dal deb integrale conservato, **solo per un futuro nuovo percorso scelto esplicitamente**, con preflight spazio e verifica del membro. Mai ripristino/sostituzione automatica dopo errore runtime. Nessun nuovo risultato M1 né gate GPU dichiarato superato. Il coordinatore può ripetere il preflight quota per D; questo record non prenota spazio contro altri writer.
