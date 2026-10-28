# M1.b: metadati della sessione dopo il fallimento xcb

Data: 26 ottobre 2026. Controllo del coordinatore, in sola lettura; nessuna GUI, connessione, lettura di cookie o modifica di autorizzazioni.

Il turno005 aveva inoltrato soltanto `DISPLAY` e `XDG_RUNTIME_DIR` al figlio xcb con HOME privata, come documentato in `M1-b-preview-005.md:48-58`. Il controllo corrente ha interrogato esclusivamente la presenza di quattro variabili e metadati tecnici, senza registrare i valori ambientali.

| Controllo | Risultato osservato |
| --- | --- |
| `DISPLAY` | Presente |
| `WAYLAND_DISPLAY` | Presente |
| `XDG_RUNTIME_DIR` | Presente; directory dell'utente corrente, mode0700 |
| `XAUTHORITY` | Presente nell'ambiente del coordinatore, non inoltrata dal turno005 |
| Socket Wayland nominato dall'ambiente | Socket dell'utente corrente, mode0775 |
| `/usr/lib/x86_64-linux-gnu/libxcb-cursor.so.0` | Percorso runtime presente |

L'omissione di `XAUTHORITY` è una differenza concreta fra sessione disponibile e ambiente del figlio. È compatibile con la diagnosi di autorizzazione xcb, ma non dimostra che un nuovo lancio riuscirà. La presenza del percorso libxcb-cursor non ne prova il caricamento; smentisce soltanto l'inferenza automatica che il warning Qt dimostri l'assenza del runtime.

Non sono stati aperti file Xauthority, letti cookie o contattati server display. Nessuna variabile è stata modificata o inoltrata da questo controllo; nessun cambio a Wayland/offscreen, nessun `xauth`, `xhost` o tentativo di aggirare la sicurezza. Un eventuale fix dell'ambiente richiede nuova ricetta/versione, pin e ammissione prima del test. Il fallimento005 resta conservato e non diventa un PASS retroattivo.
