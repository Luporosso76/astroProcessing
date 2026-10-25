# M1.e: review delle regressioni del supervisor

## Esito e copertura

PASS limitato alla suite del supervisor: 18/18 casi riusciti nell'esecuzione `002`, con exit 0 e nessun errore, skip o evento unraisable. Il test del binding ora distingue ricetta/GT incompatibili e binding CPU incompleto. Non è una prova del dispositivo o del calcolo Vulkan.

Review indipendente: agente `security`, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello `openai/gpt-6-astra` autodichiarato, senza attestazione indipendente. Il coordinatore persiste il verbale; il revisore non ha scritto questo file. Data: 25 ottobre 2026. Costi e token non esposti.

Materiali esaminati: report primario completo di 155 righe, launcher di 200 righe, diff di 40 righe, ricevute di 123 e 124 righe, qualificazione di 8 righe, log di 73 righe e 18 metodi nel sorgente test di 269 righe. Confrontati gli undici hash sorgente correnti e i dodici e diciassette file dei due snapshot.

## Evidenze verificate

- Report primario: `M1-e-supervisor-validation.md`. Ricevuta finale: `tmp/m1/e-vulkan/supervisor-validation-002/receipt.json`, identificata dal report primario, SHA-256 `36da6443f820bdca2fe4f10a88b9f225f33b8eb4af269f54fe1f9168589e146b`.
- La prima esecuzione resta conservata: il PASS di unittest non basta a qualificarla, perché conteneva un errore unraisable della fixture `FakePipe`. Nessun log fallito è stato cancellato o rietichettato.
- Nella seconda esecuzione i log confermano 18 PASS e zero failure/error/skip/unraisable. I figli reali del launcher risultano raccolti.
- Il launcher installa prima dell'import dei test un controllo che blocca avvii di sottoprocessi, fork/exec, segnali e caricamenti dinamici dalle fixture. Nessun tentativo registrato. I PID simulati non sono PID osservati del sistema.
- Gli hash degli undici sorgenti/piani coincidono fra prima e dopo la suite; le diverse strutture JSON delle due ricevute non rendono diversi quei contenuti.
- I gate e i controlli di errore non sono stati indeboliti per far passare la suite.

Il revisore ha letto artefatti e verificato hash e log. Non ha rieseguito i test, importato i candidati, avviato l'engine, consultato credenziali o svolto benchmark.

## Ammissione successiva e limiti

È ammesso il solo pilot CPU esplicito, dopo una nuova PREP che includa gli hash aggiornati dei test e del launcher, preflight della quota e ammissione esclusiva del coordinatore. Il pilot produce un nuovo lock della reference; non sceglie il migliore fra tutti i backend CPU.

Non sono ancora ammessi da questa review validate, fault o confirm GPU. Restano necessari i rispettivi gate, le ricevute compatibili e le verifiche hardware. La suite usa fixture del supervisor: non dimostra cleanup sul driver, assenza di fallback interni al driver, correttezza numerica GPU, prestazioni o idoneità del prodotto.
