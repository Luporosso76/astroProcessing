# M0: cataloghi offline e prerequisiti scientifici

Data della ricerca: 2026-10-25. Stato: **candidati NON APPROVATI all'adozione o alla distribuzione**. Analisi delle fonti, non consulenza legale certificata. Nessun catalogo, indice o pack acquisito; nessun test scientifico o benchmark eseguito.

## Ambito e copertura effettiva

Letti con `read`:

- `RICHIESTA_REVISIONE_PLAN_2_2.md`, righe 1-122, integralmente.
- `ASTRO_POSTPROCESSING_PLAN_v2.2.md`, righe 1650-1757: C-15, C-16, C-17, C-18.
- Stesso piano, righe 2023-2202: §13 fino alla prima parte di §13.14.
- Stesso piano, righe 2439-2538: §17 e contesto successivo.

Ricerca locale con `grep` sui contratti e sulle intestazioni. Non letti il piano integrale, il resto di §13, né la revisione 2.2.1 in lavorazione. Nessun codice o documento implementativo Siril consultato. Nessuna subdelega, credenziale, email esterna, installazione o build.

Modello dichiarato dall'ambiente: OpenAI GPT-6 Astra, `openai/gpt-6-astra`; identità runtime non verificata indipendentemente. Costi e token non esposti. Non sono stati calcolati hash dei cataloghi o delle pagine: non esistono artefatti acquisiti da attestare. L'eventuale storage automatico delle risposte appartiene al tool, non a un download manuale.

## Esito operativo e ordine dei gate

D-20 approva P0 colore relativo broadband e solver assistito offline. D-21 preferisce pack inclusi soltanto se leciti e sostenibili. Gaia resta un candidato, non una scelta irreversibile.

Questa ricerca non chiude M0 da sola. Il minimo M0 per progettazione e confronto dello stack non richiede che il gate empirico C-15 sia già superato. Le lacune bibliografiche di deconvoluzione non bloccano automaticamente spike generici di convoluzione/FFT o attività M1 indipendenti, quando il loro gate d'ingresso è soddisfatto. Occorre verificare ciascun contratto prima dell'implementazione del relativo algoritmo; validazione empirica prima di dichiararne disponibilità o chiusura. Il dossier legale del pack deve essere chiuso prima della sua adozione per M3 e della distribuzione.

## Registro delle fonti legali e dei cataloghi

Tutti gli accessi elencati sono del 2026-10-25. `webfetch` significa lettura della pagina, non acquisizione dei dati del catalogo. Le citazioni sono brevi estratti effettivamente restituiti.

| Fonte e comando | Evidenza | Stato e confidenza |
|---|---|---|
| `webfetch https://www.cosmos.esa.int/web/gaia-users/license` | "distributed under the CC BY-NC 3.0 IGO license" | Licenza Gaia verificata; alta |
| `webfetch https://www.cosmos.esa.int/web/esdc/terms-and-conditions` | §2: "Prior to any commercial use" richiede richiesta di autorizzazione/licenza; comprende guadagno diretto o indiretto | Commerciale non autorizzato; alta |
| `webfetch https://creativecommons.org/licenses/by-nc/3.0/igo/legalcode.en` | Testo legale letto: §3 permette riproduzione, distribuzione e adattamenti; §4 impone restrizioni e attribuzione | Uso/distribuzione non commerciale condizionati, non divieto assoluto di redistribuire; alta |
| `webfetch https://gaia.aip.de/metadata/gaiadr3/synthetic_photometry_gspc` | "218936915 rows, 54 columns"; licenza CC BY-NC 3.0 IGO; acknowledgement ESA/Gaia/DPAC | GSPC candidato, non adottato; alta |
| `webfetch https://archive.aavso.org/apass` | Circa 128 milioni di oggetti, circa 99% del cielo; "DR10 is still missing the northern polar cap" | APASS DR10 candidato; pagina contiene previsioni obsolete riferite al 2019; alta sui limiti documentati |
| `webfetch https://dc.g-vo.org/apass/q/cone/info` | Campo Rights richiede acknowledgement; non esplicita concessione commerciale/ridistributiva | Diritti APASS da verificare, accessibilità non equivale a licenza; alta |
| `webfetch https://aavso.org/blog/new-data-usage-guidelines` e `webfetch https://aavso.org/about/governing-documents` | Post 2025 annuncia linee guida 2025; pagina corrente collega PDF luglio 2026 | Non usare il post come testo vigente completo |
| `webfetch https://aavso.org/wp-content/uploads/2026/07/Data-Usage-Guidelines-2026-07.pdf` | Tool: "Unsupported fetched file content type: application/pdf" | PDF NON LETTO; nessuna conclusione sul suo contenuto |
| `webfetch https://www.cosmos.esa.int/web/hipparcos/tycho-2` | "positions, proper motions and two-colour photometry" per 2,5 milioni di stelle brillanti | Tycho-2 candidato limitato; diritti del prodotto e canale esatti da chiudere |

Il testo CC BY-NC 3.0 IGO richiede notices, licenza/URI, attribuzioni, identificazione degli adattamenti e assenza di restrizioni aggiuntive incompatibili. I termini ESA richiedono credit mission-specifici; per Gaia: ESA, Gaia DPAC. AIP fornisce inoltre l'acknowledgement esteso della missione, DPAC e finanziamento tramite Gaia Multilateral Agreement. Conservare entrambi i livelli pertinenti nel dossier del prodotto esatto.

Un rilascio gratuito non dimostra da solo uso non commerciale. Un'installazione separata non elimina i vincoli. L'ospitalità presso NASA/CDS/GAVO non trasforma automaticamente dati altrui in pubblico dominio. Le licenze delle immagini promozionali Gaia non sono quelle del catalogo.

Verificare separatamente: dati originali; selezione/adattamento; indice geometrico derivato o precostruito; codice del generatore; curve di risposta e calibrazione; codice dell'applicazione. Un indice ricostruito internamente non acquisisce automaticamente diritti commerciali. Nessuna licenza di software o curve è stata approvata qui.

## Trade-off e dimensionamento

| Candidato | Vantaggio | Limite e criterio di scelta |
|---|---|---|
| Subset astrometrico Gaia DR3 + GSPC selezionato | Seleziona colonne e qualità necessarie; separa solver e fotometria | Primo candidato da confrontare per bande supportate, senza imporlo; trasformazione RGB e copertura da validare |
| XP DR3 e fotometria per risposte specifiche | Riferimenti coerenti con risposte dichiarate | Più calibrazioni, covarianze e provenance; non requisito universale P0 |
| APASS DR10 | Fotometria BV e Sloan disponibile | Diritti incompleti, blending, saturazione, copertura e release da verificare |
| Tycho-2 | Astrometria e moto proprio di stelle brillanti | Densità/profondità limitate; due bande non garantiscono RGB arbitrario |

Non occorrono necessariamente tutti i miliardi di sorgenti Gaia, tutti gli XP o un pack da 1 TB. GSPC conta 218936915 righe e non è completo per ogni banda. Non è necessaria l'intersezione obbligatoria delle stelle del solver con quelle fotometriche.

Stima progettuale, non misura: `bytes_catalogo ≈ righe × bytes_record + indici_spaziali + metadati`. L'indice dei pattern aggiunge costi dipendenti da numero e scale. Misurare separatamente distribuzione compressa, installazione, scratch di generazione e RAM runtime.

Registrare area/celle/lacune, densità dopo filtri, magnitudini, bande e flag per sorgente, scale/FOV dei pattern, epoche e criteri di selezione. Un subset spaziale deve includere margini per hint, campo e movimento stellare. Nessuna dimensione o prestazione reale è stata misurata.

### Errata GSPC da conservare

La pagina AIP sopra segnala:

1. Unità originarie errate per flussi SDSS/PS1: `W m−2 Hz−1`, non `W m−2 nm−1`.
2. Fotometria PS1 `y` senza offset di flusso previsto; bug corretto in GaiaXPy 1.2.4, pubblicato il 2023-01-09.
3. "The table ... will not be fixed". Rigenerare è un nuovo prodotto con provenance, non una correzione silenziosa della tabella originale.

Nessuna rigenerazione eseguita. Congelare release, errata, trasformazioni e versione del prodotto derivato.

## Fonti scientifiche e limiti della lettura

### C-16: risposta e colore relativo

Fonte primaria: Montegriffo et al., 2023, `https://arxiv.org/html/2206.06215v2`. Accesso `webfetch`, seguito da `execute` con HTTP `fetch` e selezione in memoria dei paragrafi metodologici. Letti estratti di introduzione, metodi, standardizzazione, campionamento e cautele. Non letti integralmente tutti gli esperimenti e appendici. Il sito A&A `https://www.aanda.org/articles/aa/full_html/2023/06/aa43709-22/aa43709-22.html` ha restituito 403; arXiv è copia pubblica degli autori.

Citazioni: "The magnitude and colour differences ... define a relative photometric scale"; GSPC "is by no means a complete sample"; calibrazione "best suited to dealing only with point sources". Il paper documenta XP 330-1050 nm, circa 220 milioni di sorgenti, limiti di risoluzione spettrale, sistematici, sottostima delle incertezze nominali e problemi con crowding, variabili e sorgenti non stellari.

Derivazione indipendente: per conteggio di fotoni, `s_c ∝ ∫ f_lambda(lambda) T_c(lambda) lambda/(h c) d lambda`. Dichiarare convenzione della risposta, unità e normalizzazione; non contare due volte il fattore fotonico. La risposta comprende filtro, rivelatore, ottica e atmosfera quando pertinente. Tre fattori relativi non correggono ogni differenza di forma delle passband. Un buon fit stellare non certifica il colore della nebulosa.

Rilievo sul piano riga 1690: un metodo spettrofotometrico può usare risposte note; non implica che identifichi la risposta spettrale dal solo RGB.

### C-18: astrometria assistita

Fonte primaria: Lang et al., 2010, DOI `https://doi.org/10.1088/0004-6256/139/5/1782`, `https://arxiv.org/html/0910.2233v1`. Letti abstract e paragrafi metodologici/di validazione via `webfetch` e HTTP `fetch` in memoria; non tutto il paper.

Citazioni: "subsets of four stars, which we call 'quads'"; "a second stage does detailed verification". Separare generazione di ipotesi geometriche e validazione. Scale dei pattern e FOV devono essere compatibili; percentuali del paper non sono gate trasferibili al nostro solver. L'uso di cataloghi alternativi negli esperimenti del paper non autorizza fallback nel prodotto.

Il solver fornisce coordinate/WCS, non colore della nebulosa né prova della risposta del sensore.

`websearch` su `https://www.cosmos.esa.int/web/gaia/dr3` ha verificato epoca J2016.0, ICRS e TCB. Requisiti derivati: conservare frame/epoca/unità e convenzione `pmra = mu_alpha cos(delta)`; propagare posizioni e incertezze all'epoca osservativa. Precessione del riferimento e moto proprio non sono intercambiabili. Dichiarare trattamento di parallasse, prospettiva, aberrazione e rifrazione secondo precisione richiesta. Testare poli, RA a cavallo di zero, alto moto proprio ed epoca mancante.

### FITS, resampling, C-15 e denoise

| Fonte/comando | Lettura effettiva | Evidenza e lacuna |
|---|---|---|
| `websearch` pagina `https://fits.gsfc.nasa.gov/fits_standard.html`; `webfetch https://fits.gsfc.nasa.gov/fits_wcs.html` | Pagina indice WCS e risultato normativo FITS 4.0; NON specifica completa | FITS 4.0 del 2018-08-13; WCS Greisen/Calabretta 2002. Dichiarare separatamente convenzioni SIP/TPV. WCS non valida il resampling fotometrico |
| `webfetch https://arxiv.org/abs/astro-ph/9808087`, poi HTTP `fetch https://arxiv.org/html/astro-ph/9808087v2` | Abstract ed estratti sul rumore, NON paper completo | Fruchter & Hook 2002: "noise in adjacent pixels will be correlated" |
| `websearch` Hansen 1992; `webfetch https://epubs.siam.org/doi/abs/10.1137/1034115` | Riferimento identificato, accesso diretto 403; NON testo integrale | Bibliografia C-15 non chiusa da questa lettura |
| `webfetch https://www.cns.nyu.edu/~lcv/pubs/makeAbs.php?loc=Portilla03` | Abstract degli autori, NON paper integrale | Portilla et al. 2003, DOI 10.1109/TIP.2003.818640. Esperimenti dichiarati su rumore gaussiano bianco: non prova universale per rumore correlato/non lineare |

Derivazioni matematiche indipendenti, non risultati empirici:

- Resampling lineare fisso: `C_out = R C_in R^T`. Geometria stimata dai dati richiede anche trattamento della sua incertezza.
- C-15: `A = H_V^T W H_V + lambda L^T L`. Per `lambda > 0`, la condizione di intersezione dei kernel del piano garantisce unicità, non condizionamento accettabile.
- Parametri fissi e A invertibile: `K = A^-1 H_V^T W`, `x_hat = K(y_V-b_V)`, `C_x_hat = K C_(y_V-b_V) K^T`. La covarianza del fondo include termini incrociati se stimato dagli stessi dati. Adattività di PSF, pesi o lambda richiede ulteriore trattamento.
- Trasformata wavelet fissa B: `C_w = B C B^T`; thresholding generalmente non lineare. Dominio radiometrico lineare non significa operatore matematicamente lineare.
- Dopo trasformazione g, `J_g C J_g^T` è approssimazione locale. Clipping e trasformazioni forti richiedono validazione dell'intero modello, non riuso della varianza originale.

Preregistrare domini, soglie, seed, generatori, separazione dei campioni, baseline no-op e budget. Misurare bias, danno al segnale, correlazioni, ringing, falsa soluzione e astensione utile. L'holdout riusato adattivamente non è validazione indipendente. Nessuno di questi test è stato eseguito qui.

## Provenance, autenticità e installazione

Un hash dimostra uguaglianza rispetto a un riferimento fidato, non autentica da solo l'origine. Manifest proposto: URL/DOI, release, acquisizione, licenze/notices per componente, query e filtri, trasformazioni/errata, generatori/versioni, conteggi/copertura, byte e hash. Dichiarare riferimento fidato o firma del distributore; non presentarla come firma ESA. Esistenza di firme ufficiali sui pack non verificata.

Preflight di estrazione: percorsi, symlink, dimensioni/espansione, schema; pubblicazione atomica soltanto dopo verifiche. Pack mancante/corrotto: errore senza rete implicita o sostituzione. Aggiornamenti immutabili; le ricette esistenti restano agganciate al pack originale.

## Lacune e prosecuzione circoscritta

1. Completare letture primarie pertinenti C-15 e C-12; distinguere lettura, derivazione e validazione.
2. Chiudere diritti specifici APASS/Tycho-2 e curve/codice/indici prima dell'adozione pertinente. Gaia non commerciale è condizionato, non automaticamente approvato.
3. Definire schema e dominio minimo prima di misurare un pack lecitamente acquisibile. Nessuna acquisizione autorizzata da questo dossier.
4. Proseguire le attività M0/M1 indipendenti secondo i loro gate; non trasformare lacune di algoritmi M3 in blocco indiscriminato dello stack.

## Integrazione: lettura completa pertinente C-15/C-12

Prosecuzione nella stessa data, dopo la prima persistenza. Letto anche C-12 del piano 2.2, righe **1564-1581**, con `read`, dopo localizzazione tramite `grep`. La revisione 2.2.1 non è stata letta.

### Documento e acquisizione

Starck, Pantin e Murtagh, **Deconvolution in Astronomy: A Review**, PASP 114, 1051-1069 (2002), DOI `10.1086/342606`. È una review scientifica degli autori, non il lavoro originario di Tikhonov o una dimostrazione primaria di ogni risultato citato. Il link pubblico al PDF è stato verificato nella pagina dell'autore:

`webfetch https://jstarck.cosmostat.org/publications/signal_publi`

PDF: `http://jstarck.free.fr/pasp02.pdf`. Nessun paywall aggirato. Caricata la skill PDF. Verificati gli strumenti già disponibili con `command -v pdftotext; command -v curl`: `/usr/bin/pdftotext`, `/usr/bin/curl`. Nessuna installazione.

Comandi eseguiti dalla root del progetto:

```sh
set -o pipefail
curl --fail --silent --show-error --location --max-time 45 'https://jstarck.free.fr/pasp02.pdf' | pdftotext -layout - -
curl --fail --silent --show-error --location --max-time 45 'http://jstarck.free.fr/pasp02.pdf' | pdftotext -layout - -
curl --fail --silent --show-error --location --max-time 45 'http://jstarck.free.fr/pasp02.pdf' | pdftotext -layout -f 1 -l 10 - -
curl --fail --silent --show-error --location --max-time 45 'http://jstarck.free.fr/pasp02.pdf' | pdftotext -layout -f 1 -l 2 - -
for page in 3 11; do
  curl --fail --silent --show-error --location --max-time 45 'http://jstarck.free.fr/pasp02.pdf' | pdftotext -layout -f "$page" -l "$page" - - || exit
done
```

HTTPS ha fallito la connessione; il PDF HTTP esplicitamente linkato dall'autore è risultato accessibile. È un canale senza autenticazione TLS: il contenuto è utile come fonte pubblica, non attestazione crittografica. I comandi HTTP sono terminati senza errore. Streaming fra processi, nessun PDF o testo salvato manualmente. Il tool conserva automaticamente l'output, compresa la prima estrazione di 1201 righe.

**Copertura effettiva completa del testo estratto delle 19 pagine**, incluse didascalie e bibliografia: la prima risposta mostrava righe 679-1201; l'estrazione pagine 1-10 mostrava righe 132-667; le ulteriori letture pagine 1-2, 3 e 11 hanno coperto i tratti iniziali mancanti e la giunzione. Figure non ispezionate visivamente. Alcuni simboli delle equazioni risultano degradati nell'estrazione; non copiare formule OCR senza verifica del facsimile e derivazione indipendente. Nessun hash del PDF calcolato; richieste ripetute non costituiscono confronto di integrità dei byte.

### Risultati applicabili

| Punto letto | Evidenza testuale | Conseguenza per il progetto |
|---|---|---|
| §2, pp. 1052-1053 | Modello di convoluzione per sistema lineare e shift-invariant; errori della PSF si ripetono attorno agli oggetti | PSF fissa è un'assunzione da verificare, non conseguenza della sola FWHM. Non attivare un metodo myopic/blind quando fallisce |
| §3, pp. 1053-1054 | Tikhonov bilancia fedeltà ai dati e smoothness; "this noise estimation does not take into account errors relative to inaccurate knowledge of the PSF" | Coerente con C-15: propagazione condizionata non copre incertezza PSF; mantenere analisi di sensibilità e misura del condizionamento |
| §5.2, p. 1055 | "the number of iterations can be considered as a regularization parameter"; gli iterati possono prima avvicinarsi e poi allontanarsi dalla verità | Più iterazioni non dimostrano qualità. Stop preregistrato e test del danno |
| §6.3, p. 1059 | À trous ridondante, ogni scala ha dimensione dell'immagine; ricostruzione come somma di dettagli e residuo liscio | Fondamento della trasformata C-12 e del costo J+1 piani se conservati tutti; non è prova del budget reale con buffer riusati |
| §6.4, pp. 1059-1060 | Dopo inversione, rumore "is not white but remains Gaussian" nel modello trattato; "It is not trivial to consider non-Gaussian noise" | Non trasferire soglie bianche a rumore correlato; il caso discusso non copre automaticamente stretch, clipping o eteroschedasticità |
| §6.5, pp. 1061-1062 | Supporto multirisoluzione dipende dalla probabilità sotto un modello di rumore; esempio gaussiano con soglie per scala | La scelta empirica citata di 3-5 sigma non è un gate universale del nostro denoise né prova di controllo con dipendenze tra coefficienti |
| §§7-8, pp. 1065-1067 | Risoluzione obiettivo, aliasing e superrisoluzione richiedono assunzioni aggiuntive | Non promettere recupero oltre il dato per deconvoluzione ordinaria a PSF fissa |

La lettura sostiene il razionale di C-15 e C-12, non ne certifica l'implementazione. Alcune affermazioni generali della review sono troppo ampie se isolate: per esempio "No a priori information can be used" a proposito dei metodi lineari non va adottato letteralmente, perché la penalizzazione quadratica incorpora già una preferenza di regolarità. Il limite pertinente riguarda vincoli ulteriori come positività e adattività, che cambiano lo stimatore. Non importare inoltre positività o interpretazione del residuo come puro rumore nei casi con negativi legittimi ed errore di modello.

### Gap scientifici rimasti, senza blocco indiscriminato

- La review completa non equivale alla lettura dei lavori originali di Tikhonov/Hansen, delle prove di shrinkage o della specifica completa FITS. Questi restano non letti integralmente in questa sessione.
- C-15: la condizione di kernel e le formule matriciali sopra sono derivazioni indipendenti. Servono review numerica, verifica dell'aggiunto e dei bordi, scelta preregistrata di lambda/stop e test su PSF/fondo errati prima di dichiarare valido l'algoritmo specifico.
- C-12: restano da fissare filtro à trous, normalizzazione, soglie, rumore interscala/intercanale, bordi e maschere. Una base bibliografica per à trous non dimostra preservazione del segnale dopo soft shrinkage.
- Rumore correlato e dominio non lineare richiedono validazione dell'intera catena, incluse selezione, stima del rumore, stretch e clipping. Un paper su rumore gaussiano bianco non chiude tale requisito; una formula al Jacobiano non dimostra da sola accuratezza fuori regime locale.
- Nessun test empirico eseguito o dichiarato superato. Questi gap appartengono alla verifica del contratto prima dell'implementazione specifica e ai successivi gate dell'algoritmo; non vietano automaticamente confronti dello stack o spike FFT/convoluzione indipendenti ammessi dal piano.

Dossier minimo standalone sufficiente per registrare candidati, limiti legali e fondamento metodologico. La ricerca si arresta qui senza estenderla a tutti i lavori citati. La priorità successiva del ramo cataloghi è chiudere il dossier del prodotto esatto prima dell'adozione; quella dei singoli algoritmi è completare il contratto e il protocollo prima di implementarli.
