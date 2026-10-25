# Stato del progetto astroProcessor

Ultimo aggiornamento: 25 ottobre 2026. Unica fonte dello stato corrente; risultati e limiti nei record di evidenza.

## Ora

- **M0 PASS per avvio degli esperimenti locali M1. M1 avviata: preparazione ed esecuzione del preflight M1.e affidate al coder; risultati ancora da consegnare.** Review indipendente dei minimi: `docs/evidence/M0-ops-review.md`. Il controllo degli artefatti da pubblicare resta distinto dal gate degli esperimenti.
- Piano operativo: `ASTRO_POSTPROCESSING_PLAN_v2.2.1.md`, SHA-256 `95abbb1a1622c1933ad603af1715524d202a70873f1e049a7514fecfa3ad4094`. Review indipendente PASS in `docs/evidence/M0-plan-review.md`; il PENDING interno è lo snapshot precedente, non lo stato corrente.
- Ricevuta dei minimi e controlli: `docs/evidence/M0-closure.md`. Nessun autore sta modificando il piano.
- Autore attivo: coder `ses_e8c79a7feffeP0DVNPDMLswIUo`, sorgenti isolati `tmp/m1/` e record M1.e/M1.a. Nessun codice di prodotto ancora scritto.
- D-19/D-20 approvate P0. D-21 approva la preferenza per pack inclusi, subordinata a verifiche; non attende un nuovo voto e non autorizza ad adottare pack incompatibili. D-02 resta riservata, senza bloccare gli spike generici.
- Repository locale su `work/m0-m1`, origin ufficiale verificato. Nessun commit o push ancora eseguito.

## Evidenze disponibili

| Unità | Record | Esito effettivo |
|---|---|---|
| M0.1 | `docs/evidence/M0-plan-review.md` | PASS del delta documentale; non validazione empirica |
| M0.2 | `docs/evidence/M0-environment.md` | Audit toolchain e inizializzazione; record pubblico anonimizzato |
| M0.3-M0.4 | `docs/decisions/M0-policy.md`, `docs/evidence/M0-catalogs-and-science.md` | Policy d'ingresso e limiti legali documentati; nessun catalogo adottato |
| M0.5 | `docs/test-matrix.md` | Protocollo per tre famiglie Linux; supporto non misurato |
| M0.6 | `plans/M1-spikes.md` | Preregistrati sei spike; due blocchi bash sintatticamente validi, non eseguiti |
| Archivio | `plans/M0-M1.md`, `doc_old/README.md`, `docs/evidence/M0-closure.md` | Otto file spostati senza modifiche; 8/8 hash ricontrollati |

## Prossima unità

Avviare **M1.e**, preflight dei backend GPU espliciti secondo `plans/M1-spikes.md`, poi i rami CPU a/b/c/d/f. Creare sorgenti isolati soltanto in `tmp/m1/`. Registrare comandi, sorgenti, manifest, errori e risultati reali; nessun cambiamento di backend dopo un guasto. Il gate complessivo M1 resta aperto fino alle misure comparabili e alla review della decisione tecnica.

## Limiti aperti

- Vulkan: compilatore shader assente; OpenCL: header/ICD assenti al probe M0. Non sono prove di prestazioni né motivi per fermare gli altri rami.
- Cataloghi: Gaia non-commerciale; diritti APASS/Tycho non verificati. Il ramo di adozione/distribuzione resta aperto; nessun accesso di rete occulto sostituisce il pack.
- M2-M9 non eseguite. Nessun ADR di stack è ancora adottato; nessun `LICENSE` del prodotto emesso.
- CodeGraph/Graphify disponibili, indici assenti. AGENTS vieta di crearli; non sono prerequisiti M1.
- File draft di provenienza incerta preservato in `doc_old/`, escluso dalla pubblicazione finché non verificato.
