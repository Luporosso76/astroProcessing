# Verbale M3-dev-preset: ottimizzazione del preset `dev` ed esecuzione seriale dei test pesanti

Stato: **DECISO dal coordinatore** l'8 novembre 2026 (decisione tecnica delegata, `AGENTS.md`). Motivo: debito del gate M3.a (`docs/evidence/M3-a.md`, «Arbitraggio 8/11/2026»).

| Campo | Contenuto |
|---|---|
| Domanda | Come far stare `T-M2R2-09.real_palomar_product_masters` nel suo timeout di 1800 s nel preset `dev-full` senza alzare il timeout né indebolire il test |
| Fatto misurato | Preset `dev` (`Debug`, `-O0`, ASan+UBSan): 1620-1670 s nei run completi, 1659,84 s da solo. L'esecuzione seriale da sola non basta: il test è vicino al limite anche senza carico |
| Decisione | 1) Preset `dev` con `-O1 -g`, sanitizer invariati (`-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -DASTRO_SANITIZED=1`): è il livello raccomandato dalla documentazione di AddressSanitizer per tempi ragionevoli. 2) `RUN_SERIAL TRUE` sui test `slow` sopra 600 s: `T-M2R2-09`, `T-M2E-13`, `T-M3A-RAW-10.fuzz_real_0`…`fuzz_real_6` (`tests/unit/CMakeLists.txt`) |
| Effetto misurato | `T-M2R2-09`: 603,31 s (da 1659,84 s), macchina carica, build `dev` del checkout principale. `T-M2E-13`: 567,7 s (da 1790,9 s). Monte Carlo `T-M2R2-04`, `T-M2E-21`, `T-M2E-21-26`: da 2,5 a 3 volte più veloci. Nessun errore nuovo di ASan o UBSan sui 1190 test `fast` |
| Costo | Con `-O1` GCC 15 segnala `-Wmaybe-uninitialized` dentro `<regex>` di libstdc++ (`bits/std_function.h:407`, `regex_automaton.h:282`), raggiunto da `src/core/resources.cpp` e `tests/unit/test_build_info.cpp`. Su quei due file l'avviso resta visibile ma non è un errore (`-Wno-error=maybe-uninitialized`); gli altri file restano con `-Werror` pieno. Il debugger vede alcune variabili ottimizzate |
| Alternative scartate | **Timeout più alto**: vietato (`CLAUDE.md` §3.2). **Solo `RUN_SERIAL`**: il test sfiora il limite anche da solo. **Escludere il test dal preset con sanitizer**: toglie la copertura ASan/UBSan proprio al percorso dei dati reali. **Ridurre i dati del test**: cambia cosa il test dimostra |
| Conferma | Primo run `dev-full` completo dopo la decisione (9/11/2026): 1221/1221, nessun test non passato; `T-M2R2-09` 484,85 s, `T-M2E-13` 464,38 s (`docs/evidence/M3-a.md`, «Arbitraggio 8/11/2026») |
