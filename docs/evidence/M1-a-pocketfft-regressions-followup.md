# M1.a pocketfft: correzione PREP della fixture preregressioni

Data: 2026-10-25. Modello dichiarato: `openai/gpt-6.1-sol`. Nessuna delega.
Costi/token non esposti, non stimati.

## Stato e perimetro

La review indipendente riportata dal coordinatore ha chiuso staticamente i
finding WNOWAIT/OWN-group e directory fsync. Questa consegna non li riapre:
quei blocchi di codice hanno AST identico alla versione precedente.

Corretto in PREP il nuovo finding MEDIO: la fixture FakePopen forniva Mock come
fileno e chiamava os.set_blocking reale. La diagnosi di errore poteva inoltre
finire in HERE/child-stop.json invece che nel namespace del caso.

READY per nuova review dei soli 20 metodi preregressioni. Nessun PASS runtime,
nessuna ammissione automatica alla build. Viewer BUILD005 occupa il turno:
nessuna compilazione, import di codice proprio, esecuzione di test/mock/dummy,
worker, FFT, producer o benchmark avviata. Nessuna rete, installazione, Git,
indice, dipendenza nuova o modifica di altri rami.

## Snapshot esclusivo prima delle modifiche

`tmp/m1/fft-pocket/snapshots/pre-regressions-followup-001/` contiene 22 file:
intero namespace precedente, compreso pre-followup-001 e la sua history,
preregistrazione, entrambi i report originali e piano letto in sola lettura.
Copie create con mkdir/open esclusivi e fsync; manifest registra byte, righe,
SHA-256, mode originale e mode della copia. Directory copie 0500; file privati
write-protected con mode originale privato dei bit di scrittura. Non è una
garanzia di immutabilità contro owner/root.

Manifest dello snapshot, SHA-256:
`1323ff108548f52b0fadc6acaf97dcd0080e51e5dab868270baf85f2aa9ac27c`.

Il report originale di 148 righe conserva SHA-256
`4b0eaf97c242fb1420462c81fbb8f1841b518bd945a836e983df76df00d92bcc`;
il followup di 153 righe conserva SHA-256
`b32d966a3252f866249799f39a3ac91748527e5590f050cf617b78f9e1f7317a`.
Snapshot pre-followup-001, manifest/record/diff precedenti e piano invariati.
Nessuna pulizia o sovrascrittura della history.

## Modifica della sola fixture

`regressions.py` assegna FD sintetici interi distinti, stdout 40001 e stderr
40002. La spia context-managed su S.os.set_blocking non delega al syscall reale;
il metodo positivo verifica la lista esatta delle due chiamate, entrambe False.
Nessun FD reale viene reso bloccante/non bloccante dalla fixture.

La fixture del loop usa esclusivamente read mock da 8192 byte e status mock
da 16384 byte, con verifica degli argomenti esatti. I due canali ricevono bytes
distinti e poi EOF; selector, buffer esauriti e risultato completo sono verificati.
L'attesa resta exit 0 con stdout/stderr esatti e RSS campionata attesa, non un
generico nonzero. L'open dello status è sostituito nel solo namespace Python
del supervisore, non in builtins; nessuna lettura reale di processi è necessaria.
Sono patch di API nominate e limitate al contesto, non sostituzioni dell'intero os.

Tutti i 20 metodi ereditano una fixture che crea un caso esclusivo sotto il
namespace fresh della futura esecuzione. S.HERE e S.run sono context-patched;
il wrapper ammette solo quella directory per run e relative diagnosi. Anche gli
STOP attesi producono ricevute per-caso, senza scrivere nella radice HERE.
HERE originale serve solo a leggere sorgenti pinned e inventario della radice.

Una verifica finale per caso controlla nomi/mode/hash dei file della radice e
ripristino delle API/globali toccate. LAUNCH_BLOCKED e UNRESOLVED sono isolati
con patch contestuali, non cancellati nello stato globale originale. Le pipe
mock restano soggette alle attese di cleanup originali. Nessuna directory caso,
diagnosi o ricevuta viene eliminata dopo successo o fallimento: gli errori del
supervisore restano nel caso, i failure dei test nel log aggregato del runner.

`supervisor.py` cambia una sola stringa di configurazione: il manifest corrente
è `followup-manifest-002.json`. Nessuna modifica a cleanup, fsync o algoritmi.
Il piano non è stato modificato; i suoi riferimenti al manifest 001 appartengono
alla precedente consegna. Il nuovo manifest 002 pinna i sorgenti corretti e la
stessa preregistrazione scientifica, mantenendo 001 e i risultati storici immutati.

## Verifica statica realmente eseguita

Letture semantiche di questo turno: regressions.py 1-291, supervisor.py 205-319,
manifest 001 1-155, report precedente 1-153: 714 righe. Snapshot/hash hanno letto
gli altri byte senza rivendicare una nuova review semantica integrale.

Eseguiti `du -s -B1 tmp` e snippet `python3 -I -B -S - <<'PY'` per snapshot,
hash/mode, diff difflib e ast.parse. Preflight tmp osservato: 942096384 byte;
non è una prenotazione runtime né una misura attribuita a questa unità.
Versione Python completa nel record statico. Nessun modulo proprio importato.

AST valido per i due sorgenti Python; stessi 20 nomi di test. Diciassette body
AST identici; tre adattamenti solo di fixture: due directory dei run attesi in
errore e il metodo positivo di pump/cleanup. Le sette funzioni scientifiche
header/spatial/spectrum/rational/compare/arguments/cases sono AST identiche.
Anche OwnedGroup, census, cleanup, run e funzioni fsync/retention sono identici.
Import dichiarati solo stdlib. Queste verifiche NON eseguono i 20 metodi.

| Artefatto attuale | Righe | Byte | SHA-256 |
|---|---:|---:|---|
| `tmp/m1/fft-pocket/regressions.py` | 378 | 18844 | `05fab44e4fdf34efc449c435d40e0f514367667f45ee83186b7ce34ea4d11ed4` |
| `tmp/m1/fft-pocket/supervisor.py` | 554 | 24705 | `977f3704e9a168f8246495f8c12dbff273c665ab562dc2a70b2b62e18ce8f366` |
| `tmp/m1/fft-pocket/followup-manifest-002.json` | 180 | 5249 | `76784fa8e727d254411295f534150ee93f54521776cadb050fd0cad75642494a` |
| `tmp/m1/fft-pocket/regressions-source-diff-001.patch` | 200 | 9752 | `13a532907f62a078a5797317a7e2ebd683d59d20b4a3aa49abdf46e9e37ede6e` |
| `tmp/m1/fft-pocket/static-regressions-followup-001.json` | 117 | 3399 | `c2c631ca789e2286173f12abe941c2d73bf072bb85d6a9c8929a9d006e4e97b5` |

Nuovo manifest: cinque source pin, precedente manifest, nuovo snapshot e diff
effettivo dei soli due file modificati. Recipe e oracle identici al manifest 001.
Restano 192 finiti + 14 base + 24 non finiti + 2 collisioni = 232 PLANNED, zero
eseguiti; tol ZERO, direzioni, GT indipendente, dtype/ABI e pubblicazione invariati.
Nessun cambio backend, SIMD, precisione o dipendenza.

Header e LICENSE upstream ricontrollati ai pin originari e immutati. Notices
conservate. Il limite dell'aliasing interno cmplx con -fno-strict-aliasing resta
esplicito: nessuna certificazione generale di assenza di UB ISO C++.

## Handoff

Serve review del diff e dei 20 metodi corretti. Solo dopo review e assegnazione
del turno, primo comando proposto, NON ESEGUITO:

```sh
python3 -I -B -S tmp/m1/fft-pocket/regressions.py preregressions-002
```

Nessuna build auto-ammessa. Qualifica numerica, runtime/licenze, prestazioni e
adottabilità rimangono non verificate da questa preparazione.
