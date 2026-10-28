# Mock del ritiro Rust, turno 001

Il ramo operativo resta BLOCCATO. I quattro PID S/UID1000 con EACCES sono un vincolo ricevuto, non riesaminato. Questa prova non autorizza unlink e non chiude M1.

## Esecuzione

Comando dalla radice, 2026-10-27:

```text
python3 -I -B tmp/m1/a/retire-series001-rust-mock-001/outer.py
```

Python 3.14.4; Linux 7.0.0-38-generic x86_64. Una sola invocazione, unittest failfast=True: 8 metodi, 17 scenari comprendendo i subtest, tutti riusciti; uscita naturale 0. Nessuna ripetizione o correzione dopo l'esecuzione. Raw: `suite.stderr`, `suite.stdout` nel namespace nuovo.

PID proprio 3373426: identità start_ticks=120790305, sessione/gruppo propri, WNOWAIT osservato, identità ricontrollata prima del reap, wait consumante riuscito. Slot di questa esecuzione libero.

Limiti: CPU 50 s, wall 55 s, AS 268435456, FSIZE 1048576, NOFILE 64, CORE 0; output aggregato limitato a 1 MiB; HOME/TMP/XDG privati, directory 0700, umask 077. L'outer è nuovo: questo esito non costituisce una qualificazione indipendente dei suoi percorsi di timeout/overflow.

Preflight corrente: tmp allocati 936648704 B; cap 1073741824, riserva 67108864, margine 16777216, budget log/fixture mock 4194304. Misura dei soli metadati di allocazione con du; nessuna lettura dei payload concorrenti. Nessuna promessa di capienza futura.

## Comportamento dimostrato

UNKNOWN alla prima/seconda scansione blocca il finto unlink. ENOENT confermato distingue un PID scomparso da EACCES. Guasti quota/pin/evidence/hash/FIRSTFAIL persistono STOP simulato e consumano il namespace simulato. Errori write/fsync/close di marker e preflight impediscono unlink. Errore primario con close e STOP falliti resta primario e produce diagnosi di persistenza incerta. Il percorso positivo verifica marker-fsync prima dei controlli e preflight-fsync prima dell'unico unlink simulato.

I/O, autorizzazione e processi sono oggetti in memoria. Import preceduto da verifica SHA/AST, dipendenze sostituite; entrypoint escluso. Sentinella sul vero os.unlink attiva durante import e suite, zero chiamate. Nessuna autorizzazione reale, target aperto o cancellazione reale. Nessuna prova di durabilità filesystem.

I cinque pin SHA e identità pre/post coincidono: `pins-pre.json` e `pins-post.json`, entrambi SHA256 `ed117fa6719d6efe89848f1973b63ee7d3261e9fb9353f5c40a035b09bee1f51`.

SHA256 test: `0d79b1dcf292b79bee49b4ad2333ae2fffa34baf734a5e48d528de43fc45a29a`.

SHA256 outer: `303cc431b994ddf9cf26d249e2626d21177481ae4e3631087798e6edded75ab9`.

SHA256 receipt: `695500245f35ead0961b7138a7a1ea42a9c38d7a834157650c260a051d6c5cae`.
