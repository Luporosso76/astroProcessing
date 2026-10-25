# M0: ambiente, repository e integrità

Unità: M0.2, audit di ambiente e repository.

Scopo: verificare l'accesso al repository ufficiale, preservare gli input e identificare gli strumenti già disponibili per gli spike.

Autore e data: agente di verifica, 25 ottobre 2026. Provider e modello dichiarati dal runtime: OpenAI, GPT-6.1 Sol (`openai/gpt-6.1-sol`). È un self-report del runtime, non un'attestazione indipendente del backend. Nessun subagente invocato.

Esito attuale: **audit e preparazione Git locali completati**, da verificare con review indipendente del record. L'esito iniziale PARZIALE e i tentativi precedenti sono conservati sotto come storia. Questo record non chiude M0 nel suo insieme. Nessuna compilazione, prova numerica, prova GUI o misura comparativa eseguita. Nessuno stack scelto e nessun gate scientifico chiuso.

Output: stdout/stderr dei tool della sessione, riassunti qui; nessun log grezzo su disco. Costi, token e quote non disponibili nel report di esecuzione: nessuna stima.

## Input e integrità

SHA-256 calcolati sui file presenti, prima della scrittura di questo record. L'elenco comprende anche il baseline 2.2 e l'addendum, per non omettere input storici. Il calcolo dell'hash non equivale a una lettura semantica integrale.

| Percorso relativo alla radice | SHA-256 |
|---|---|
| `ASTRO_POSTPROCESSING_PLAN.md` | `f1b6d4d74cebdc4ff200b3f31b3caa2b151e63b5179b27d4eee762b7695592f9` |
| `ASTRO_POSTPROCESSING_ADDENDUM_ANALISI_INTELLIGENTE_CALIBRAZIONE.md` | `d9cb4cb9dacc013b30231efec2a30dac31dee32151137beef3468dd70c0e1de5` |
| `ASTRO_POSTPROCESSING_MASTER_SPEC_CODEX_PLAN.md` | `d2f7603577510f34af03e5e0e3da1ec989927e039b273c789332c3e98bbea985` |
| `ASTRO_POSTPROCESSING_PLAN_v2.1.md` | `384857061fc7cc79c271dbc114a4a4dccecbe9b792dbe3ad6790bee4b59ce5a3` |
| `ASTRO_POSTPROCESSING_PLAN_v2.2.md` | `8fa0f881bd932c1e468449dea74037bad8cd88bdc8937c7077d1f233aa1f29c1` |
| `RICHIESTA_REVISIONE_ASTRO_POSTPROCESSING_PLAN_CODEX.md` | `2445b29591d3a6e80096362de336ef8ec7918296f2908c7f15b6ae445ce7b874` |
| `RICHIESTA_REVISIONE_ASTRO_POSTPROCESSING_PLAN_v2.2_CODEX.md` | `5e1cabc46695d162b5a7d73dcc7c47ed0791732e49e62134a5ec26883da03ebb` |
| `RICHIESTA_REVISIONE_PLAN_2_2.md` | `9404848d8af7a5930d66856c3a4ac2e6f175d33e8d2d291d173a2be97daf26cd` |
| `F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05` | `ac4a2eee21556b80ad3bc022cca9be9ea9df62684ea34d12b2c9fb254ea7d74e` |
| `ISTRUZIONI_OPERATIVE.md` | `048a2bbe0bf432cf33cad078550c9a10cc9612c2bf1a2323aca1a80573a500a3` |

Comando effettivamente eseguito, nella radice `/opt/workspace/astroProcessor`:

```bash
sha256sum -- ASTRO_POSTPROCESSING_PLAN.md ASTRO_POSTPROCESSING_ADDENDUM_ANALISI_INTELLIGENTE_CALIBRAZIONE.md ASTRO_POSTPROCESSING_MASTER_SPEC_CODEX_PLAN.md ASTRO_POSTPROCESSING_PLAN_v2.1.md ASTRO_POSTPROCESSING_PLAN_v2.2.md RICHIESTA_REVISIONE_ASTRO_POSTPROCESSING_PLAN_CODEX.md RICHIESTA_REVISIONE_ASTRO_POSTPROCESSING_PLAN_v2.2_CODEX.md RICHIESTA_REVISIONE_PLAN_2_2.md 'F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05'
```

Letture testuali di questo agente: mandato corrente integrale, 122 righe; `AGENTS.md` integrale nella prima versione consegnata, 43 righe; `docs/STATUS.md`, 50 righe; `docs/evidence/README.md`, 42 righe; baseline 2.2, righe 3057-3101; intestazione del file anomalo, prime 26 righe, più outline delle sezioni. Non si dichiara una review integrale del baseline. La revisione 2.2.1 in scrittura non è stata letta né modificata.

## Repository: prove eseguite

### Copia locale prima dell'eventuale inizializzazione

`.git` assente. La verifica è stata ripetuta dopo l'autorizzazione operativa:

```bash
env GIT_OPTIONAL_LOCKS=0 GIT_TERMINAL_PROMPT=0 GIT_CONFIG_NOSYSTEM=1 GIT_CONFIG_GLOBAL=/dev/null git rev-parse --is-inside-work-tree --show-toplevel
```

Risultato: exit **128**. Diagnosi: `fatal: .git non è un repository Git (né lo è alcuna delle directory genitrici)`.

Nel primo passaggio anche `git status --short --branch`, `git remote` e `git log -5 --format='%h %cs'` hanno prodotto la stessa diagnosi. La shell del gruppo è terminata con exit 128; i codici individuali di quei tre comandi non sono stati registrati.

### Repository ufficiale pubblico

Comando ripetuto dopo l'autorizzazione operativa:

```bash
env TMPDIR=/opt/workspace/astroProcessor/tmp GIT_TERMINAL_PROMPT=0 GIT_ASKPASS=/bin/false SSH_ASKPASS=/bin/false GIT_CONFIG_NOSYSTEM=1 GIT_CONFIG_GLOBAL=/dev/null GIT_OPTIONAL_LOCKS=0 git -c credential.helper= -c http.extraHeader= ls-remote --symref https://github.com/Luporosso76/astroProcessing.git
```

Risultato: exit **0**, stdout vuoto. Nessuna ref annunciata. Nessuna configurazione utente o credenziale ispezionata; helper e prompt disattivati per il comando.

Nel primo passaggio sono stati consultati anche i seguenti endpoint pubblici, solo per metadati, senza scaricare contenuti di file:

```bash
curl -q --fail --silent --show-error --connect-timeout 10 --max-time 20 -H 'Accept: application/vnd.github+json' https://api.github.com/repos/Luporosso76/astroProcessing
curl -q --silent --show-error --connect-timeout 10 --max-time 20 -H 'Accept: application/vnd.github+json' -w '\nHTTP_STATUS=%{http_code}\n' https://api.github.com/repos/Luporosso76/astroProcessing/branches
curl -q --silent --show-error --connect-timeout 10 --max-time 20 -H 'Accept: application/vnd.github+json' -w '\nHTTP_STATUS=%{http_code}\n' https://api.github.com/repos/Luporosso76/astroProcessing/git/trees/main
```

Risultati: repository `private=false`, `size=0`, `default_branch=main`, `archived=false`, `disabled=false`; branches HTTP **200**, lista vuota; tree HTTP **409**, messaggio `Git Repository is empty.`. Il 409 è una risposta API effettiva, non un errore nascosto. Il comando curl senza `--fail` non trasforma quel codice HTTP in errore di processo. Non risultano storia utile, branch, tag o layout remoto consultabile. `main` è il default dichiarato, non un branch esistente. Permessi di push non provati.

### Preparazione di `.git`: primo handoff, storico

Non ancora eseguita. Il precedente divieto in `AGENTS.md` è stato rimosso durante questo incarico, ma la versione aggiornata prescrive `m0-m1`, mentre l'incarico richiede `work/m0-m1`. Alla domanda mirata il coordinatore ha scelto di allineare prima `AGENTS.md`. Nessun `git init`, clone annidato, add, commit, push, checkout, reset o cancellazione eseguito da questo agente. Il prossimo passo è verificare l'allineamento del nome, ripetere il controllo remoto e inizializzare soltanto se `.git` è ancora assente e le ref remote ancora vuote.

## Strumenti e versioni già presenti

Questa sezione conserva gli esiti del primo passaggio. Non è stato ripetuto l'inventario completo. I probe non compilano codice e non dimostrano l'assenza di fallback interni delle librerie.

Comandi di individuazione: `command -v` per `git clang clang++ gcc g++ cmake ninja cargo rustc rustup python3 pip3 dotnet pkg-config qmake6 qtpaths6 gtk4-launch curl`; `git --version`; `--version` per GCC/G++, CMake e Ninja; `python3 --version`; `pkg-config --version`; `qmake6 -v`; `qtpaths6 --qt-version`.

| Strumento | Esito osservato |
|---|---|
| Git | 2.53.0 |
| GCC/G++ | 15.2.0, stringa Ubuntu `15.2.0-16ubuntu1` |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| pkg-config | 2.5.1 |
| Python | 3.14.4; il comando trovato tramite shim risolve a `/usr/bin/python3` |
| Rust | rustc 1.99.0 (`b940084d7`, 2026-09-28) |
| Cargo | 1.99.0 (`5f94df478`, 2026-08-27) |
| Qt tramite qmake/qtpaths | 6.10.2 |
| Clang/Clang++ | non rilevati in PATH né fra `/usr/bin/clang-[0-9]*` e `/usr/bin/clang++-[0-9]*` |
| .NET | non rilevato in PATH o nei percorsi comuni controllati; mai invocato |

Cargo e rustc in PATH sono link a rustup. Per evitare inizializzazioni o download sono stati invocati direttamente i binari esistenti:

I quattro comandi storici sotto sono redatti per la pubblicazione: `<USER_HOME>` sostituisce il percorso personale originale. Versioni e risultati restano quelli osservati; non sono trascrizioni byte-identiche né comandi da eseguire senza risolvere il segnaposto.

```bash
<USER_HOME>/.rustup/toolchains/1.99.0-x86_64-unknown-linux-gnu/bin/rustc --version
<USER_HOME>/.rustup/toolchains/1.99.0-x86_64-unknown-linux-gnu/bin/cargo --version
<USER_HOME>/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin/rustc --version
<USER_HOME>/.rustup/toolchains/stable-x86_64-unknown-linux-gnu/bin/cargo --version
```

Entrambe le directory hanno riportato le versioni sopra. Non sono stati letti file di configurazione rustup. I percorsi .NET controllati solo come metadati sono `/usr/share/dotnet/dotnet`, `/usr/lib/dotnet/dotnet` e `/opt/dotnet/dotnet`, tutti non rilevati.

### Pacchetti Python

Verifica con `python3 -B` e `/usr/bin/python3 -B`, tramite `importlib.metadata.version` e `importlib.util.find_spec` sui moduli top-level. `PYTHONDONTWRITEBYTECODE=1`. Sono metadati di distribuzione e scopribilità, non prove di importazione, ABI o calcolo numerico.

| Distribuzione | Versione/esito |
|---|---|
| NumPy | 2.3.5, modulo individuabile |
| SciPy | 1.16.3, modulo individuabile |
| PySide6 / PySide2 | distribuzioni assenti; modulo PySide6 non individuabile |
| PyGObject | 3.56.2, modulo `gi` individuabile |
| PyQt6 | 6.10.2 |
| Astropy | 7.2.0 |
| pytest | 9.0.2 |

PyQt6 non è una prova di compatibilità con PySide. Nessuna sostituzione automatica decisa.

### Librerie native

Comando effettivo: ciclo `pkg-config --modversion "$p"` per ciascun modulo elencato. Per moduli mancanti il primo passaggio ha stampato `not discoverable`; stderr era soppresso e il codice individuale non è stato registrato.

| Modulo pkg-config | Versione/esito |
|---|---|
| Qt6Widgets, Qt6Quick, Qt6Core | 6.10.2 |
| Qt5Widgets | 5.15.18 |
| gtk4 | non individuabile |
| gtk+-3.0 | 3.24.52 |
| cfitsio | 4.6.3 |
| libraw, libraw_r | 0.21.5 |
| wcslib | 8.4 |
| fftw3, fftw3f, fftw3l | 3.3.10 |

`ldconfig -p` ha confermato le librerie runtime Qt6, GTK4, CFITSIO, LibRaw, WCSLIB e FFTW. `dpkg-query` ha riportato GTK4 runtime e introspezione `4.22.4+ds-0ubuntu0.1`, e `python3-gi` `3.56.2-1`. GTK4 runtime installato non significa che i file di sviluppo siano disponibili: `gtk4.pc` non è stato trovato. Nessuna installazione eseguita.

## Fotografia dell'ambiente, non riferimento prestazionale del prodotto

Ambiente: workspace `/opt/workspace/astroProcessor`; area temporanea già presente `tmp/`. Nei probe configurabili: `TMPDIR=/opt/workspace/astroProcessor/tmp`, `PYTHONDONTWRITEBYTECODE=1`. Nessun .NET invocato e nessuna inizializzazione deliberata di cache esterne. Nessuna impostazione di sistema modificata.

Comandi: `uname -sm`, `getconf LONG_BIT`, `getconf _NPROCESSORS_ONLN`, lettura di `MemTotal`, `MemAvailable`, `SwapTotal` da `/proc/meminfo`, lettura dei soli limiti cgroup standard disponibili, `os.sched_getaffinity(0)`, `time.get_clock_info`, controlli di presenza delle variabili display e dei device grafici.

- Linux x86_64, 64 bit. Release del kernel e tipo di filesystem non rilevati.
- 16 CPU logiche; affinità del processo su 16 CPU, cpuset effettivo `0-15`.
- `MemTotal: 31591684 kB`; `MemAvailable: 16139968 kB` nella prima lettura; `SwapTotal: 91644344 kB`.
- Limite effettivo di memoria e quota CPU non determinati: i file standard cgroup v1/v2 cercati non erano leggibili/presenti. Non se ne deduce assenza di limiti.
- `perf_counter` e `monotonic`: `clock_gettime(CLOCK_MONOTONIC)`; `process_time`: `clock_gettime(CLOCK_PROCESS_CPUTIME_ID)`; risoluzione dichiarata 1e-09 secondi. Non è una precisione misurata.
- `DISPLAY`, `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR`, `XDG_SESSION_TYPE` impostati; valori non raccolti nel report.
- `/dev/dri/card1` e `/dev/dri/renderD128` accessibili in lettura/scrittura; `/dev/nvidia0` assente. Driver, API GPU e funzionamento GUI non provati.
- `taskset` e `perf` trovati; nessuna profilazione eseguita.

Questi valori descrivono una sessione. Non fissano budget, prestazioni minime, macchina di riferimento o requisiti del prodotto.

## File anomalo e directory storica

Comandi eseguiti:

```bash
stat --printf='%F | %s bytes | %n\n' -- 'F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05' doc_old tmp
file -- 'F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05'
head -n 26 -- 'F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05'
wc -lc -- 'F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05' ASTRO_POSTPROCESSING_PLAN_v2.2.md
cmp -s -- 'F-ANA-02.b, F-ANA-03.b, F-ANA-04.b, F-UX-05' ASTRO_POSTPROCESSING_PLAN_v2.2.md
find doc_old -mindepth 1 -maxdepth 2 -printf '%y %s %p\n'
```

Risultati: file regolare UTF-8, non symlink, 367039 byte, 3275 righe. Intestazione di piano versione 2.2. `cmp` exit **1**: diverso dal baseline ufficiale, 430316 byte e 4452 righe. Provenienza non dimostrata; nessuna cancellazione, rinomina o normalizzazione. `doc_old` è una directory, 4096 byte di metadati; nessuna voce interna, incluse quelle nascoste. `tmp/` già esistente. Nessun indice CodeGraph presente né creato.

## Modelli e limiti della prova

Il tool `tools.opencode.models({ limit: 100 })` ha restituito 27 modelli, tutti riportati come `active`, senza pagine ulteriori: OpenAI 5, Google 13, Morph 2, OpenCode 7. È un catalogo, non la prova di 27 modelli eseguiti o accessibili con quota disponibile. La presenza di Morph nel catalogo non ne autorizza l'uso: nessun modello Morph è stato invocato. Quote e disponibilità effettiva di ciascun modello non verificate. Nessuna prova di costo o token, nessuna attestazione inventata. Nel catalogo degli specialisti esposto a questa sessione non compare un agente astrofisico dedicato.

Ambito del primo passaggio: verifica repository pubblico, integrità e disponibilità dichiarata degli strumenti. Sono esclusi compilazione/link, benchmark, import numerici, verifica delle licenze, funzionamento GPU/GUI, push, validazione scientifica, scelta dello stack e prove del requisito no-fallback. Nel seguito sono stati aggiunti soltanto metadati minimi di licenza, non un'approvazione legale.

## Verifica finale del primo handoff, storico

Un controllo con `/usr/bin/python3 -B`, `hashlib.sha256` e parsing della tabella di questo record ha confrontato tutti i nove hash registrati con i file attuali: **9/9 identici**, exit **0**. Il baseline 2.2 conserva `8fa0f881bd932c1e468449dea74037bad8cd88bdc8937c7077d1f233aa1f29c1`; il file anomalo conserva `ac4a2eee21556b80ad3bc022cca9be9ea9df62684ea34d12b2c9fb254ea7d74e`. `doc_old/` ancora vuota; `.git` ancora assente.

Il comando `env GIT_CONFIG_NOSYSTEM=1 GIT_CONFIG_GLOBAL=/dev/null GIT_OPTIONAL_LOCKS=0 git --no-pager diff --no-index --check -- /dev/null docs/evidence/M0-environment.md` ha restituito exit **1**, senza diagnosi su stdout/stderr. Non è contato come lint superato. Non sono stati eseguiti test applicativi.

Un successivo controllo indipendente con Python ha verificato nuovamente i nove hash, la newline finale del record e l'assenza di spazi a fine riga: exit **0**. È un controllo di integrità e forma del documento, non compilazione, lint del codice o gate scientifico.

L'ultima verifica della regola di branch, `grep -n 'branch di lavoro' AGENTS.md`, trova ancora `m0-m1` alla riga 18. Come scelto dal coordinatore, l'inizializzazione attende l'allineamento a `work/m0-m1`. Origin, branch e stato untracked dopo inizializzazione non sono verificabili finché `.git` non esiste. Nessun add, commit o push.

## Seguito M0.2: repository ufficiale preparato e branch allineato

Il coordinatore ha comunicato di avere eseguito un nuovo `git ls-remote` (exit 0, nessuna ref), `git init` sul branch `m0-m1` e aggiunta dell'origin ufficiale. L'inizializzazione è un'operazione del coordinatore, non attribuita a questo agente. Questo seguito ne verifica lo stato locale e modifica soltanto il riferimento simbolico di un branch unborn, senza checkout o riscrittura di commit.

Comandi eseguiti in `/opt/workspace/astroProcessor`, con `TMPDIR=/opt/workspace/astroProcessor/tmp`, `GIT_CONFIG_NOSYSTEM=1`, `GIT_CONFIG_GLOBAL=/dev/null`, `GIT_OPTIONAL_LOCKS=0`, `GIT_TERMINAL_PROMPT=0`:

```bash
git rev-parse --is-inside-work-tree --show-toplevel
git symbolic-ref HEAD
origin=$(git config --local --get remote.origin.url)
test "$origin" = 'https://github.com/Luporosso76/astroProcessing.git'
git rev-parse --verify --quiet HEAD
git for-each-ref --format='%(refname)'
git symbolic-ref HEAD refs/heads/work/m0-m1
git symbolic-ref --short HEAD
git status --short --branch --untracked-files=normal
git ls-files | wc -l
git for-each-ref --format='%(refname)'
```

Risultati e guardie effettive:

- `rev-parse`: exit **0**, `true`, radice `/opt/workspace/astroProcessor`.
- `symbolic-ref HEAD` iniziale: exit **0**, `refs/heads/m0-m1`.
- Origin locale confrontato con la stringa attesa: **uguale**, `https://github.com/Luporosso76/astroProcessing.git`. Nessun altro campo di configurazione letto o stampato.
- `rev-parse --verify --quiet HEAD`: exit **1**, nessun commit; `for-each-ref` vuoto. La mutazione era condizionata a questi risultati e al nome iniziale atteso; in caso diverso non sarebbe stata eseguita.
- `git symbolic-ref HEAD refs/heads/work/m0-m1`: exit **0**.
- Nome finale: `work/m0-m1`; `git status`: exit **0**, intestazione `## Non ci sono ancora commit su work/m0-m1`, soltanto voci `??`.
- File tracciati: **0**; riferimenti a commit: nessuno. Nessun add, commit, push, reset, checkout, clone annidato o cancellazione eseguito da questo agente.

Il passaggio non verifica credenziali o permessi di push. Il ramo è pronto per il lavoro locale; la review del record resta distinta dalla chiusura globale M0.

### Letture stabili del seguito

Letti integralmente: `AGENTS.md` 52 righe, `PLANS.md` 46, `plans/M0-M1.md` 97, `docs/test-matrix.md` 34, `docs/progress.md` 66, `ISTRUZIONI_OPERATIVE.md` 164. La 2.2.1 non è stata aperta. Hash degli input operativi, rilevati con `sha256sum -- AGENTS.md PLANS.md plans/M0-M1.md docs/test-matrix.md docs/progress.md ISTRUZIONI_OPERATIVE.md`:

```text
9f238dba19ae175ae2d8b136162d9735e8b0a8d94b14b4057c4c174c512bb464  AGENTS.md
8900b7d91aa27fd4b9f0c818bc4f5bca8ab2fd6ed161ad27cf035afa56543889  PLANS.md
85b0d333aae89961bfdbf8810a91e6311b3c59509cdf5a4704ccd65333065c6b  plans/M0-M1.md
47e6492e12fa5c8d642b1eeb5d2f89033f0517f88acce3cf641184bf5ec4dc53  docs/test-matrix.md
8721c6b51e9adc40a87ecd0407a92163287e0bfdd9cf96bc4b07a9e250c12ff2  docs/progress.md
048a2bbe0bf432cf33cad078550c9a10cc9612c2bf1a2323aca1a80573a500a3  ISTRUZIONI_OPERATIVE.md
```

Sono snapshot degli input letti, non vincoli per impedire al coordinatore di aggiornarli. Nessuno di questi file è stato modificato da questo agente.

## Probe tecnici preparatori di M1.e, eseguiti senza benchmark

Il seguente ciclo è stato eseguito, registrando il codice di ciascun modulo:

```bash
for p in vulkan OpenCL Qt6Gui shaderc shaderc_combined glslang SPIRV-Tools egl gl libdrm lcms2 sqlite3 openssl; do
  printf '%s: ' "$p"
  pkg-config --modversion "$p"
  printf 'pkg-config exit=%s\n' "$?"
done
```

Exit **0**: Vulkan **1.4.341**, Qt6Gui **6.10.2**, EGL **1.5**, GL **1.2** (versione del modulo pkg-config, non capacità OpenGL del dispositivo), libdrm **2.4.131**, LittleCMS **2.17**, SQLite **3.46.1**, OpenSSL **3.5.5**.

Exit **1**, `Package ... not found`: OpenCL, shaderc, shaderc_combined, glslang, SPIRV-Tools. Header presenti: `/usr/include/vulkan/vulkan.h`, `/usr/include/GL/gl.h`, `/usr/include/EGL/egl.h`, `/usr/include/sqlite3.h`. Assenti: `/usr/include/CL/cl.h`, `/usr/include/CL/opencl.h`, `/usr/include/shaderc/shaderc.h`, `/usr/include/glslang/Public/ShaderLang.h`, `/usr/include/gtk-4.0/gtk/gtk.h`.

Il probe `command -v` ha trovato `vulkaninfo`, `vkcube`, `glxinfo`, `eglinfo`, `openssl`, `sqlite3`. Non ha trovato `glslangValidator`, `glslc`, `spirv-val`, `spirv-dis`, `clinfo`, `clspv`, `drm_info`, `nvcc`, `nvidia-smi`, `hipcc`, `rocminfo`, `qsb`, `b3sum`. I percorsi Qt6 consueti controllati non contengono `qsb`. `/usr/bin/qml` e `/usr/bin/qmlscene` risolvono a `qtchooser`: non provano la presenza di un runner QML Qt6.

`ldconfig -p` trova i loader Vulkan e OpenCL e le librerie EGL, SQLite e LittleCMS. `/usr/share/vulkan/icd.d` contiene manifest `asahi`, `virtio`, `intel_hasvk`, `lvp`, `radeon`, `intel`, `nouveau`, `gfxstream`; sono nomi di driver installati, non dispositivi effettivi. `/etc/OpenCL/vendors` assente. Nessuna enumerazione di dispositivi, creazione di contesto o esecuzione GPU: `vulkaninfo`, `vkcube`, `glxinfo` ed `eglinfo` non sono stati lanciati.

`dpkg-query -W -f='${binary:Package} ${Version} ${db:Status-Abbrev}\n'` sui pacchetti GPU/dev richiesti ha riportato Vulkan loader/dev **1.4.341.0-1**, Mesa Vulkan **26.0.8-1ubuntu0.3**, OpenCL loader **2.3.4-1**, SQLite dev **3.46.1-9ubuntu0.3**, LittleCMS dev **2.17-1ubuntu0.2**. Il gruppo termina con exit **1** perché alcuni pacchetti richiesti non esistono; non annulla le righe installate. Un secondo probe trova `vulkan-tools` **1.4.341.0+dfsg1-1** e `mesa-utils` **9.0.0-2build1**.

Conclusione di disponibilità, non benchmark: Vulkan ha loader/header, ma nessun compilatore shader o validatore SPIR-V rilevato; OpenCL ha solo il loader, senza header o ICD rilevati. OpenGL/EGL hanno header/tool, ma capacità compute, dispositivo hardware e controllo dei fallback restano da verificare. Nessun backend GPU scelto. Il ramo GPU non può vincere per presunzione e non blocca gli spike CPU indipendenti.

## Metadati minimi di licenza dei candidati M1

Probe eseguiti: `importlib.metadata` sulle distribuzioni Python, senza import dei candidati; estrazione dei campi `Files:` e `License:` dai copyright pubblici dei pacchetti. Comando copiabile dell'ultimo passaggio:

```bash
for pkg in qt6-base-dev qt6-declarative-dev libfftw3-dev libsqlite3-dev libssl-dev libvulkan-dev; do
  printf '\n%s\n' "$pkg"
  awk '/^Files:|^License:/{print}' "/usr/share/doc/$pkg/copyright" | head -n 10
done
```

- NumPy **2.3.5**, SciPy **1.16.3**: classificatore `License :: OSI Approved :: BSD License`; `License-Expression` assente. Il testo completo e le dipendenze transitive non sono stati verificati.
- PyQt6 **6.10.2**: `License-Expression: GPL-3.0-only`; PyGObject **3.56.2**: `LGPL-2.1-or-later`. Il primo non va confuso con Qt C++ né con una licenza commerciale già disponibile.
- Qt base/declarative: stanza `Files: *`, `LGPL-3 or GPL-2`; esistono stanzas diverse, per esempio qmlcompiler con `GPL-3 with Qt-1.0 exception`. La compatibilità dei moduli effettivamente linkati resta da analizzare.
- FFTW: `Files: *`, `GPL-2+`; una prova locale isolata non autorizza la sua integrazione nel prodotto.
- SQLite: `Files: *`, `public-domain`; `GPL-2+` appartiene alla stanza `debian/*`, non attribuito al core SQLite.
- OpenSSL: `Files: *`, `Apache-2.0`; Vulkan loader/dev: `Files: *`, `Apache-2.0`, con altri file MIT.
- I documenti pubblici della toolchain Rust esistono come `share/doc/rust/COPYRIGHT.html` e `COPYRIGHT-library.html`. La ricerca delle sole etichette nel primo trova Apache-2.0, MIT, BSD-2-Clause, BSD-3-Clause, Unicode-3.0: non è una mappa completa delle licenze del runtime. I tre percorsi di testo inizialmente cercati erano assenti.
- `blake3` Python assente; `b3sum` assente. Nessuna dipendenza BLAKE3 installata o scaricata.

`sqlite3 -version`: **3.46.1**, exit **0**. `OPENSSL_CONF=/dev/null openssl version`: **3.5.5**, exit **0**. `libasan.so.8` e `libubsan.so.1` individuate tramite `ldconfig -p`, non ancora provate in compilazione. Nessuna licenza del progetto emessa, nessuna adozione di dipendenze o verifica legale finale dichiarata.

## Handoff e limiti residui

`plans/M1-spikes.md` preregistra sei unità, con M1.e per prima. Tutti i risultati empirici sono NON ESEGUITI. In questo incarico non si crea codice in `tmp/` e non si avvia M1: il coordinatore registra prima la chiusura M0. I cataloghi reali e i contratti M3 non sono prerequisiti artificiali degli esperimenti analitici sintetici.

Il prossimo passo verificabile è la review indipendente di questo record e della preregistrazione, seguita dall'evento di chiusura M0 in `docs/progress.md`. Solo dopo si preparano i sorgenti isolati e si eseguono i comandi pianificati. Nessun commit o push in questo incarico.

### Verifica di chiusura del seguito

Validazione documentale eseguita tramite `/usr/bin/python3 -B`: **10/10 hash degli input immutati**, newline finale e assenza di spazi terminali nei due file, fence Markdown bilanciate, link locali esistenti, sei unità M1 nell'ordine e/a/b/c/d/f. Undici blocchi shell verificati con `/bin/bash --noprofile --norc -n`, su stdin e con ambiente minimo: sintassi valida, comandi non eseguiti. Il gruppo termina con exit **0**. Questa prova non compila né esegue il prototipo.

La preregistrazione consegnata è `plans/M1-spikes.md`, **167 righe**, SHA-256 `40690ab08e5a1a86560097c07165c2343ec4cbc93079e42fbbef6b72f103f46a`. Il suo hash identifica questa serie proposta; ogni modifica prima degli esperimenti richiede una nuova registrazione. L'hash del presente record è comunicato nell'handoff, non incluso nel suo stesso contenuto.

Controllo finale Git: `work/m0-m1`, nessun commit, zero file tracciati, solo untracked. `tmp/` e `doc_old/` vuote al controllo. L'agente ha scritto solo questo record, la nuova preregistrazione e il riferimento simbolico HEAD autorizzato. Gli altri documenti e la revisione 2.2.1 non sono stati modificati. Audit/inizializzazione M0.2 consegnati per la verifica del coordinatore; M0 globale e M1 non dichiarate chiuse.
