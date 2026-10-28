# M1: diritti dei candidati APASS DR10 e Tycho-2

Data di consultazione: 2026-10-25. Stato: **nessun catalogo approvato o adottato**. Ricerca documentale circoscritta per D-21, non attestazione legale. Lo storico `M0-catalogs-and-science.md` resta immutato; Gaia non è riesaminato.

Autore: OpenAI GPT-6 Astra, `openai/gpt-6-astra`, come dichiarato dall'ambiente; identità non verificata indipendentemente. Nessuna delega. Costi/token non esposti. Nessuna acquisizione di cataloghi, test, benchmark, esecuzione di payload, contatto esterno, credenziale, installazione o operazione Git. Nessun namespace attivo consultato o modificato. Scritture limitate a questo nuovo report e all'unico PDF normativo autorizzato.

## Esito e confronto

| Candidato | Diritto verificato | Limite | Stato per pack incluso |
|---|---|---|---|
| APASS DR10 | Linee guida AAVSO su accesso e riconoscimento scientifico, con voce DR10 | Nessuna concessione esplicita di redistribuzione trovata nel documento, né commerciale né non commerciale | **Da chiarire; non adottabile sulla sola evidenza raccolta** |
| Tycho-2 tramite archivi ESA | Concessione CC BY-NC 3.0 IGO di riproduzione, distribuzione e adattamento, soggetta ai termini | Uso commerciale richiede autorizzazione preventiva; notices e attribuzioni obbligatorie. Non dimostrata una licenza alternativa per altre distribuzioni | **Non adottabile per pack commerciale senza autorizzazione; candidato condizionato per uso effettivamente non commerciale**, non approvato |

APASS è stato esaminato per primo. L'assenza di una concessione redistributiva nel PDF ha motivato il solo secondo passaggio su Tycho-2. Non si deduce un divieto universale da una licenza non trovata; manca la prova necessaria per adottare il pack.

## APASS: fonte vigente e lettura integrale

Pagina istituzionale consultata con `webfetch`: <https://aavso.org/about/governing-documents>. Documento collegato: <https://aavso.org/wp-content/uploads/2026/07/Data-Usage-Guidelines-2026-07.pdf>, titolo **AAVSO Data Usage Guidelines**, data interna **2026-07-16**. Questa fonte corrente sostituisce, per la presente verifica, il riferimento storico al PDF non letto; non modifica lo storico.

Letto integralmente il testo estratto delle **tre pagine**: introduzione, General Expectations, Recommended Acknowledgements e tabella AAVSO Facility Keywords, inclusa l'intera riga APASS. Nessuna lettura visiva del facsimile dichiarata. Il testo estratto è stato riletto integralmente dopo la ripresa della sessione.

Citazioni brevi esatte, con ritorni a capo normalizzati:

- Introduzione: “The AAVSO provides free access to a growing collection of data”. È accesso gratuito, non una concessione esplicita di redistribuzione.
- General Expectations: “Recognize the use of AAVSO data, products, or services in the acknowledgement section of their publications.”
- APASS: “This research was made possible through the use of the AAVSO Photometric All-Sky Survey (APASS), funded by the Robert Martin Ayers Sciences Fund and NSF AST-1412587.”
- La citazione raccomandata è Henden et al., “APASS Data Release 10”, vol. 232, Art. no. 223.06, 2018; facility keyword `AAVSO (APASS)`.

**Fatto verificato:** il documento contiene richieste e raccomandazioni per riconoscimenti/citazioni, non termini espliciti che autorizzino copie distribuite da terzi, subset commerciali o indici inclusi. Non chiarisce sublicenza, distribuzione modificata e uso commerciale. Le richieste di comunicare pubblicazioni o donare non sono state eseguite. **Conclusione progettuale:** una citazione corretta non basta per approvare la redistribuzione. L'assenza nel documento non prova l'assenza di ogni altra possibile autorizzazione AAVSO; nessuna autorizzazione ulteriore è attestata qui.

## Tycho-2: canale e condizioni effettive

Fonte ESA: <https://www.cosmos.esa.int/web/hipparcos/tycho-2>. Letti descrizione, Reference Papers e Catalogue Access; consultazione `webfetch`, poi HTTP `fetch` in memoria con estrazione del testo HTML per verificare il collegamento al canale esatto. La pagina afferma: “The data is available in the Hipparcos and Tycho Legacy Archive” e indica il servizio ESASky Legacy TAP. Descrive “positions, proper motions and two-colour photometry”. Nessuna query TAP o acquisizione di dati eseguita.

Fonte normativa: <https://www.cosmos.esa.int/web/esdc/terms-and-conditions>. Letti integralmente i termini d'uso dati, punti **1–7**, la riga crediti Hipparcos e il paragrafo dei termini di servizio. Citazioni:

- “Data hosted in the ESA Space Science Archives are distributed under the CC BY-NC 3.0 IGO licence.”
- Punto 2: “Prior to any commercial use” e “including any use or application that directly or indirectly generates a financial gain”. Richiede una richiesta dettagliata di autorizzazione/licenza; non è un'autorizzazione già concessa.
- Crediti: riga **Hipparcos**, “Credit: ESA”. La tabella non contiene una riga distinta denominata Tycho-2.

**Collegamento documentale:** la pagina Tycho-2 indica l'archivio ESA e i termini ESDC coprono i dati ospitati negli archivi ESA. È la base per valutare questo canale sotto CC BY-NC; non una prova che ogni copia presso qualsiasi distributore abbia necessariamente gli stessi termini. Nessuna deroga Tycho-2 trovata nelle pagine lette. I termini escludono endorsement e garanzie, non concedono diritti esclusivi e non garantiscono assenza di violazioni altrui. Il servizio è destinato a ricerca/educazione, senza garanzie operative.

Consultata anche la FAQ degli autori presso l'Università di Copenaghen: <https://www.astro.ku.dk/~erik/Tycho-2/faq.html>. È una fonte storica con riferimenti e collegamenti datati, utile sul reperimento ma non usata come concessione redistributiva. Nessuna licenza permissiva alternativa attestata dalla FAQ.

Testo normativo CC letto integralmente, **sezioni 1–8 e notice**, non soltanto deed: <https://creativecommons.org/licenses/by-nc/3.0/igo/legalcode.en>. Lettura per il ramo Tycho-2, non riapertura del dossier Gaia. È una versione precedente alla 4.0, ma quella espressamente richiamata da ESA; non si sostituisce automaticamente con una licenza più recente.

- §3 concede “to Reproduce, Distribute and Publicly Perform the Work” e adattamenti, identificando le modifiche.
- §4(b) esclude esercizio dei diritti “primarily intended for or directed toward commercial advantage or private monetary compensation”. Il punto 2 ESA esplicita anche guadagno indiretto: conservare entrambe le formulazioni, senza ridurre la seconda alla sola vendita del file.
- §§4(a), 4(c): licenza o URI, notices e attribuzioni, titolo/URI pertinenti, credit dell'adattamento; nessuna restrizione aggiuntiva o misura tecnologica incompatibile sui dati. Vietato suggerire endorsement.
- §2 preserva gli usi liberi da copyright; non è una dimostrazione che tutto il catalogo o un indice particolare siano privi di protezione.
- §§5–8: esclusioni di garanzie/responsabilità, cessazione per violazione, condizioni di ripristino, regole residue e dispute. I termini ESA contengono proprie disposizioni sulle controversie; questo report non ne certifica il coordinamento legale.

**Esito:** esiste una base esplicita per redistribuzione non commerciale condizionata dal canale ESA, non un divieto assoluto di redistribuire. Non esiste nella lettura un'autorizzazione commerciale preventiva già ottenuta. L'adozione resta separata dalla sola identificazione della licenza.

## Separazione dei componenti e dei modelli di distribuzione

| Componente | Valutazione e limite |
|---|---|
| Codice applicazione/generatore | Licenza indipendente dai dati. Nessuna licenza di progetto scelta o cambiata; nessun codice di terzi acquisito |
| Dati originali | APASS: grant redistributivo da chiarire. Tycho-2 ESA: termini sopra, conservando notices specifiche dell'artefatto futuro |
| Subset, conversione, indice geometrico | Trasformare o ricostruire internamente non prova libertà da diritti. Per Tycho-2 gli adattamenti sono contemplati, ma restano condizioni NC e identificazione delle modifiche. Classificazione giuridica dell'indice esatto non attestata |
| Bande/passband e calibrazioni | Magnitudini del catalogo e tabelle di risposta spettrale sono artefatti distinti. Nessuna curva, libreria fotometrica o sua licenza è stata verificata/acquisita; i nomi delle bande non concedono diritti sulle curve |
| Open/closed e commerciale/non commerciale | Sono assi diversi. Codice aperto o download gratuito non prova uso non commerciale; codice chiuso non prova da solo uso commerciale. Separare notices e termini dati dall'EULA software; non imporre ai dati restrizioni incompatibili |

Non dichiarare il pack “open” senza precisarne i limiti NC. Un componente software indipendente non diventa automaticamente soggetto alla licenza dati; ciò non autorizza a includere i dati in un'offerta commerciale. Nessuna compatibilità scientifica delle bande o densità del catalogo è stata misurata.

## Acquisizione, integrità e quota

Preflight globale osservato: **857067520 byte**. Guard eseguito: occupazione + 8388608 byte massimi PDF + 33554432 byte margine <= 1073741824 byte. Namespace nuovo `tmp/m1-catalog-rights/`, mode 0700. Unico download persistente: **96700 byte**, SHA-256 **`8ead332c240a1ada50417f859acdfe28a2685fe83329da201158171ceff87d98`**. Hash locale per identificare il documento, non firma AAVSO né prova della licenza del catalogo.

Comandi effettivi dalla root:

```sh
du -s -B1 tmp
mkdir -m 700 tmp/m1-catalog-rights
TMPDIR="$PWD/tmp/m1-catalog-rights" curl -q --fail --silent --show-error --location --proto '=https' --max-filesize 8388608 --max-time 90 'https://aavso.org/wp-content/uploads/2026/07/Data-Usage-Guidelines-2026-07.pdf' -o tmp/m1-catalog-rights/Data-Usage-Guidelines-2026-07.pdf
sha256sum tmp/m1-catalog-rights/Data-Usage-Guidelines-2026-07.pdf
stat -c '%s bytes' tmp/m1-catalog-rights/Data-Usage-Guidelines-2026-07.pdf
TMPDIR="$PWD/tmp/m1-catalog-rights" pdftotext -layout tmp/m1-catalog-rights/Data-Usage-Guidelines-2026-07.pdf -
```

La rilettura dopo ripresa ha usato anche `pdftotext -layout ... -`, output stdout senza file di testo. Skill PDF già caricata nella sessione. Strumenti preesistenti, nessuna installazione; versioni non rilevate per questo compito. Nessun temporaneo/cache manuale esterno. `webfetch`, `websearch` e `execute/fetch` hanno consultato pagine pubbliche; risultati di ricerca non usati come prova autonoma di licenza. Ultima occupazione osservata dopo acquisizione: **857169920 byte**, margine **216571904 byte**, superiore a 32 MiB. Quota campionata, non prenotazione contro writer concorrenti.

## Decisione operativa circoscritta

Non includere ora nessuno dei due cataloghi in un pack commerciale D-21. Per APASS manca una concessione redistributiva verificata; per Tycho-2 ESA manca autorizzazione commerciale e restano da congelare artefatto/notices prima dell'eventuale adozione non commerciale. Nessun contatto viene avviato. Si arresta questa ricerca circoscritta; il blocco riguarda l'adozione del pack, **non M1 né i rami indipendenti M0–M9**. Nessun fallback o scelta irreversibile di catalogo introdotti.
