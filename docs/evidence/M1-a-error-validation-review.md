# M1.a: review indipendente delle regressioni d'errore

Data: 25 ottobre 2026. Unità: M1.a, contratti d'errore della versione separata v3.

## Esito e provenienza

PASS limitato ai tre rilievi iniziali e alle regressioni eseguite. M1.a complessiva resta aperta. Verbale persistito dal coordinatore dalla review `tests`, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello autodichiarato OpenAI GPT-6.1 Sol, senza attestazione indipendente.

Input principale: `M1-a-error-validation.md`, SHA-256 `45936368c1e8d2634680930e255210e9d9f6709db8b9e71c28685720c6735203`; ricevute sotto `tmp/m1/a-fault-contract-v3/runtime-attempt-001/`. Versioni e ambiente sono nei manifest della build richiamati dal record di esecuzione. La review non ha eseguito né importato i candidati: ha letto fonti, diff, manifest e raw, ricalcolando hash e conteggi con strumenti in sola lettura. Non esiste un nuovo comando di benchmark associato a questo verbale.

## Rilievi chiusi e riscontri

- Inverse non finite: 36 casi real/imag, f32/f64, NaN/±Inf sui tre backend; exit 2, diagnosi specifica, nessuna risposta o output. I controlli precedono il kernel.
- Parser C++: 14 casi; undici richieste invalide rifiutate, due EOF puliti e una richiesta finale completa senza newline accettati. Otto token esatti e consumo numerico completo.
- Dispatch: eventi effettivi dei propri `Popen`, non contatori costanti. Due mutanti registrano tentativo e rifiuto, zero avvii alternativi e nessun summary accettato. L'exit 2 dei probe è imposto dal validatore hashato; non esiste una ricevuta exit separata per probe.
- Confermati 360 casi finiti, 36 vecchi errori, 36 inverse non finite, sei copie bitwise e dieci regressioni IPC.
- 107 journal e 105 PID distinti; ogni avvio è associato alla propria uscita: 14 exit 0, 83 exit 2, otto exit −9. Guardie osservate: tre deadline, due dimensioni di linea, una RSS. Nessun watchdog o cleanup di soccorso nei risultati IPC.
- Reader non bloccante con `os.read` e deadline monotona; partial senza newline non neutralizza la scadenza. Wrapper e auditor archiviati: exit 0, stderr vuoti.

## Integrità e limiti

Ricalcolati gli SHA del report, undici fonti attive, 353 artefatti runtime, 127 file protetti non-fixture, dodici file di snapshot, manifest e binari. Per le 168 fixture sono state confrontate soltanto le mappe hash congelate: gli 847 MB non sono stati riletti.

Oracle e comparatori verificati mediante confronto AST; dodici bound puntuali invariati, senza nuova epsilon. Il controllo aggiunto di dimensione e finitudine rafforza il rifiuto. Nessun tempo della serie 002 viene attribuito alla v3.

La raccolta dei PID è riscontrata negli artefatti, non ripetuta ora. Il PASS riguarda dispatcher e figli controllati, non interni FFTW/NumPy, terminabilità universale, ownership, maschere o NC-21 dell'applicazione futura. Nessuna validazione scientifica M3, misura prestazionale nuova o scelta di stack. Costi e token non disponibili.
