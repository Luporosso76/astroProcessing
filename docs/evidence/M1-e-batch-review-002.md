# M1.e: review indipendente del controller batch 002

Data: 25 ottobre 2026. Record del coordinatore della review security, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`; modello autodichiarato `openai/gpt-6-astra`, costi/token non esposti.

## Esito

Non READY per la suite completa con dummy propri. Nessun dato batch ammesso. La somma statica della riserva è corretta, ma non è ancora dimostrata la conservatività del bound di successo.

Il revisore ha letto integralmente sette moduli, 1549 righe; report 181, piano 147, PREP002 77 e manifest snapshot 31. Analizzato integralmente il manifest delle tuple. Verificati nove hash dei deliverable, sei file snapshot001, venti mock originali byte-identici, baseline e protocolli. Le 66 tuple restano 42 forward e 24 inverse, batch1 f32. Individuati 37 metodi, nessuno eseguito dalla review.

## Rilievi da correggere prima dei mock

| Priorità | Posizione | Difetto e requisito |
|---|---|---|
| Alta | `supervisor.py:95-113` | Un errore nel censimento può interrompere l'escalation anche con identità del leader trattenuto verificabile. Separare osservazione, verifica e tentativi di cleanup; segnalare soltanto il gruppo proprio ancora identificato e conservare tutti gli errori. |
| Media | `supervisor.py:110-122,163-167` | Reap del leader con discendenti residui non registra sempre la sessione irrisolta. Bloccare lanci successivi finché la scomparsa non è provata; non segnalare un PGID dopo aver perso l'identità trattenuta. |
| Media | `budget.py:179-204`, `supervisor.py:227-235`, `controller.py:437-440,517-591` | I sidecar gestiti dal wrapper possono superare il bound successo restando sotto il maggiore limite per-file. Verificarne schema e consumo prima dell'adozione oppure riservare i massimi consentiti; superamento significa STOP con riserva FAIL, non troncamento con PASS. |

Namespace futuro, barriera pre-exec, sessione privata, `WNOWAIT`, ricevuta dopo i close, RLIMIT_FSIZE prelaunch, gate delle fasi e nuovo envelope separato dai vecchi lock sono corretti staticamente. Queste chiusure non dimostrano cleanup runtime o quote universali del driver.

L'aritmetica verificata è `862466048 + 286240768 - 1073741824 = 74964992 B`: gap all'osservazione della preparazione, non una prenotazione o un nuovo preflight. Nessun output `batch-001` esistente alla review.

L'autore è incaricato delle correzioni e dei relativi mock aggiuntivi in sola preparazione, con snapshot002 e tuple invariati. Seguiranno review del delta e un turno distinto per i mock. Nessun import, processo, test, compute, scrittura, rete o Git eseguito dal revisore.
