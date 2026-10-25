# M1.c: prima serie temporale SQLite e JSON

## Esito effettivo

Esecuzione `measure` terminata con exit 0 e stderr vuoto. Due candidati primari dichiarati prima dell'esecuzione, senza sostituzioni: SQLite e JSON. La validazione precedente resta 44/44 e 46/46 casi; questa fase non la riesegue e non la rinomina benchmark.

Comando dalla radice:

```sh
/usr/bin/python3 -I -B tmp/m1-persistence/run.py --unit c --phase measure --backend both --series-id m1c-v2-01 --output tmp/m1-persistence/paired-series-v2-01
```

Ricevuta esterna: `tmp/m1-persistence/measure-execution-001/execution.json`. Stdout e stderr sono conservati nella stessa directory. Il tempo esterno è diagnostico, non un campione del confronto.

Protocollo: `plans/M1-spikes.md`, SHA-256 `1f04fa01a6042ce66c1c1e511de798a92bd6c0b8b7be17ece0ffda0abb20cbba`.

Sorgenti verificati prima dell'avvio e dal supervisore al termine:

- `persist.py`: `aa1be7debaf126680e1b9b34c986f61e8dfbf450a6746b7d3b3bcb0afe127e67`.
- `run.py`: `f54caff9c25685502ea035045a9cc91fe1735c32bc7a14bfe6c75f5a13f8fcb2`.

Ricetta: `bc0a4d775349c7ed616b981f01fb8b5c3a54462e1e7c2f8e53401d891e8d47c4`. Hash di `measure/samples.json`: `6e98c1e85a11d4f5f864c53e520841cbca8ee37a7a7e7b363d45978fa1650c7a`.

## Campioni e ricalcolo

Sei processi distinti, tre per candidato, tutti raccolti con exit 0. Ogni processo esegue due warm-up e sette round misurati: 6 warm-up e 21 campioni per candidato. Il supervisore alterna i candidati; lo schedule contiene 54 esecuzioni complessive. Startup e tempi esterni dei processi restano separati dai campioni warm.

| Metrica | Mediana SQLite, ms | Mediana JSON, ms | Rapporto appaiato SQLite/JSON | IC percentile 95% della serie |
|---|---:|---:|---:|---|
| Totale fino alla riapertura verificata | 160,438410 | 146,532013 | 1,090257 | [1,077537; 1,104816] |
| Commit, secondo le primitive del candidato | 7,191287 | 3,032009 | 2,341000 | [2,232585; 2,472713] |
| Riapertura verificata | 56,567843 | 53,582803 | 1,057388 | [1,033167; 1,071412] |

Il rapporto è `exp(median(log(T_sqlite[p,r]/T_json[p,r])))`, non il rapporto delle mediane. Bootstrap gerarchico appaiato: 2000 ricampionamenti, seed 4059, tre cluster di processo e sette coppie di round entro ciascun cluster; percentili con interpolazione lineare.

Il coordinatore ha ricalcolato dai JSON tutti e tre i rapporti, gli intervalli e le sei mediane: corrispondenza esatta con i risultati registrati. Ha verificato gli hash elencati in `measure/artifacts.json`, le identità delle 21 coppie per candidato, i sei PID e i relativi exit code. Questa verifica non è una nuova esecuzione né una review indipendente del coordinatore. La review indipendente dei risultati temporali resta da registrare prima della decisione ADR.

## Limiti e stato del gate

- Un solo schema sintetico, un host e un filesystem. Tre cluster non permettono generalizzazioni fra sistemi.
- Nessun altro benchmark comparativo era ammesso. Preparazione di sorgenti e review in sola lettura potevano coesistere; non si dichiara isolamento fisico dell'host.
- Cache del sistema non svuotate. Le fixture preesistenti sono state verificate e alcune rese sparse conservandone i byte; non si confrontano tempi I/O antecedenti come se lo storage fosse invariato.
- Il totale include preflight, scrittura, verifica e riapertura del prototipo. La riapertura SQLite include la copia dello snapshot e `integrity_check`; non misura soltanto una chiamata di apertura.
- Le primitive di commit dei due candidati non hanno identica struttura. I contatori applicativi non osservano tutte le scritture o gli `fsync` interni SQLite.
- SIGKILL e fault injection precedenti non provano il comportamento dopo blackout.
- I tempi inferiori di JSON in questa serie non decidono l'autorità del progetto, la semantica concorrente o lo stack complessivo.
- Sorgenti e campioni restano in `tmp/`, esclusa dalla pubblicazione Git. I soli report non rendono il clone integralmente riproducibile.

M1.c dispone ora di validazione e misure reali. Decisione di persistenza, review dei campioni e gate complessivo M1 restano aperti.
