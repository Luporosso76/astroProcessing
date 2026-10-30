# M3.entry, D-08 e NFR-TEST-02: dataset pubblici e uso nei test

Stato: **review PASS al giro 2 di 3** (nessun rilievo aperto). Unità: M3.entry, riga «D-08, NFR-TEST-02» di `plans/M3.md`. Solo testo: nessun codice, nessun test eseguito, nessun dato scaricato.

| Campo | Valore |
|---|---|
| Unità | M3.entry / D-08, NFR-TEST-02 |
| Scopo | Fissare per ogni slice M3 il dato pubblico scelto, con licenza, fonte, rilascio, hash e stato; fissare come i test usano il dato pubblico |
| Comando | `python3 -S tools/trace.py ids --stage M3.entry`; `grep` su `tests/` per PUB-LFC-01 e PUB-CFA-01; lettura dei metadati di raw.pixls.us con `tmp/d08/rpu.py` e `tmp/d08/pick.py` (non versionati); consultazione web (licenze ESO, SDSS, MAST, record Zenodo 3989223, servizio TAP ESO, ricerca sulla licenza 2MASS); `python3 -S tools/evidence.py docs/evidence/M3-entry-D08.md` |
| Input | Piano attivo (SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`): NC-16 riga 379, NFR-LIC-01 riga 2018, NFR-TEST-02 righe 2020 e 3367, §21.5 righe 2681-2703, D-08 righe 2874 e 3458, archi righe 3917 e 4028. `plans/M3.md` (M3.entry, «Test, verità di riferimento e dati», «Dati pubblici e blocchi»); `docs/datasets/M3-candidates.md`; ADR-08.b §4 e §6; `tests/unit/test_fits.cpp`, `test_import_real.cpp`, `test_calibration_real.cpp`, `tests/unit/CMakeLists.txt`; metadati raw.pixls.us del 29 ottobre 2026 (`tmp/datasets-research/rpu_repo.json` nella copia principale, SHA-256 `3908c07909ebc66d6d0a8ddbdb56e1bc4f480f4e5b1703af256d7c9ca82e55f5`) |
| Versioni | Worktree da `463ccf9` |
| Ambiente | Linux 7.0.0-38-generic x86_64, macchina di sviluppo; rete solo per pagine pubbliche |
| Risultato | Verbale `docs/decisions/D-08-dataset.md`; record nuovi `docs/datasets/PUB-RAW-01.md` e `PUB-REG-01.md` (DA ACQUISIRE); riga `Stato`/`Licenza SPDX` aggiunta a `PUB-LFC-01.md` e `PUB-CFA-01.md`; 6 ID pianificati `T-M3A-DAT-01…06` e riga T-M3A-RAW-09 aggiornata in `docs/test-matrix.md` |
| Output | File elencati nel blocco generato sotto; artefatti di review in `.omc/artifacts/ask/` |
| Ambito | Decisione e contratto d'uso. Fuori: acquisizione dei dati (passo separato del coordinatore), codice dei controlli `T-M3A-DAT-*`, scelta dei file di PUB-DEEP-01 (M3.i), dei campi SDSS (M3.q) e del corpus (M3.r) |
| Esito | Review PASS al giro 2 (Codex `gpt-6-astra`); vedi «Review» |
| Limiti | Vedi sotto |
| Autore e data | Agente Claude (modello dichiarato dal runtime: Opus 5.5), 30 ottobre 2026 |

## Fatti verificati

- `data/public/` nella copia principale contiene solo `zenodo-3254683` (PUB-LFC-01) e `zenodo-17843622` (PUB-CFA-01), 552 MB in tutto (`du -shL`); nessun RAW. Il worktree dell'unità non ha `data/`.
- `.gitignore` riga 7: `/data/`.
- Test M2 su dato pubblico: `test_fits.cpp` legge l'hash dal record (`documented_sha256`); `test_import_real.cpp` e `test_calibration_real.cpp` hanno copie letterali degli SHA-256; tutti falliscono se il file manca (`ASSERT_TRUE(fs::exists…)`, `ASSERT_EQ` sul digest, eccezione «dato reale mancante»); nessun `GTEST_SKIP` né `DISABLED_` in `tests/`. `T-M2R1-09.public_samples` non ha la guardia `FAIL_REGULAR_EXPRESSION` sul caso «0 tests».
- raw.pixls.us: gli otto file scelti sono CC0 nei metadati del 29 ottobre; il 30 ottobre la URL dei metadati restituisce un testo della stessa lunghezza. Le pagine `data/<marca>/<modello>/` non mostrano licenza né hash.
- ESO: policy del 7 novembre 2022 letta il 30 ottobre (CC BY 4.0, credito, header conservati). TAP: 21 bias, 6 flat a cupola e 6 flat di cielo R#642, 15 light NGC 2808 da 150 s nella notte 23–24 febbraio 2014; 73 righe NGC 2808 R#642 nel programma.
- Zenodo 3989223: CC BY 4.0, v1, un tar da 15,1 GB, MD5 `7d201a0be5648a8b3ab5e5af1a8a3239`.
- SDSS: «All SDSS data released in our public data releases are considered in the public domain».
- MAST: «science data become available for public use without restriction»; «Most data hosted at MAST are in the public domain».
- 2MASS: nessuna pagina primaria trovata con una dichiarazione di pubblico dominio; la dichiarazione esiste in una risposta dell'helpdesk del 2015 citata nel copyright Debian.

## Decisioni

- Misto: sintetico come verità, pubblico dove il piano lo chiede; lista chiusa di licenze di ADR-08.b §4.3 anche per i dati di test.
- M3.a: PUB-RAW-01, otto file CC0 (sette marche Bayer più un X-Trans) con SHA-256 pubblicato dalla fonte.
- M3.f e dipendenti, E2E e Gate M3: PUB-REG-01, una notte EFOSC2 (48 file), hash all'acquisizione.
- o e p: licenza aperta (HST, 2MASS); q nel footprint SDSS; corpus r su campi disgiunti.
- NFR-TEST-02: hash nel record come unica casa, verifica sugli stessi byte prima dell'uso, dato assente = test fallito, sola lettura, solo file registrati, corpus separato.

## Limiti

- Nessun dato acquisito in questa unità: gli hash di PUB-RAW-01 sono quelli pubblicati dalla fonte, non ricalcolati.
- Gli elenchi del TAP ESO sono passati da uno strumento che riassume la risposta; si rileggono all'acquisizione.
- Licenze HST e 2MASS non chiuse.
- Verifica delle licenze non legale.

## Review

Revisore: Codex `gpt-6-astra`, reasoning high, sola lettura, con `tools/review.sh`.

| Giro | Artefatto | Delta | Verdetto | Rilievi |
|---|---|---|---|---|
| 1 | `.omc/artifacts/ask/codex-d08-round1-20261030-0443.md` | `tmp/d08-round1.diff` | FAIL | 3 MED: T-M3A-DAT-01 non copriva copie versionate fuori da `data/public/`; T-M3A-DAT-02 controllava l'appartenenza dell'hash e non la coppia percorso-hash; T-M3A-DAT-03 non verificava l'adozione del supporto nei test esistenti (`EXPECT_EQ` non fatale in `test_fits.cpp`, rilettura per percorso). 1 LOW: il file Sony 1582 è un modo «APS-C crop», sostituito con 1581 «Fullframe non-crop» |
| 2 | `.omc/artifacts/ask/codex-d08-round2-20261030-0448.md` | `tmp/d08-round2.diff` | PASS | Rilievi del giro 1 chiusi; nessun rilievo nuovo |

<!-- evidence:begin -->
Blocco generato da `tools/evidence.py`; non modificare a mano.

| File | SHA-256 |
|---|---|
| `docs/datasets/M3-candidates.md` | `a6414361632bad160c9ffeeb3d3fefecd1544354ce9f09b079a751c8f4f519e8` |
| `docs/datasets/PUB-CFA-01.md` | `59e9b5c084a2938ef6487697e1bdbeb5ef8d3ec320c3a38c11d29a8625f31edd` |
| `docs/datasets/PUB-LFC-01.md` | `50e2b701f428d5bfbc8d5a8fb3dd5500668f44d2d6c180416e87c3d62f28c5e3` |
| `docs/datasets/PUB-RAW-01.md` | `a0ff1c535ca37abee9fde2aaf7117a14af87bfcc5cc71e04ab95e876d7631000` |
| `docs/datasets/PUB-REG-01.md` | `c9376dacca580a1fc47f6c67ab682f01661a704866a57634fe5044632133679f` |
| `docs/decisions/D-08-dataset.md` | `9bfd000039b91e2ff363cc5ce5016ee49538dc293d3c940101c147885e3653d4` |
| `docs/test-matrix.md` | `46029cf97cd57f6a4733c626ddf40ae83eb4d7f577353e65579cd71c9ea5af55` |

<!-- evidence:end -->
