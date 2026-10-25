# M1.e: review indipendente dei nove fault tiny004

25 ottobre 2026. Verbale del coordinatore sul risultato dell'agente `tests`, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`. Modello autodichiarato: `openai/gpt-6.1-sol`; costi e token non esposti.

## Esito

PASS nel perimetro dei nove fault reali di `M1-e-faults.md`. READY soltanto per l'eventuale CONFIRM affine f32 17x19, stride 19, batch 1, stessa ricetta e lock CPU, dopo un nuovo turno esclusivo. Nessun CONFIRM avviato da questa review.

Ricalcolati hash di report, ricevuta fault, execution e nove trace; verificati otto sorgenti, PREP004, engine e shader, protocolli, indice, descriptor e input/GT. Binding identici alla VALIDATE, SHA-256 `66dce9d16e844ae8d900e77bd626f041a6ef96c4a9812775afdffdff35d542e1`.

- Sette rifiuti prima del dispatch: zero dispatch, submit accettate e fence.
- `output-mutation`: un dispatch, una submit e una fence; mismatch dopo download, prima della risposta numerica.
- `device-lost`: un dispatch registrato, nessuna submit accettata e nessuna fence. Iniezione prima di `vkQueueSubmit`, non perdita fisica del device.
- Nove worker con exit 2, raccolti dal runner, cleanup senza errori, diagnosi pertinente, nessun output numerico pubblicato e nessun kernel CPU. Driver exit 0 e stderr vuoto.

## Copertura e limiti

Letti 43 righe del record primario, report raw 322, ricevuta 81, execution 66 e nove trace per 76 righe. Esaminati il ramo fault del runner e i punti di iniezione/STOP dell'engine, riusando la precedente review dei sorgenti dopo verifica dei relativi hash.

Review in sola lettura, senza replicare fault, test o compute. Raccolta worker attestata dalle ricevute, non da un nuovo audit OS dei PID. L'identità device completa deriva dalla VALIDATE compatibile; i ready riusciti sono confrontati dal runner, non conservati separatamente nei raw fault. Nessun ready nuovo attribuito ai rifiuti anticipati.

Nessuna prova degli interni opachi delle librerie, di blackout, perdita fisica del device, blocco del driver o terminabilità universale. Tempi esterni diagnostici, non campioni prestazionali. Nessuna chiusura complessiva M1 o scelta di stack.
