# M1.b: GTK 4.22.4, selezione Cairo e barriera contro retry

Data: 2026-10-25. **Gate del candidato ancora UNQUALIFIED/INCOMPLETE; nessun lancio autorizzato da questo record.** La factory Cairo pubblica esiste. Non è però un setter del renderer di GtkWindow. `GSK_RENDERER=cairo` da solo è una preferenza con prosecuzione su errore. La combinazione con warning fatali merita una conclusione distinta: nel sorgente della versione esaminata può interrompere il ramo di errore prima del candidato successivo, sotto condizioni esplicite. Non dichiarare impossibile universalmente GTK, né attestare quelle condizioni sul runtime GI senza prova.

Modello dichiarato dall'ambiente: OpenAI `openai/gpt-6-astra`, identità non verificata indipendentemente. Costi/token non esposti. Nessuna delega, import GI/GTK, GUI, rendering, GPU, benchmark, compilazione, installazione, clone, Git o accesso a configurazioni personali. Nessuna esecuzione dei file scaricati. Scritti soltanto questo nuovo report e testi pubblici nel nuovo `tmp/m1-gtk-renderer/`.

## Perimetro/versioni realmente osservati

Letti con `read` solo `plans/M1-b-viewer.md` **17–38 e 133–138**. Non letti launcher, viewer o namespace degli altri autori. Il prereg richiede Cairo, GPU disabilitate via GDK, warning/critical fatali e verifica della classe; riconosce già che ambiente e classe finale non provano assenza di retry.

Metadati locali strettamente tecnici, `dpkg-query -W`:

- `libgtk-4-1 4.22.4+ds-0ubuntu0.1`;
- `python3-gi 3.56.2-1`;
- `libglib2.0-0t64 2.88.0-1ubuntu0.1`.

`dpkg-query -L libgtk-4-1` ha localizzato `/usr/lib/x86_64-linux-gnu/libgtk-4.so.1.2200.4`. SHA-256 letto: **`bd4435a1cbaff65249bed87b9c6305692c065e3747959f4aef056487f63a9483`**. `readelf --dyn-syms --wide` sul solo file ha confermato simboli esportati `gsk_cairo_renderer_new`, `gsk_renderer_realize`, `gsk_renderer_realize_for_display`, `gsk_renderer_new_for_surface`, `gtk_native_get_renderer`. Nessun caricamento dinamico o chiamata API.

Ciò prova presenza di simboli C, non esposizione/utilizzabilità GI, correttezza runtime o prestazioni. Assenza `gtk4.pc` dal contesto M0 non riverificata e non usata come prova di API mancanti. Non letto GIR/typelib. Non verificati patch Ubuntu, flag di build, corrispondenza binario-sorgente o handler di log PyGObject. L'hash del runtime è un identificatore, **non una prova positiva di admission**.

Fonti GTK fissate al tag upstream **4.22.4**, non main: base URL `https://raw.githubusercontent.com/GNOME/gtk/4.22.4/`. Fonte GLib fissata a **2.88.0**. Trasporto HTTPS verificato da curl, nessuna firma release/tag verificata, nessun commit immutabile risolto: byte conservati e hash sotto rendono identificabili le fonti lette, senza fingere identità con la build distro.

## Risposte API con corpi effettivi

### Factory tipizzata: sì, senza selettore di altri renderer nel corpo letto

[gsk/gskcairorenderer.c](https://raw.githubusercontent.com/GNOME/gtk/4.22.4/gsk/gskcairorenderer.c), **210–228**, documenta “Creates a new Cairo renderer.” e implementa:

```c
gsk_cairo_renderer_new (void)
{
  return g_object_new (GSK_TYPE_CAIRO_RENDERER, NULL);
}
```

La dichiarazione è pubblica in `gskcairorenderer.h`, `GDK_AVAILABLE_IN_ALL`. [gsk/gskrenderer.c](https://raw.githubusercontent.com/GNOME/gtk/4.22.4/gsk/gskrenderer.c), **242–315**, contiene integralmente `gsk_renderer_do_realize` e `gsk_renderer_realize`: dispatch sulla vfunc dell'oggetto scelto; su FALSE pulisce la surface e restituisce FALSE, **senza creare un altro tipo**. Il pubblico `realize` passa `attach=FALSE`; `realize_for_display`, **318–343**, è pubblico da 4.14 e realizza senza surface. È obbligatorio unrealize prima della distruzione.

La vfunc Cairo, **53–73**, crea il contesto Cairo quando c'è surface e, se richiesto `attach`, restituisce FALSE quando l'attach fallisce. Nessuna factory GL/Vulkan nel corpo. Questo non è un audit transitivo di tutti i sottosistemi GDK/Cairo. Il testo della factory avverte che Cairo è incompleto, non rende trasformazioni 3D e può mostrare un error marker. La parola upstream “fallback renderer” descrive il ruolo nella catena automatica: una sua costruzione diretta può essere una **scelta primaria**, senza autorizzare fallback nel prodotto.

### GtkWindow: getter non equivale a injection

[gtk/gtknative.c](https://raw.githubusercontent.com/GNOME/gtk/4.22.4/gtk/gtknative.c), **251–265**, `gtk_native_get_renderer` restituisce il renderer tramite interfaccia, `(transfer none) (nullable)`. L'header pubblico `gtknative.h` non espone un setter né una factory callback del renderer. “Readonly” qui significa accesso tramite getter, non immutabilità dell'oggetto restituito.

[gtk/gtkwindow.c](https://raw.githubusercontent.com/GNOME/gtk/4.22.4/gtk/gtkwindow.c), **2188–2194**, restituisce `priv->renderer`. Il corpo completo `gtk_window_realize`, **4483–4589**, contiene:

```c
surface = gdk_surface_new_toplevel (gtk_widget_get_display (widget));
priv->surface = surface;
gdk_surface_set_widget (surface, widget);

if (priv->renderer == NULL)
  priv->renderer = gsk_renderer_new_for_surface_full (surface, TRUE);
```

`gsk_renderer_new_for_surface_full(..., attach)` è dichiarata in **header privato** `gskrendererprivate.h`, non nel pubblico `gskrenderer.h`. Il corpo mostra che GtkWindow costruisce la propria surface e sceglie il renderer internamente. Creare separatamente un GskCairoRenderer e realizzarlo sulla stessa surface **non assegna `priv->renderer`**, né impedisce la precedente selezione interna. `get_for_surface` è lookup, non factory d'iniezione. Nessun accesso a dati privati o aggiramento del lifecycle è proposto.

**Metodo “factory tipizzata più setter/iniezione pubblica nel normale GtkWindow”: non adottabile con le API esaminate**, perché quel setter non è esposto. Non è un teorema su custom native, altre architetture o futuri GTK. Nessuna nuova architettura viene esplorata qui.

### Factory per surface e variabile: preferenza, non strict

`gskrenderer.c` **777–797**, documentazione e corpo pubblici:

> “If the `GSK_RENDERER` environment variable is set, GSK will try that renderer first, before trying the backend-specific default. The ultimate fallback is the cairo renderer.”

`new_for_surface` chiama il selettore privato con `attach=FALSE`, non accetta un tipo da imporre. `get_renderer_for_name`, **487–537**, interpreta un **singolo nome**, non una lista utente di renderer. È la tabella interna **709–725** a definire la lista: override sul display, ambiente, backend-specifico, Vulkan/GL e relativi fallback, infine Cairo. L'override sul display precede l'ambiente; è letto da `g_object_get_data(..., "gsk-renderer")`, non documentato come API pubblica strict.

Il ciclo **728–775** crea il tipo, prova realize, ritorna su successo; su fallimento libera e continua. Per display/ambiente `warn_if_fail=TRUE`; per gli altri FALSE. La sezione [running.md](https://raw.githubusercontent.com/GNOME/gtk/4.22.4/docs/reference/gtk/running.md) **477–499** dice “selects” ma non promette stop: la documentazione API e il corpo chiariscono la prosecuzione. Un controllo `getclass` dopo la factory non può annullare tentativi precedenti.

## Warning fatali: barriera condizionale reale, non ignorarla

Nel ramo fallito selezionato dall'ambiente, il ciclo esegue **prima della prossima iterazione** (`gskrenderer.c` 753–770):

```c
if (renderer_possibilities[i].warn_if_fail)
  {
    g_warning ("Failed to realize renderer '%s' for surface '%s': %s",
               G_OBJECT_TYPE_NAME (renderer),
               G_OBJECT_TYPE_NAME (surface),
               error->message);
  }
```

La documentazione primaria [GLib 2.88.0 running.md](https://raw.githubusercontent.com/GNOME/glib/2.88.0/docs/reference/glib/running.md), **45–54**, dice per `G_DEBUG=fatal-warnings`:

> “Causes GLib to abort the program at the first call to `g_warning()` or `g_critical()`.”

Quindi **inferenza dal sorgente, non prova runtime**: con `GSK_RENDERER=cairo` e `G_DEBUG=fatal-warnings` effettivi prima dell'inizializzazione, nessun override display precedente e fatalità non alterata, il fallimento di realize Cairo termina il processo prima che il ciclo tenti il backend successivo. Non è corretto affermare che tale combinazione sicuramente ritenti comunque. `fatal-criticals` da solo non basta per quel `g_warning`.

Ma non è una API GSK “strict Cairo only”. Per usarla come barriera preventiva occorre dimostrare sul percorso reale: ambiente installato prima della prima inizializzazione; Cairo primo candidato effettivo; assenza di override precedenti; warning realmente fatali anche con integrazione GI/logging; corrispondenza della build ai rami letti. L'abort è diverso da un errore recuperabile: supervisor/receipt devono trattarlo come STOP, mai rilanciare con altro renderer. Questa ricerca non avvia tale supervisor o prova. La documentazione GLib stessa sconsiglia il flag fuori dal debugging.

`GDK_DISABLE=gl,vulkan` è documentato in `running.md` **351–375** come disabilitazione delle funzionalità, **non come rimozione della lista GSK o garanzia “nessun tentativo”**. I helper della lista chiamano prepare GL/Vulkan; il loro dettaglio non è stato auditato. Non basta sostituire “non riesce a usare GPU” a “non prova un altro backend”. Nessuna variabile GSK strict/no-retry è esposta nelle sezioni di selezione lette. La factory tipizzata pubblica evita il selettore per un renderer indipendente, ma non risolve l'iniezione GtkWindow.

## Copertura e artefatti

Letti integralmente: `gskcairorenderer.c` 1–228, `gtknative.c` 1–326, header pubblici `gskcairorenderer.h` 1–58, `gskrenderer.h` 1–72, `gtknative.h` 1–57, header privato `gskrendererprivate.h` 1–65. `gskrenderer.c`: 230–344 e 487–818, tutti i corpi decisivi sopra. `gtkwindow.c`: 2188–2194 e 4483–4589 più risultati grep, **non** tutte le 7306 righe. `running.md` GTK: 351–387 e 477–521 più grep, non l'intero documento. GLib 2.88.0: estratto G_DEBUG 45–54 via grep con contesto, non intero documento. Ricevere un file non equivale a leggerlo tutto.

Consultata anche <https://docs.gtk.org/glib/running.html>, sezione Environment variables: pagina corrente dichiara **2.91.0**, quindi non usata come prova della versione installata; la clausola è stata verificata anche nel tag 2.88.0. Nessuna documentazione GTK main/futura sostituita alla 4.22.4. Non letti corpi GDK di creazione context/surface, logger GLib, patch distro o codice GI; nessuna garanzia transitiva su tutto GTK/GDK/compositore.

Comandi effettivi: `read` degli intervalli prereg; `du -s -B1 tmp`; guard `used+2097152+16777216 <=1073741824`; `mkdir -m 700 tmp/m1-gtk-renderer`; curl `-q --fail --silent --show-error --proto '=https' --max-filesize ... --max-time 60`, TMPDIR nel namespace; `wc -lc`, `grep`, `read`, `cat` dei quattro header, `sed` degli estratti Window; dpkg-query limitato ai tre pacchetti; readelf e sha256sum. Nessun accesso ai device/configurazioni o esecuzione dei payload.

Scaricati **305702 byte** su massimo 2097152. I nove file GTK provengono dalla base 4.22.4 nei percorsi sopra; `running.md` da `docs/reference/gtk/`, header/source renderer da `gsk/`, native/window da `gtk/`. Decimo file `glib-running-2.88.0.md` dall'URL GLib sopra. Hash SHA-256:

```text
f553c31e61e3d0d90137d84ecc4e0ca4553e0760f344590e2ef39422998c5ec2  gskcairorenderer.c
ed6a207a4073e4c1fd393a353a370d9673ff61603817961e93f2b14225267227  gskcairorenderer.h
d7976357a39dfc695f4419e77265be593f6cef11fabe85877d550b3efa978df4  gskrenderer.c
9e1e5053835d183ab04299f871fdbde815759536bdd04cbdf6412697519a0bed  gskrenderer.h
7efce7b1bf98351c442208dcf084908eb9c8f925b853b41769aa6f31fc9a01de  gskrendererprivate.h
5207bff86fcb72a523cb0cd9ed1c195fcc221a2278367d995a65d50e3f1f0e6b  gtknative.c
8c8c951b6a5c6533850e042179374ce50653d7cdf0baed24dd65b36cbc95d6ba  gtknative.h
59c0c837334e3e0039c2799fb5df04827b9f78bd4518c65d25ae66afd28d41be  gtkwindow.c
8180c610e722f258fde00266a51e196ef9dcd0fcce746f295526d62a25992d80  running.md
eab560912a64f91580643427af0f23e1536bca44dc25b7c3b4382d7baf7fdab2  glib-running-2.88.0.md
```

Scratch prima **868241408**, dopo download **868569088 byte**, sotto 1 GiB con oltre 16 MiB di margine. Quote campionate, non prenotazione atomica contro altri writer. Nessun nuovo file in altre cartelle; prereg/progress/viewer invariati.

## Verdetto operativo circoscritto

**Non adottabile con il solo metodo ambiente + controllo classe finale. Non adottabile il presunto metodo setter pubblico GtkWindow, che qui non esiste.** La barriera warning-fatale, già prevista nel prereg, ha invece un fondamento statico preciso: va distinta da questi due metodi, non cancellata con una conclusione universale d'impossibilità.

Per il candidato reale resta **UNQUALIFIED**, mancando la prova di admission legata alla build/integrazione GI e alle condizioni della barriera. Questo record non la sostituisce, non modifica il prereg e non autorizza tentativi. Cairo deve essere selezionato come percorso primario; ogni fallimento deve fermare quel percorso, senza fallback nemmeno esplicito/autorizzato. Non registrare `fallback_calls=0` o PASS inventati. Nessuna conclusione di velocità o inefficacia GTK; gli altri rami restano indipendenti. Ricerca bounded conclusa.
