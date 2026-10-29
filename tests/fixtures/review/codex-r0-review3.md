[BLOCCANTE] docs/contracts/C-02-master.md:197 — Le scale dipendono dagli stessi campioni e dal dark-flat: fissarle dopo il calcolo non rende valide le formule a coefficienti indipendenti. Con tre flat e un solo pixel valido, `M=median(x1,x2,x3)` e `Cov(M,DF)=0`, mentre il contratto produce generalmente `(1−s_mean)V_DF≠0`. La cancellazione successiva in `Q` non corregge `Var(M)` o `Var(Fc)`. Il difetto ricorre alle righe 202, 210, 246, 275–283 — derivare la propagazione delle scale e le relative covarianze, oppure delimitare un dominio giustificato.

[BLOCCANTE] docs/contracts/C-02-master.md:103 — L’esattezza della media e il limite della mediana ignorano il bias di `observed_clamped`: per bias gaussiano con media `offset`, `read_noise_adu=2` e gain=1, `E[v(x)]≈4.7979` contro varianza vera 4; il rapporto della media è ≈0.8337 e quello della mediana con n=9 ≈0.7934, fuori dagli intervalli dichiarati. Sono coinvolte anche le righe 90–111 e T-M2R2-04 a riga 246 — derivare l’errore dello stimatore effettivo e il suo dominio prima di fissare le tolleranze.

[BLOCCANTE] tools/clip_variance_mc.py:74 — Il Monte Carlo applica una soglia costante `k` e propaga `(1+qvar)/n_kept` a riga 112; il contratto applica invece `k·sqrt(v(c))` e propaga `Σv(x_f)/n_kept²`. Inoltre il termine di quantizzazione aggiunto dall’oracle non compare nella formula contrattuale. `δ_clip=0.02` misura quindi uno stimatore diverso; anche «scala non stimata» in C-02:121 ignora la dipendenza dal centro casuale — allineare la derivazione allo stimatore specificato e rigenerare evidenza, dominio e tolleranze.

[BLOCCANTE] tools/mutate.sh:38 — Il ripristino con `cp -p` riporta il timestamp del sorgente a prima della compilazione del mutante: una build incrementale successiva può conservare gli oggetti mutati nonostante `cmp` confermi il sorgente originale — isolare la build della mutazione oppure invalidare e ricostruire gli artefatti dopo il ripristino.

[BLOCCANTE] tools/mutate.sh:75 — Qualunque uscita non nulla di CTest diventa «RILEVATA» ed exit 0, anche quando un test è `Not Run` per errore del loader o dell’interprete — leggere risultati strutturati e distinguere fallimenti osservati dai test dagli errori infrastrutturali, che devono invalidare la prova.

[BLOCCANTE] tools/trace.py:169 — Il token viene risolto soltanto fra i test presenti nel JUnit; il confronto con la build richiede un qualunque match. Per esempio T-M2A-02 comprende quattro test, ma un JUnit con uno solo passato certifica comunque il token — ricavare tutti i nomi attesi dalla build e richiederli presenti e passati, con negativi per gruppi incompleti e nomi rinominati.

[BLOCCANTE] tools/lint.sh:42 — Un errore di `jq` nella process substitution non raggiunge il chiamante: senza righe prodotte, `found=0` restituisce successo. Anche il selftest alle righe 70–74 ignora `status` e accetta un errore di `scan` con stdout vuoto — verificare esplicitamente gli esiti del parser e di entrambi gli scan.

[BLOCCANTE] tests/trace/M2.tsv:29 — Restano senza test NFR-SEC-02, NFR-LIC-01.c, ADR-02, D-03, D-04 e D-09.a, rispettivamente alle righe 29, 31, 37, 51, 52 e 54, benché verbale e matrice indichino test esistenti; `tmp/trace-check.txt` li segnala effettivamente mancanti e propaga il difetto ai dipendenti — completare queste associazioni senza anticipare la verifica di C-02.a e ADR-07.a.

[MED] tools/check_noexec.sh:4 — La regex non riconosce `fexecve`, che esegue un programma tramite descrittore, e il negativo a riga 36 non la esercita — includere l’API e una prova negativa discriminante.

[MED] tools/check_core_headless.sh:30 — Un errore di `grep`, per esempio su `CMakePresets.json` assente o illeggibile, viene trattato come assenza di riferimenti e può produrre successo. La stessa classe compare alle righe 14, 20, 33, 54 e in tools/check_noexec.sh:24 — distinguere assenza di match da errore dello strumento e propagare diagnosi e fallimento.

[MED] tools/review.sh:41 — Il parser accetta un verdetto trovato ovunque nell’artefatto, anche seguito da rilievi BLOCCANTI o altro testo — richiedere un unico verdetto conclusivo e rifiutare PASS contraddetti da rilievi BLOCCANTI.

[MED] tools/median_variance_table.py:45 — `main()` non ha gestione delle eccezioni: errori di scrittura dell’uscita, inclusi disco pieno e pipe chiusa, terminano con eccezione non gestita, contro il vincolo operativo sui report di crash — aggiungere uscita esplicita con diagnosi conservando il dettaglio dell’errore.

[LOW] docs/contracts/ADR-07-cpu-simd.md:72 — Il controllo previsto delle istruzioni fuse omette `vfnmsub*`; stessa omissione nell’elenco a riga 14 — includere questa famiglia in T-M2R5-05.

[LOW] docs/evidence/M2-R0.md:32 — Il record conserva `δ_est=0.05`, mentre contratto e verbale adottano `δ_med=0.0483` — aggiornare nome e valore della tolleranza corrente.

Verdetto: FAIL