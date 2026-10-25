# Politica d'ingresso M0

Data: 25 ottobre 2026. Ambito: autorizzare le prove isolate M1 senza anticipare una licenza del prodotto o l'adozione delle dipendenze.

## Licenza e componenti

- La licenza del codice resta riservata al proprietario. Non si emette un `LICENSE` e non si attribuisce al progetto una licenza presunta.
- Le opzioni open source e proprietaria restano distinte dalla vendita: un prodotto open source può essere commerciale.
- Ogni componente effettivamente usato negli spike entra nel manifest con versione, origine, termini rilevanti e ruolo. Il record di disponibilità è [M0-environment](../evidence/M0-environment.md); non costituisce un SBOM della futura applicazione.
- Gli eseguibili degli spike restano in `tmp/`, esclusi da Git e non distribuiti. Una prova locale non autorizza a integrare il componente nel prodotto. Prima dell'adozione si verificano licenza, opzioni di build e dipendenze transitive effettive.
- L'inventario e il SBOM di release descriveranno gli artefatti realmente costruiti, non una lista di librerie ipotizzate. I gate corrispondenti restano aperti fino alla loro fase.

## Dataset e cataloghi

- M1 usa dati sintetici deterministici con formule e ground truth registrati prima delle misure. Non richiede immagini private né un catalogo acquisito.
- Prima di usare dati pubblici si registrano fonte, release, licenza, condizioni di trasformazione e distribuzione, hash e riferimento attendibile. Accessibilità pubblica e checksum non dimostrano rispettivamente autorizzazione e autenticità.
- Cataloghi, indici, curve di risposta, generatori e dataset hanno diritti distinti. D-21 è una preferenza per pack inclusi, non un'autorizzazione a distribuire materiale incompatibile.
- Il [dossier cataloghi e scienza](../evidence/M0-catalogs-and-science.md) lascia i cataloghi candidati non approvati. L'eventuale blocco riguarda acquisizione, adozione e distribuzione del pack interessato; non impedisce gli spike indipendenti.

## Criterio di uscita per M0.4

La politica è registrata, le dipendenze disponibili sono inventariate, il metodo di registrazione delle dipendenze effettive è definito e nessun pack è adottato. Questo soddisfa la preparazione M0 per M1, dopo review indipendente. Non equivale a chiudere licenze, bibliografia o validazione empirica degli algoritmi M3 e della release M5.

## Linux

La matrice in [test-matrix](../test-matrix.md) è un protocollo, non una prova di supporto. Le configurazioni sono candidate; versioni e combinazioni effettive saranno congelate prima dei test pertinenti. La verifica M0.5 riguarda completezza e tracciabilità del protocollo. Le misure M1 e la verifica distributiva M5 non sono prerequisiti retroattivi per iniziare M1.
