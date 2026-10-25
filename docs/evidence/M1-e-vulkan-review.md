# M1.e: prima review statica Vulkan

Data: 25 ottobre 2026. Esito: due rilievi medi da correggere prima dell'ammissione; nessuna prova compute eseguita.

Review indipendente `security`, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato GPT-6 Astra. Verbale persistito dal coordinatore. Letti integralmente engine (523 righe), preparazione (276), runner (280), tre shader (38/46/12), README (94), preregistrazione (156), report (128), ricevuta (329), indice di 42 casi e 17 righe di log. Nessun import, avvio di candidati, benchmark, scrittura o accesso ai namespace altrui.

Riferimenti: `plans/M1-e-vulkan.md`, `M1-e-vulkan-preparation.md`, `tmp/m1/e-vulkan/`, ricevuta preparazione002 SHA-256 `223d160786ef4678f9e2d8ef00208eb7c6ee877e79c64ea3ae6c84a51689f32e`. Verificati sette hash delle fonti e pin di piano, binario, indice, tre shader e nove log. Non ripetuta l'autenticazione dei dodici artefatti tool. Ambiente e versioni nei manifest della preparazione; nessun costo/token disponibile.

## Rilievi

1. **Medio, cleanup:** eccezione dopo `Popen` nel costruttore può lasciare un figlio; errore nel primo `close` può impedire il secondo. Servono cleanup indipendenti di tutti e soli i propri figli, conservazione degli errori e regressioni dedicate.
2. **Medio, ammissione:** `confirm` può essere la prima esecuzione GPU positiva. Introdurre validazione numerica distinta e richiedere ricevute di validazione e fault compatibili con la ricetta congelata prima dei campioni confermativi. Assenza o mismatch deve fermare l'operazione.
3. **Basso, osservabilità:** il piano promette CPU user/system, ma l'engine raccoglie soltanto RSS. Implementare delte effettive oppure dichiarare la misura assente.

Il controllo delle copie end-to-end resta aperto finché non esiste il runner corrispondente. L'iniezione DEVICE_LOST predisposta prima della submission non prova perdita reale del device durante il lavoro.

## Riscontri e limiti

Pin hardware AMD vendor/device/UUID/type/queue e rifiuto del renderer software presenti; f64 non supportata comporta stop, non cast. Budget buffer e heap dichiarati senza equiparare memoria unificata a VRAM dedicata. Submission, barriere, fence e download rientrano nell'end-to-end; gli incrementi di dispatch sono vicini alle chiamate effettive, non costanti.

SPIR-V verificato staticamente: Float32 e NoContraction. Non eseguiti shader, pilot, fault o confronti GPU/CPU; nessuna garanzia su driver o librerie interne. Le correzioni sono assegnate al coder in una nuova revisione, senza modificare questa fotografia della review.
