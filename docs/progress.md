# Stato del progetto astroProcessor

Ultimo aggiornamento: 25 ottobre 2026. Unica fonte dello stato corrente; risultati e limiti nei record di evidenza.

## Ora

- **M0 PASS. M1.a/M1.e hanno consegnato risultati reali, ma i rispettivi gate restano parziali.** La review M1.a conferma numerica e statistiche; tre rilievi sui contratti d'errore sono in correzione. M1.e: hardware AMD e tool shader locali disponibili, compute non qualificato e nessuno speedup dichiarato. Il controllo degli artefatti da pubblicare resta distinto dai gate degli esperimenti.
- Piano operativo: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. Review indipendente PASS in `docs/evidence/M0-plan-review.md`; il PENDING interno è lo snapshot precedente, non lo stato corrente.
- Ricevuta dei minimi e controlli: `docs/evidence/M0-closure.md`. Nessun autore sta modificando il piano.
- Attivi: coder `ses_e8c14fabaffeCEsreEF1hHtERa` esegue le regressioni dei contratti d'errore M1.a, con ammissione esclusiva. Gli altri preparano soltanto sorgenti: `ses_e8c79a7feffeP0DVNPDMLswIUo` M1.f, `ses_e8c3d6129ffeFj3KFYadWdzsug` Vulkan, `ses_e8c255272ffeYHdt1kmmJ7SiWJ` viewer, `ses_e8c05a853ffeV1jnKQ3fWQQ6wx` ciclo di vita dello spill. Review ownership e risultati M1.c concluse; nessuna misura comparativa concorrente e nessun codice di prodotto ancora scritto.
- D-19/D-20 approvate P0. D-21 approva la preferenza per pack inclusi, subordinata a verifiche; non attende un nuovo voto e non autorizza ad adottare pack incompatibili. D-02 resta riservata, senza bloccare gli spike generici.
- Repository su `work/m0-m1`, origin ufficiale. Commit iniziale `fc3d3d8b612c50dbf97674568a8109a2b6e33c93` pubblicato e verificato sul remoto; ricevuta in `docs/evidence/M0-publication.md`. Gli otto storici restano locali, non tracciati; `tmp/` è ignorata.

## Evidenze disponibili

| Unità | Record | Esito effettivo |
|---|---|---|
| M0.1 | `docs/evidence/M0-plan-review.md` | PASS del delta documentale; non validazione empirica |
| M0.2 | `docs/evidence/M0-environment.md` | Audit toolchain e inizializzazione; record pubblico anonimizzato |
| M0.3-M0.4 | `docs/decisions/M0-policy.md`, `docs/evidence/M0-catalogs-and-science.md` | Policy d'ingresso e limiti legali documentati; nessun catalogo adottato |
| M0.5 | `docs/test-matrix.md` | Protocollo per tre famiglie Linux; supporto non misurato |
| M0.6 | `plans/M1-spikes.md` | Preregistrati sei spike; due blocchi bash sintatticamente validi, non eseguiti |
| Review M1 | `docs/evidence/M1-protocol-review.md`, `docs/evidence/M1-a-review.md` | Rettifiche preregistrate applicate alla serie corrente; numerica e bootstrap ricalcolati indipendentemente, contratti d'errore ancora da chiudere |
| M1.e | `docs/evidence/M1-e.md` | Probe Vulkan/OpenGL eseguiti; nessun kernel compute, confronto GPU/CPU o backend adottato |
| M1.a | `docs/evidence/M1-a.md`, `docs/evidence/M1-a-review.md`, `docs/evidence/M1-a-ownership-review.md` | Numerica e statistiche confermate; ponte ownership single-thread verificato. Contratti d'errore ancora da validare |
| M1.c | `docs/evidence/M1-c-validation.md`, `docs/evidence/M1-c-validation-review.md` | Eseguiti 44/44 casi SQLite e 46/46 JSON, artefatti verificati indipendentemente; nessun benchmark comparativo |
| Contratti M1.a | `docs/evidence/M1-a-error-contract-followup.md`, `docs/evidence/M1-a-error-review.md` | Correzioni statiche confermate; prima esecuzione delle regressioni della nuova versione in corso |
| M1.d | `docs/evidence/M1-d-review.md`, `docs/evidence/M1-d-corrections.md` | Tre correzioni confermate staticamente. Nuova preregistrazione del ciclo di vita dello spill in preparazione; nessuna validazione dinamica |
| Tool M1.e | `docs/evidence/M1-e-tools.md` | Autenticati pacchetti e prefisso locale, due comandi di versione riusciti; non prova compute |
| Archivio | `plans/M0-M1.md`, `doc_old/README.md`, `docs/evidence/M0-closure.md` | Otto file spostati senza modifiche; 8/8 hash ricontrollati |

## Prossima unità

Completare regressioni e review dei contratti d'errore **M1.a**, poi ammettere una sola esecuzione alla volta fra M1.c, M1.d, M1.e e le nuove preparazioni M1.b/M1.f. Per **M1.d**, verificare prima il ciclo di vita preregistrato: conservare ogni prova fallita e liberare soltanto intermedi riusciti dopo ricevuta durevole; nessun rilascio contabile mentre i blocchi restano allocati. Lo storage sparse ha preservato i 168 hash delle fixture; dieci copie di tool non selezionati sono state rimosse mantenendo i deb integrali. Prima di ogni esecuzione verificare quota globale e supervisore libero: questi risparmi non prenotano lo spazio per lo spill fisico di 256 MiB. Il gate complessivo M1 resta aperto fino alle sei prove e alla review della decisione tecnica.

## Limiti aperti

- Vulkan: compilatore shader ora disponibile nel prefisso locale autenticato, candidato compute ancora da provare. OpenCL: header/ICD assenti al probe M0. Non sono prove di prestazioni né motivi per fermare gli altri rami.
- Cataloghi: Gaia non-commerciale; diritti APASS/Tycho non verificati. Il ramo di adozione/distribuzione resta aperto; nessun accesso di rete occulto sostituisce il pack.
- M2-M9 non eseguite. Nessun ADR di stack è ancora adottato; nessun `LICENSE` del prodotto emesso.
- CodeGraph/Graphify disponibili, indici assenti. AGENTS vieta di crearli; non sono prerequisiti M1.
- File draft di provenienza incerta preservato in `doc_old/`, escluso dalla pubblicazione finché non verificato.
