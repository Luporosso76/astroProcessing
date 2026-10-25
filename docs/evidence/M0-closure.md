# M0: verifica di chiusura

Data: 25 ottobre 2026. Autore: coordinatore GPT-6.1 Sol, identità dichiarata dal runtime, non attestata. Ambito: minimi documentali e operativi per avviare gli spike sintetici isolati M1. Nessun test del prodotto o benchmark M1 è dichiarato eseguito da questo record.

## Evidenze e review

- Piano attivo: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, 4482 righe, SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`.
- Review indipendente: [M0-plan-review](M0-plan-review.md), architect `ses_e8cd7a18effet7SEK9Mx757TMg`, GPT-6 Astra autodichiarato, **PASS documentale**. Copre il mandato, le istruzioni, il diff effettivo e verifiche mirate; non è una nuova lettura integrale del piano né una prova empirica.
- La review operativa `ses_e8c728004ffePMe5c5ViqYDFye`, GLM-5.3 Flash Free autodichiarato, ha letto i documenti operativi. La risposta era parzialmente degradata: si recepiscono soltanto i rilievi verificabili su stato obsoleto, ricevuta d'archivio e conteggio shell. Non si accredita un audit completo sulla base delle parti illeggibili.
- La review sicurezza/privacy `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, GPT-6 Astra autodichiarato, ha controllato i candidati documentali alla pubblicazione: nessun pattern di segreto rilevato; richiesto oscuramento del nome personale nei percorsi toolchain. Oscuramento applicato nel record ambiente e nella preregistrazione, con distinzione fra comandi storici anonimizzati e comandi futuri parametrizzati. La pubblicazione richiede ancora verifica dei file effettivamente selezionati.

## Minimi M0 verificati

| Unità | Evidenza | Esito limitato |
|---|---|---|
| M0.1 | Review indipendente del delta, hash del piano ricontrollato | PASS documentale; snapshot PENDING superato dal verbale esterno |
| M0.2 | [Ambiente](M0-environment.md), repository ufficiale e branch verificati | PASS preparazione; disponibilità non equivale a benchmark |
| M0.3 | D-19/D-20 P0; D-21 preferenza approvata condizionata ai diritti | PASS recepimento; nessun pack adottato, licenza del prodotto riservata |
| M0.4 | [Policy](../decisions/M0-policy.md), inventario e [dossier](M0-catalogs-and-science.md) | PASS politica d'ingresso; licenze dei pack e SBOM di release restano ai propri gate |
| M0.5 | [Matrice](../test-matrix.md), tre configurazioni candidate e criteri | PASS protocollo; nessuna compatibilità Linux misurata |
| M0.6 | [Preregistrazione](../../plans/M1-spikes.md), due blocchi shell verificati con bash -n | PASS preparazione; nessuno spike eseguito |

Il limite legale dei cataloghi riguarda il ramo interessato, non gli esperimenti sintetici generici. La futura validazione C-12/C-15/C-16 non è un prerequisito retroattivo di M1. D-19/D-20 non vengono riaperte.

## Controllo ripetuto dal coordinatore

Eseguito dalla radice con `/usr/bin/python3 -B`, exit **0**. Input: piano attivo, manifest in `plans/M0-M1.md`, otto file archiviati e preregistrazione. Versioni e ambiente tecnico sono nel record M0-environment; questa verifica usa soltanto letture, SHA-256, parser testuale, bash -n e metadati Git.

```python
from pathlib import Path
import hashlib, re, subprocess
root = Path.cwd()
assert hashlib.sha256((root/'ASTRO_POSTPROCESSING_PLAN_v2.2.1.md').read_bytes()).hexdigest() == '95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094'
manifest = (root/'plans/M0-M1.md').read_text()
archived = re.findall(r'^\| `([^`]+)` \| `([0-9a-f]{64})` \| `doc_old/', manifest, re.M)
assert len(archived) == 8
for name, digest in archived:
    p = root/'doc_old'/name
    assert p.is_file() and not p.is_symlink() and not (root/name).exists()
    assert hashlib.sha256(p.read_bytes()).hexdigest() == digest
blocks = re.findall(r'^```(?:bash|sh)\s*\n(.*?)^```\s*$', (root/'plans/M1-spikes.md').read_text(), re.M | re.S)
assert len(blocks) == 2
for block in blocks:
    subprocess.run(['/bin/bash', '--noprofile', '--norc', '-n'], input=block, text=True, check=True)
assert subprocess.check_output(['git', 'symbolic-ref', '--short', 'HEAD'], text=True).strip() == 'work/m0-m1'
assert subprocess.check_output(['git', 'remote', 'get-url', 'origin'], text=True).strip() == 'https://github.com/Luporosso76/astroProcessing.git'
assert not subprocess.check_output(['git', 'ls-files'], text=True)
subprocess.run(['git', 'check-ignore', '-q', 'tmp/m1/src/probe.cpp'], check=True)
```

Output osservato: **piano invariato; archivio 8/8 con nomi e hash preservati; sintassi bash 2/2; branch/origin corretti; zero file tracciati; tmp esclusa**. Lo zero dei file tracciati descrive questo controllo iniziale, non un vincolo sui commit successivi.

| Record verificato | Righe | SHA-256 |
|---|---:|---|
| `plans/M1-spikes.md` | 169 | `93468f1ceb4afcd735a822a3b87589e963917260ce6d87cfdf0269c6721cc9ff` |
| `M0-environment.md` | 295 | `df0031c30826c8258c062621d2ddb9d9e5f300cc8bc40a2c339a8d7b9b3512a9` |
| `M0-plan-review.md` | 114 | `30b78d9a7d41b8b58ce81472ff3c0ebac5a056087620f27d77367a278e037eeb` |
| `M0-catalogs-and-science.md` | 176 | `c971357f3bcc7f1d23fcfac0a95de8fde264bbc38b6b6c90a296440eb2d092ca` |

## Errata e limiti

Il precedente handoff M0-environment dichiarava undici blocchi shell. Il conteggio non è riproducibile: la preregistrazione pubblicabile contiene **due** fence bash, entrambe verificate ora senza eseguire i comandi interni. Il dato undici non sostiene alcun PASS. Le modifiche privacy dei due documenti nuovi spiegano i loro hash differenti dagli handoff originali; gli otto documenti storici non sono stati alterati.

Nessuna misura di velocità, correttezza numerica di un'implementazione, GUI, GPU, recovery del prodotto o conformità delle librerie al divieto di fallback è stata eseguita da questa verifica. CodeGraph e Graphify non hanno indici: non sono stati creati e non bloccano M1. M0 prepara esperimenti, non dimostra la qualità del prodotto.
