# M0 — Review indipendente del piano 2.2.1

## Esito e handoff

**PASS documentale sul delta effettivo 2.2 → 2.2.1. Nessun blocker rilevato nel perimetro della review.**

La review richiesta per adottare la 2.2.1 come fonte è conclusa. Il coordinatore può registrare questo esito, completare gli altri deliverable M0 e avviare M1 secondo i prerequisiti reali, senza un nuovo voto del proprietario. Questo verbale non certifica da solo la chiusura di tutti i deliverable M0, che non sono stati esaminati.

I PENDING interni al piano identificano lo stato alla consegna dell'autore. Questo verbale registra l'esito successivo sullo specifico hash, senza modificare il piano. Stato operativo e archiviazione byte-preserving degli storici restano al coordinatore; questa review non esegue l'archiviazione.

## Identità, ambiente e oggetto

- Data di riferimento della sessione: 25 ottobre 2026.
- Revisore: agente architect, indipendente dall'autore indicato dal coordinatore come sessione `ses_e8c7a6c67ffeLwIFRSfzxJGMau`. Nessuna subdelega.
- Modello/provider dichiarato dal runtime: `openai/gpt-6-astra`, self-reported, senza attestazione esterna. Costi e token non attestati.
- Directory dei comandi: `/opt/workspace/astroProcessor`; ambiente rilevato: Linux x86_64, Python 3.14.4, GNU diffutils 3.12.
- Baseline: `ASTRO_POSTPROCESSING_PLAN_v2.2.md`, 4452 righe, SHA-256 `8fa0f881bd932c1e468449dea74037bad8cd88bdc8937c7077d1f233aa1f29c1`.
- File revisionato: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, 4482 righe, SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`.

## Copertura effettiva

Letti integralmente con il tool read:

- `RICHIESTA_REVISIONE_PLAN_2_2.md`, righe 1–122;
- `ISTRUZIONI_OPERATIVE.md`, righe 1–164;
- `AGENTS.md`, righe 1–52.

Letto tutto il delta testuale reale tra i due piani, non il solo riepilogo dell'autore. Il primo output combinato è stato troncato: la copertura è stata completata rigenerando il diff con Python `difflib.unified_diff(..., n=1)` e leggendo tutti i suoi 663 elementi in tre blocchi `[0:250]`, `[250:430]`, `[430:]`. GNU diff conta 165 righe rimosse e 195 aggiunte; difflib allinea diversamente i contesti e conta 179/209. Non sono due revisioni diverse.

Riletture mirate del piano corrente: 379–387, 1655–1675, 2004–2009, 2647–2655, 3572–3600, oltre alle corrispondenze della ricerca su approvazioni residue, P0 proposti, consenso dedicato, gate pre-implementazione e dipendenze M0/M1. L'Appendice D è stata analizzata programmaticamente per i controlli selettivi sotto; non riletta integralmente in questa review.

La baseline beneficia della precedente review con la sua copertura dichiarata. Non si dichiara una nuova lettura integrale delle 4482 righe né una nuova validazione di tutte le formule invariate.

## Riscontri sul piano corrente

- D-19 e D-20 sono approvate P0 nelle priorità, nei requisiti, nelle decisioni e nei nodi. I predicati di inclusione sono veri per mandato, non per presunti test superati: §§5, 23.1; righe 3473–3475 e 3597–3598. Il fallimento non declassa le funzioni e blocca M5.
- D-21 approva una preferenza, non i diritti dei singoli pack. Audit, integrità, copertura e primo avvio offline restano da provare; pack assente o invalido arresta il ramo, senza download o cambio implicito: §17.4, righe 2483–2498.
- Decisioni tecniche e gate ordinari sono autonomi dopo evidenze; questioni irreversibili, M10, costi, credenziali e permessi reali restano riservati. I blocchi isolano i rami coinvolti: §§0.8, 20, 21.13 e 23.1.
- M0 è documentale; M1 contiene spike isolati già autorizzati. I gate dei singoli operatori restano vincolanti prima della loro adozione, non richiedono una deconvoluzione applicativa già implementata per avviare gli spike generici: §§21.1–21.3, C-15 e controllo di raggiungibilità M1 sotto.
- NC-21 e NFR-FBK-01 mantengono il divieto assoluto, compreso il fallback autorizzato dall'utente. Bugfix e ripetizione dei test non autorizzano sostituzioni runtime, identità su errore o allentamento delle soglie: righe 384–386, 2009, 2025 e 2833.
- Il preflight C-14/F-IO-10 rifiuta il fit su import soltanto NL senza antenato lineare verificabile. Non inventa inversioni o operatori sostitutivi; il futuro operatore NL resta una proposta distinta: righe 1622, 1821, 2182 e 2696.
- Gli spike richiedono tracce del percorso effettivo e fault injection, non soltanto un messaggio d'errore. Dipendenze con fallback nascosti non disattivabili sono escluse: riga 2646.
- L'override Git è esplicito e coerente con AGENTS: solo origin ufficiale, credenziali esistenti, niente force-push o segreti. Nessuna operazione Git è stata eseguita da questo revisore.

## Verifiche documentali eseguite

Comandi di lettura e confronto: `wc -l`, `cat` sui tre documenti operativi, `sha256sum` sui due piani, `diff -u ASTRO_POSTPROCESSING_PLAN_v2.2.md ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, `diff --version`, Python inline e ricerca testuale con grep. Il codice di uscita 1 di diff indica differenze presenti, non un test applicativo fallito. Nessuno script è stato salvato.

Il controllo Python ha estratto le tabelle D.1–D.6 e confrontato i due registri. Risultati indipendentemente osservati:

- 395 nodi, 847 archi, 27 predicati;
- insiemi degli ID di nodi e predicati conservati; multinsieme degli archi identico alla baseline;
- zero duplicati di nodi, archi o predicati; zero estremi o predicati irrisolti;
- 133 foglie F/NFR P0 entro M5, tutte raggiungibili da M5 nell'unione `requires_all`, `requires_when`, `decided_by`;
- nessun C-15, C-16 o C-18 raggiungibile da M1 nella stessa unione;
- `.codegraph` e `graphify-out/graph.json` assenti alla radice. Nessun indice creato; assenza non bloccante per M1.

Riproduzione del confronto strutturale eseguito, senza file temporanei:

```sh
python3 - <<'PY'
from pathlib import Path
from collections import Counter, defaultdict
def parse(path):
    s = Path(path).read_text()
    s = s[s.index('### Appendice D.'):]
    nodes, edges, predicates, section = [], [], [], ''
    for line in s.splitlines():
        if line.startswith('#### D.'):
            section = line
        if not line.startswith('| '):
            continue
        c = [x.strip() for x in line.strip('|').split('|')]
        if 'NODI' in section and len(c) == 5 and c[0] != 'ID':
            nodes.append(c)
        if 'D.5' in section and c[0].startswith('P-'):
            predicates.append(c[0])
        if 'D.6' in section and len(c) == 4 and c[1] in {
            'contains', 'requires_all', 'requires_when', 'decided_by', 'verified_by'
        }:
            edges.append(tuple(c))
    return nodes, edges, predicates
n, e, p = parse('ASTRO_POSTPROCESSING_PLAN_v2.2.1.md')
on, oe, op = parse('ASTRO_POSTPROCESSING_PLAN_v2.2.md')
ids = {x[0] for x in n}
print('counts', len(n), len(e), len(p))
print('preserved', ids == {x[0] for x in on}, Counter(e) == Counter(oe), set(p) == set(op))
print('duplicates', len(n)-len(ids), len(e)-len(set(e)), len(p)-len(set(p)))
print('unresolved', [x for x in e if x[0] not in ids or x[2] not in ids or (x[3] != '-' and x[3] not in p)])
g = defaultdict(list)
for a, t, b, q in e:
    if t in {'requires_all', 'requires_when', 'decided_by'}:
        g[a].append(b)
def reach(start):
    seen, todo = set(), [start]
    while todo:
        x = todo.pop()
        if x not in seen:
            seen.add(x)
            todo.extend(g[x])
    return seen
parents = {a for a,t,b,q in e if t == 'contains'}
leaves = {x[0] for x in n if x[0].startswith(('F-', 'NFR-')) and x[2] == 'P0'
          and x[3].startswith(tuple('M'+str(i) for i in range(6))) and x[0] not in parents}
print('P0 leaves', len(leaves), 'unreachable', sorted(leaves-reach('M5')))
print('M1 scientific dependencies', sorted(reach('M1') & {'C-15','C-16','C-18'}))
PY
```

Non è stata rieseguita la ricerca completa dei cicli o degli stadi: gli archi identici conservano la topologia precedentemente verificata. I contatori di sotto-milestone e campi scientifici non sono stati ricalcolati. I risultati dichiarati dall'autore restano distinti dai controlli ripetuti qui.

## Riserve

Il PASS riguarda coerenza documentale, recepimento del mandato e conservazione strutturale del delta. Non dimostra correttezza empirica, prestazioni, compatibilità Linux, licenze, disponibilità degli strumenti, runtime dei predicati o completezza fisica degli scenari. Fonti primarie, dataset, soglie, audit e test restano dovuti ai rispettivi gate. Nessun test applicativo, spike, benchmark, installazione, rete o scrittura Git è stato eseguito. L'unico documento scritto dal revisore è questo verbale.
