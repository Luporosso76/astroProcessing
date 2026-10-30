# PUB-REG-01: ESO EFOSC2, NGC 2808, filtro R#642, una notte

- Stato: DA ACQUISIRE. Licenza SPDX: `CC-BY-4.0`.
- Fonte: ESO Science Archive Facility, https://archive.eso.org/. Strumento EFOSC2 al NTT (La Silla), programma `092.D-0449(A)`. Selezione dalla tabella `dbo.raw` del servizio TAP `https://archive.eso.org/tap_obs/sync`, interrogata il 30 ottobre 2026.
- Licenza: ESO Data Access Policy del 7 novembre 2022, https://archive.eso.org/cms/eso-data-access-policy.html, letta il 30 ottobre 2026: «All data in the ESO archive retain ESO's copyright» e sono «distributed according to the terms of the Creative Commons Attribution 4.0 International license». Per chi ridistribuisce: «Credit to the ESO provenance of the data must be acknowledged, and the file headers preserved». Le calibrazioni sono «public immediately after the observations»; i dati scientifici dopo il periodo proprietario (tipicamente un anno; osservazioni del 2014). Non è un parere legale.
- Attribuzione: «Based on observations collected at the European Southern Observatory under ESO programme 092.D-0449(A)».
- Header: i file si conservano come consegnati dall'archivio; nessun test li riscrive. Un estratto, se servirà, conserva l'header originale intero.
- Rilascio: l'archivio non ha versioni; l'identità di ogni file è il suo `dp_id`.
- Hash: l'archivio non pubblica uno SHA-256 per file (nessuno trovato). All'acquisizione si calcola lo SHA-256 del file consegnato e, se compresso, del FITS decompresso, e si scrivono qui prima di ogni test. Se l'header porta `CHECKSUM` e `DATASUM` (standard FITS), si verificano all'acquisizione come controllo d'integrità indipendente dal progetto; la presenza delle due keyword è un'ipotesi.
- Copia locale prevista: `data/public/eso-092.D-0449A/` (esclusa da git), un file per `dp_id`.
- Uso: T-REG-02 (M3.f), registrazione e stacking nello stesso filtro (f, g, h), E2E pubblico di M3.s e benchmark B1, Gate M3 punto 1; hint reali (puntamento negli header) per M3.p.

## File scelti

Notte 23–24 febbraio 2014 (UTC). Conteggi dalla risposta TAP filtrata per strumento `EFOSC`, intervallo `2014-02-23T12:00` – `2014-02-24T12:00`, `dp_type = 'BIAS'` oppure `filter_path = 'R#642'`: 21 bias, 6 flat a cupola R#642, 6 flat di cielo R#642, 26 `OBJECT` R#642 di cui 15 da 150 s. Si acquisiscono i 15 light da 150 s di NGC 2808 e tutte le calibrazioni della notte nello stesso filtro, 48 file; circa 1,9 MB ciascuno secondo il record di ricerca (`docs/datasets/M3-candidates.md`), stima non misurata.

| Ruolo | `dp_id` | Esposizione s |
|---|---|---|
| light NGC 2808 | `EFOSC.2014-02-24T01:24:23.691`, `T01:27:33.994`, `T01:30:44.457`, `T02:06:54.865`, `T02:10:06.218`, `T02:13:16.732`, `T03:02:44.406`, `T03:05:54.650`, `T03:09:05.073`, `T04:02:13.533`, `T04:05:25.387`, `T04:08:35.971`, `T05:02:30.327`, `T05:05:41.751`, `T05:08:52.194` (tutti `EFOSC.2014-02-24`) | 150 |
| bias, sera | `EFOSC.2014-02-23T20:17:37.847`, `T20:18:08.429`, `T20:18:38.861`, `T20:19:09.463`, `T20:19:40.074`, `T20:20:10.836`, `T20:20:40.748`, `T20:21:11.390`, `T20:21:41.881`, `T20:22:12.483`, `T20:22:42.875` (tutti `EFOSC.2014-02-23`) | 0 |
| bias, mattina | `EFOSC.2014-02-24T10:04:57.700`, `T10:05:50.550`, `T10:06:43.450`, `T10:07:36.231`, `T10:08:28.841`, `T10:09:20.971`, `T10:10:13.762`, `T10:11:06.542`, `T10:11:59.362`, `T10:12:52.142` (tutti `EFOSC.2014-02-24`) | 0 |
| flat a cupola R#642 | `EFOSC.2014-02-23T20:12:34.990` (1,0 s); `T20:13:10.203`, `T20:13:57.121`, `T20:14:43.649`, `T20:15:30.537`, `T20:16:16.776` (12,41 s) | 1,0; 12,41 |
| flat di cielo R#642 | `EFOSC.2014-02-23T23:56:12.538` (1,0 s), `T23:56:48.712` (12,0), `T23:57:35.280` (14,2), `T23:58:24.080` (17,1), `T23:59:16.309` (20,8), `EFOSC.2014-02-24T00:00:11.641` (25,6) | 1,0–25,6 |

Nessun dark nella risposta TAP per quella notte: la calibrazione disponibile è bias e flat. La scelta della ricetta (D-16) la fa l'analisi G-CAL sui file acquisiti, non questo record.

Hash SHA-256 dei file: da scrivere all'acquisizione, nel formato `<sha256>  <percorso relativo>` usato da `PUB-LFC-01.md`.

## Limiti

- L'elenco dei `dp_id` è stato letto attraverso uno strumento di consultazione web che riassume la risposta CSV del TAP, non dal CSV grezzo. All'acquisizione la query si ripete e il CSV grezzo si conserva accanto ai file; un `dp_id` diverso o mancante ferma l'acquisizione e il record torna in review.
- Lo stesso programma ha 73 righe NGC 2808 R#642 in quattro notti (24–27 febbraio 2014) secondo la risposta TAP del 30 ottobre; il record di ricerca ne riportava 67 light da 150 s. La differenza non è risolta: questo record usa una sola notte e non dipende dal totale.
- Il canale di download (portale dati ESO, file pubblici senza autenticazione) non è stato provato.
- Ammasso globulare affollato: la registrazione lavora su un campo denso; è una proprietà del dato, dichiarata.
- I flat da 1,0 s possono essere esposizioni di prova; la loro esclusione, se serve, la decide l'analisi dei master, non questo record.
