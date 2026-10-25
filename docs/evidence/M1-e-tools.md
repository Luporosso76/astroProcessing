# M1.e: preparazione locale dei tool shader

Data: 2026-10-25, acquisizione alle 16:31:55 UTC. **PRONTI i tool per il successivo tentativo esplicito del candidato Vulkan; compute NON ESEGUITO e backend NON QUALIFICATO.** Non è un fallback: si prepara il candidato prima della sua esecuzione. Nessuna conclusione sulle prestazioni GPU.

Autore: OpenAI GPT-6 Astra, `openai/gpt-6-astra`, identità dichiarata dall'ambiente, non verificata indipendentemente. Nessuna subdelega. Costi e token non esposti.

## Perimetro effettivo

Letti `AGENTS.md` 1-52, `docs/progress.md` 1-37, `docs/evidence/M1-e.md` 1-41. Il brief corrente aggiorna lo stato delle misure CPU rispetto al progress letto; questo record non modifica lo stato centrale. Non letti namespace attivi d/c/ownership, sorgenti M1, configurazioni private o credenziali. Nessuna modifica a report M1.a/e, piano, Git, index, servizi, sistema o licenza del prodotto.

Scritto soltanto questo nuovo record e il nuovo namespace `tmp/m1-tools/`. Nessun apt update/install, postinst, ldconfig, shim, installazione globale, compilazione, shader o kernel compute. Eseguiti dai payload soltanto `--version`, dopo autenticazione e verifica dei diritti d'uso locale.

## Preflight spazio e memoria

Comandi iniziali: `du -s -B1 tmp`, `df -B1 .`, controllo che `tmp/m1-tools` non esistesse, `command -v gpgv`, `command -v dpkg-deb`, `ls -l /usr/share/keyrings/ubuntu-archive-keyring.gpg`, `date -u +%FT%TZ`.

- Scratch iniziale: **959541248 byte**, limite 1073741824 byte. Disco disponibile: 124795604992 byte.
- InRelease limitato a 1 MiB; indice compresso effettivo 15960824 byte. Nessun indice decompresso scritto.
- Prima di ogni download lo script controlla uso scratch + dimensione attesa + riserva estrazione 48 MiB. Buffer di rete 1 MiB; parsing incrementale LZMA, stanza massimo 4 MiB. Python con limite address-space 512 MiB; non è una misura RSS né un limite aggregato dell'intero sistema.
- Prima dell'estrazione: 981262336 byte occupati, riserva richiesta 36754540 byte. Dimensione dei membri degli archivi: 32560236 byte; margine aggiuntivo 4 MiB.
- Ultimo controllo dopo manifest: **1013936128 byte**, circa 57 MiB ancora disponibili. Nessuna cancellazione di scratch altrui. Sono controlli campionati, non quota atomica contro crescita concorrente: ripetere prima del prossimo lavoro.

## Catena di autenticità verificata

Trust anchor locale preesistente, letto soltanto come metadato tecnico pubblico: `/usr/share/keyrings/ubuntu-archive-keyring.gpg`, SHA-256 `80a36b0a6de2f69f49d2df75ef473ccde121e9e190b9ea01d20a4f63778d5c31`. La sua installazione/provenienza originaria non è stata auditata indipendentemente.

Fonte release: <https://archive.ubuntu.com/ubuntu/dists/resolute/InRelease>. Suite/codename `resolute`, data firmata `Thu, 23 Apr 2026 17:07:15 UTC`. SHA-256 `45f95ce276cdba3e41870516a130e03c58b8b7a79e9546b0efe9e526d255740c`.

`gpgv` ha restituito firma valida e `VALIDSIG F6ECB3762474EDA9D21B7022871920D1991BC93C`, Ubuntu Archive Automatic Signing Key (2018). Verifica ripetuta nello script prima di fidarsi dei digest. Nessun `Valid-Until` presente: autenticità rispetto al trust anchor verificata, freshness/replay non garantiti; non si pretende che siano gli ultimi aggiornamenti o pacchetti senza vulnerabilità.

Indice: <https://archive.ubuntu.com/ubuntu/dists/resolute/universe/binary-amd64/Packages.xz>, SHA-256 `1587be86d66d38542325215e0e109f75bd695c8f24d793ace2762038a6ad581e`, 15960824 byte. Dimensione e digest confrontati con la sezione SHA256 dell'InRelease firmato prima di decomprimerlo in streaming.

| Pacchetto | Versione / byte | SHA-256 verificato | URL HTTPS ufficiale |
|---|---|---|---|
| glslang-tools | 16.2.0-2 / 2642688 | `2ad3ea819da071f1eff681415d35bc0af03e076ed5ff3993bffb558095adbb4d` | <https://archive.ubuntu.com/ubuntu/pool/universe/g/glslang/glslang-tools_16.2.0-2_amd64.deb> |
| spirv-tools | 2026.1-1 / 2934512 | `24e972ed4f2e92ada6f64b32ff40550fda02038385656736af871ca3dcb2b867` | <https://archive.ubuntu.com/ubuntu/pool/universe/s/spirv-tools/spirv-tools_2026.1-1_amd64.deb> |

Versione, architettura amd64, dimensione e SHA256 dei record autenticati coincidono con i pin del brief. Quei pin erano metadati APT preesistenti comunicati dal coordinatore, non firme: la catena appena verificata è evidenza distinta. I byte dei due deb sono stati verificati al download e nuovamente prima di estrarre. La firma autentica l'archivio Ubuntu attraverso i digest, non una firma individuale Khronos dei binari né una build riproducibile.

## Copyright e limiti legali effettivi

Letti integralmente i copyright dei due deb autenticati, prima dell'estrazione, attraverso `dpkg-deb --fsys-tarfile` e `tarfile` in streaming. Conservati senza modifiche sotto `prefix/usr/share/doc/{glslang-tools,spirv-tools}/copyright`.

| Componente | Evidenza e lettura | Decisione limitata |
|---|---|---|
| glslang | Copyright distribuzione: BSD-3-clause, MIT, Apple-MIT-license, Apache-2.0 e voci GPL-3. Fonte indicata: <https://github.com/KhronosGroup/glslang>. Letto <https://raw.githubusercontent.com/KhronosGroup/glslang/16.2.0/LICENSE.txt>, con termini BSD/MIT/Apache/GPL ed eccezione; parte finale NVIDIA troncata dal tool, ma testo NVIDIA letto nel copyright del deb | Uso locale del tool non modificato consentito dalle concessioni lette. Non etichettare l'intero pacchetto come BSD-only o interamente permissivo |
| Parser glslang | Letto il preambolo dei primi 4200 caratteri di <https://raw.githubusercontent.com/KhronosGroup/glslang/16.2.0/glslang/MachineIndependent/glslang_tab.cpp>: "As a special exception ... distribute that work under terms of your choice" | L'eccezione Bison è esplicita upstream, omessa dalla voce sintetica GPL-3 del copyright distribuzione. Non provata corrispondenza delle patch Debian al sorgente upstream; nessuna approvazione alla redistribuzione del tool |
| SPIRV-Tools | Copyright distribuzione: Apache-2.0 per upstream e debian/*. Fonte <https://github.com/KhronosGroup/SPIRV-Tools>; URL versione richiesto con HTTP 200: <https://raw.githubusercontent.com/KhronosGroup/SPIRV-Tools/v2026.1/LICENSE>. Output congiunto troncato, non contato come lettura integrale autonoma; testo completo Apache letto nel LICENSE glslang | Uso locale consentito. Redistribuzione futura richiede licenza, notices/NOTICE applicabili, attribuzioni e indicazione delle modifiche |

Precisazione: la GPL §2 letta afferma "unlimited permission to run the unmodified Program". La classificazione mista del pacchetto non rende illecito il suo uso come tool locale e non impone la licenza del prodotto. L'estrazione è una copia locale privata; non è stata autorizzata o effettuata la distribuzione di questi tool. La permissività del core e l'eccezione Bison non cancellano le notices dei singoli file. Nessuna garanzia generale sui diritti di futuri shader/input/output, né consulenza legale certificata.

Contraddizione da conservare: LICENSE upstream dice "Bison was removed long ago", ma lo stesso tag contiene parser generato con Bison 3.8.2 ed eccezione. Non usarne una frase isolata come prova dell'assenza di componenti GPL. Prima di eventuale bundling, verificare sorgente/patch e obblighi del pacchetto esatto.

## Comandi ed estrazione sicura

Sequenza effettiva, dalla root; implementazione di acquisizione completa conservata in `tmp/m1-tools/acquire.py`:

```sh
mkdir -m 700 tmp/m1-tools tmp/m1-tools/gnupg
TMPDIR="$PWD/tmp/m1-tools" curl -q --fail --silent --show-error --location --proto '=https' --max-time 90 --max-filesize 1048576 'https://archive.ubuntu.com/ubuntu/dists/resolute/InRelease' -o tmp/m1-tools/InRelease
GNUPGHOME="$PWD/tmp/m1-tools/gnupg" gpgv --keyring /usr/share/keyrings/ubuntu-archive-keyring.gpg --status-fd 1 tmp/m1-tools/InRelease > tmp/m1-tools/signature.txt 2> tmp/m1-tools/signature-stderr.txt
TMPDIR="$PWD/tmp/m1-tools" PYTHONDONTWRITEBYTECODE=1 python3 -B tmp/m1-tools/acquire.py
```

Lo script usa URL ufficiali HTTPS, controlla redirect sullo stesso host/percorso, size e hash, salva i due soli record necessari. Nessuna lettura di configurazione curl personale (`-q`), nessuna credenziale.

Ispezione tramite `dpkg-deb --fsys-tarfile DEB`, `tarfile.open(..., mode='r|')`: tutti i nomi normalizzati relativi; nessun `..` in uscita, device/FIFO o setuid/setgid; symlink/hardlink con target confinato; nessun membro discende da symlink dell'archivio, verificato anche sull'unione dei due archivi. Inventario in `archive-inspection.json`. I copyright sono stati letti in memoria senza eseguire payload.

Solo dopo quei controlli: `dpkg-deb -x tmp/m1-tools/glslang-tools_16.2.0-2_amd64.deb tmp/m1-tools/prefix` e analogo per `spirv-tools_2026.1-1_amd64.deb`, chiamati tramite `subprocess.run(..., check=True)`. Prefix nuovo. Controllati tutti i symlink estratti con `resolve().is_relative_to(prefix)`. Nessun maintainer script eseguito; nessun contenuto copiato in `/usr`.

`dpkg-deb -f DEB Package Version Depends Installed-Size` conferma dipendenze libc6 >=2.38 e libstdc++6 >=13.1, oltre spirv-tools >=2026.1 per glslang. `dpkg-query -W` limitato a libc6/libstdc++6: **2.43-2ubuntu2.4**, **16-20260322-1ubuntu1**. Nessuna modifica del database pacchetti.

## Prova runtime minima

Eseguiti soltanto i due eseguibili estratti con `--version`, timeout 15 secondi, ambiente ristretto: PATH `/usr/bin:/bin`, HOME/TMPDIR sotto `tmp/m1-tools`, XDG_CACHE_HOME sotto lo stesso namespace, LC_ALL=C. Il processo Python aveva RLIMIT_AS 512 MiB; limite ereditato, RSS non misurata.

- `tmp/m1-tools/prefix/usr/bin/glslangValidator --version`: exit **0**, `Glslang Version: 11:16.2.0`, GLSL 4.60, SPIR-V `0x00010600`.
- `tmp/m1-tools/prefix/usr/bin/spirv-val --version`: exit **0**, `SPIRV-Tools v2026.1 unknown hash, 2026-02-06T09:16:33+00:00`; elenca SPIR-V fino a 1.6 e target Vulkan fino a 1.4. `unknown hash` è metadato build upstream mancante, non fallimento dell'hash del deb verificato.

Percorsi risolti, hash degli eseguibili, argv, exit, stdout/stderr: `runtime-version.json`. Nessuna selezione GPU, compilazione o validazione shader eseguita. Tool di verifica preesistenti: gpgv 2.4.8/libgcrypt 1.12.0, dpkg-deb 1.23.7 amd64, Python 3.14.4.

## Manifest, verificabilità e consegna

Manifest: `tmp/m1-tools/manifest.json`, SHA-256 **`7a3723f5ff1f37cdce923c6d390af220a34c672adb3fc817e81aa46b975cdb7f`**. Collega hash di script, InRelease, firma/log, indice, record, inventario, runtime, diritti e notices. Le provenienze dei deb sono in `acquisition-manifest.json`; le limitazioni legali in `rights.json`. Hash del manifest non equivale a firma dell'autore.

Comandi di verifica già eseguiti: `sha256sum tmp/m1-tools/manifest.json`, `du -s -B1 tmp`, versioni degli strumenti. Tutti i controlli dichiarati sono reali; non esiste un risultato compute.

Prossimo passo del coordinatore: ripetere budget scratch e verifica manifest; usare questi percorsi assoluti nel nuovo tentativo Vulkan. Poi compilazione del prototipo, selezione vincolante dell'adattatore hardware, error injection e misure preregistrate nel ramo competente. Non modificare o rietichettare i vecchi probe. Il blocco "compilatore shader assente" è rimosso per questo prefix; restano aperti correttezza compute, assenza di fallback interni e prestazioni. Gli altri rami continuano indipendentemente.
