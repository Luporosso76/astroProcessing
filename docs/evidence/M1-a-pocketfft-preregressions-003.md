# M1.a pocketfft: 36 preregressioni mock eseguite

## Esito e confini

PASS_MOCK_ONLY: 36 metodi distinti, 20 originali e 16 nuovi, tutti PASS.
Zero failure, errori unittest, skip, expectedFailure o unexpectedSuccess.
Un solo lancio candidato, PID `906881`, exit `0`, raccolto con waitid.
Nessun figlio candidato pendente; slot libero dopo cleanup senza errori.

Il turno ha eseguito soltanto il runner stdlib ammesso. Nessun compilatore,
worker FFT, caso scientifico, GUI o benchmark. Il risultato non qualifica i 232
casi, non ammette una build e non sostituisce la review delle preregressioni.
Nessun sorgente, manifest o piano del candidato modificato.

## Provenienza e autenticazione

Il coordinatore ha ammesso un turno distinto dopo i due STOP_NOT_RUN del
verificatore. Restano invariati il report
`docs/evidence/M1-a-pocketfft-preregressions-003-STOP_NOT_RUN.md` e il raw
`tmp/m1/fft-pocket/authentication-stop-003.json`. Il primo stop usava il layout
snapshot errato; il secondo trascriveva male una costante inline. Nessuno dei
due aveva avviato il candidato. Questo turno corregge soltanto il verificatore
esterno; il suo pin manifest è definito una volta, validato come 64 cifre hex e
riusato nell'autenticazione, alla frontiera di lancio e nella verifica finale.

Prima di importare il candidato: 97 percorsi autenticati con byte/SHA-256,
size e stat prima/dopo lettura. Aperture relative a directory descriptor con
O_NOFOLLOW per file e componenti directory. Verificati sorgenti correnti,
frozen, upstream, provenienza 002 e tre snapshot con 8/22/29 copie.
Mapping espliciti: `snapshot_file` per pre-followup-001, `file` per
pre-regressions-followup-001, chiavi di `files` per pre-build-budget-003.
Copie snapshot write-protected controllate. Tra i 26 file storici correnti
verificati nel terzo snapshot figurano old20 PASS e FIRSTFAIL del compilatore.

I pin ammessi e verificati sono:

```text
followup-manifest-003.json, 6720 byte
9e66b7ed388813da15d0abe7cb04ce6f704af02a64f29640ad6639523c807a75
static-build-budget-003.json
a672e61f81dee288c88a14f811d49e633c20d33ff6ac3239bcebd5fc44be25fa
```

Dopo raccolta: gli stessi 97 record byte/stat sono identici, così come i due
artefatti STOP precedenti. Raw durevoli separati:
`authentication-before.json`, `authentication-after.json`,
`preserved-stops-before.json`, `preserved-stops-after.json` nell'outer.

La review `ses_e8861dc34ffeJhiLMCzYYtZN9h`, comunicata dal coordinatore come
READY_STATIC36, riguarda il testo statico. Non costituisce attestazione SHA
indipendente né review di questi risultati. L'autenticazione byte/stat qui
descritta è una verifica separata di questo agente.

## Comando reale e raw

Directory di lavoro: `/opt/workspace/astroProcessor`. Comando candidato
eseguito una sola volta mediante Popen, senza shell, launcher o preexec:

```sh
/usr/bin/python3 -I -B -S tmp/m1/fft-pocket/regressions.py preregressions-003
```

Il driver esterno è stato eseguito nella chiamata
`/usr/bin/python3 -I -B -S -` tramite heredoc e
`exec(compile(DRIVER_SOURCE,'<preregressions-turn-003-driver>','exec'))`.
Il corpo effettivo è salvato, dopo autenticazione e preflight, in
`tmp/m1/fft-pocket/preregressions-turn-003/driver.py`. Non rilanciarlo:
i namespace sono ora occupati e le ricevute vanno conservate.

Namespace freschi verificati assenti prima della creazione:
`tmp/m1/fft-pocket/preregressions-003/` e
`tmp/m1/fft-pocket/preregressions-turn-003/`.
Le scritture raw usano creazione esclusiva e fsync file/directory; nessun
overwrite. Stdout e stderr reali sono vuoti e conservati separatamente.
Il runner raccoglie il log unittest dentro `regression-results.json`, non su
stdout. Le diagnosi mock dei casi negativi restano nelle directory dei casi.

Hash SHA-256 degli artefatti principali, relativi a `tmp/m1/fft-pocket/`:

```text
preregressions-turn-003/driver.py, 14755 byte
e3d1c4770614278570527c529304ffdcf0d5e66eabf86047044c758ea61014bd
preregressions-turn-003/execution.json, 4414 byte
b4970eebbcd9bc203775d519d727f447c61fa4ff351b5e6e93ca3fdd7e0d1b28
preregressions-turn-003/authentication-before.json, 41078 byte
9e06dc5d69a576fefdd80948ee6d104c28c4b4a63a144142729c8687d52db4ac
preregressions-turn-003/authentication-after.json, 41078 byte
9e06dc5d69a576fefdd80948ee6d104c28c4b4a63a144142729c8687d52db4ac
preregressions-003/manifest-before-regressions.json, 1847 byte
1d84b270a308d70e1e6babc3c4f79419e3dec76ad58393bad71e46d59baf1f6d
preregressions-003/regression-results.json, 4306 byte
a34fb0cc873c972ff016d215a822248c538067fd2600a5717c036455004ebe4d
preregressions-003/complete.json, 281 byte
9280801fcbc119a559ce53727d31f5390c558df45820d389f336f5c400d9d7b3
preregressions-turn-003/stdout.log e stderr.log, ciascuno 0 byte
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
```

`preregressions-turn-003/artifact-manifest.json` fissa i 15 file raw prodotti
prima della sua creazione, incluse le diagnosi dei casi. Il controllo successivo
ha letto soltanto artefatti, JSON e AST del driver: nessun rerun o import del
progetto. Ha verificato 41 directory private mode 0700 e un solo sito Popen nel
driver. Non è una traccia syscall del candidato.

## Risorse e raccolta del processo reale

Preflight tmp globale: 956928000 byte allocati su 5381 inode distinti, hardlink
deduplicati e symlink non seguiti. Quota 1073741824 byte, riserva 67108864 e
margine 16777216; residuo 32927744 byte. Spazio filesystem disponibile
125336399872 byte; MemAvailable 19420463104 byte. Sono controlli metadata e
risorse disponibili, non letture delle fixture storiche.

Root outer e HOME/cache/TMP sono directory proprie mode 0700. Ambiente passato
esplicitamente: PATH `/usr/bin:/bin`, LANG/LC_ALL `C`, TZ `UTC`,
OMP_NUM_THREADS `1`, HOME nell'outer `/home`, XDG_CACHE_HOME nell'outer `/cache`,
TMPDIR nell'outer `/tmp`, PFFT_RUN_ROOT nel namespace preregressioni.
SIGCHLD default verificato prima del lancio.

Il runner registra prima dei mock un setup reale `limits('validate')`, stato
APPLIED: AS 268435456, RSS best effort 536870912, FSIZE 67108864 e CORE 0 byte,
ciascuno con soft/hard uguali e readback. Ereditati AS/RSS/FSIZE illimitati;
CORE soft 0/hard illimitato. Questo record viene dall'interprete candidato,
non da una lettura indipendente dell'outer. I test sui budget build AS1GiB sono
mock e non attestano un compilatore reale sotto quel limite.

Outer: deadline 120 s, cleanup bounded 5 s, log massimo 65536 byte per stream.
Campione massimo VmRSS del solo leader: 31678464 byte. Non è il picco garantito
né RSS aggregato di un albero. tmp massimo campionato durante il child:
957194240 byte; dopo autenticazione post: 957243392 byte, residuo 32612352 byte
oltre riserva e margine. Le scritture di evidenza successive aggiungono metadata;
il guard non è una quota kernel atomica né protegge da writer esterni.

Il driver crea PID `906881` con start_new_session. Osserva il leader con
waitid WNOWAIT e verifica SID/PGID uguali al PID prima di ogni segnale. Dopo
uscita, invia SIGKILL al gruppo OWN mentre il leader resta riservato; il census
non trova altri membri vivi. Quindi waitid raccoglie exit 0. Nessun segnale dopo
reap, nessun errore di cleanup, pending_unreaped false. Pipe e selector chiusi,
log sincronizzati. Questa è evidenza del processo reale del runner; i PID finti
usati nei mock non costituiscono evidenza OS. Il census non certifica eventuali
discendenti ostili che cambiano sessione.

## Esiti distinti

Il confronto usa l'insieme dei nomi AST, senza dipendere dall'ordine del runner.
Tutti i seguenti metodi compaiono una volta con esito `ok` nel raw unittest.
Le eccezioni simulate e verificate con assertRaises sono casi negativi PASS,
non errori unittest né expectedFailure. Nessun sottocaso facoltativo aggiunto.

```text
PASS Cleanup.test_all_close_attempts_independent
PASS Cleanup.test_already_reaped_no_signal
PASS Cleanup.test_census_error_does_not_skip_reap_or_close
PASS Cleanup.test_exited_unreaped_own_group
PASS Cleanup.test_identity_lost_no_signal
PASS Cleanup.test_ignored_sigchld_blocks_launch
PASS Cleanup.test_live_group_timeout_blocks_next_launch
PASS Cleanup.test_no_signal_after_explicit_reap
PASS Cleanup.test_reap_failure_no_later_signal
PASS Cleanup.test_run_never_polls_or_waits_before_cleanup
PASS Cleanup.test_running_own_group
PASS Cleanup.test_session_mismatch_no_signal
PASS Cleanup.test_signal_failure_still_reaps_and_closes
PASS DirectoryBarriers.test_either_pre_erase_directory_failure_preserves_all_inputs
PASS DirectoryBarriers.test_file_sync_failure_preserves_all_inputs
PASS DirectoryBarriers.test_json_file_then_directory_sync
PASS DirectoryBarriers.test_mkdir_child_then_parent_sync
PASS DirectoryBarriers.test_post_erase_sync_failure_stops
PASS DirectoryBarriers.test_record_directory_barriers_before_erase
PASS DirectoryBarriers.test_worker_publication_order_and_omission_discriminator
PASS LimitBudgets.test_build_selection_and_unchanged_limits
PASS LimitBudgets.test_effective_readback_mismatch_stops
PASS LimitBudgets.test_equal_hard_is_allowed
PASS LimitBudgets.test_getrlimit_error_records_unknown_effective_and_stops
PASS LimitBudgets.test_invalid_command_before_resource_calls
PASS LimitBudgets.test_larger_finite_hard_is_not_silently_selected
PASS LimitBudgets.test_lower_hard_stops_without_any_lowering
PASS LimitBudgets.test_main_build_applies_limits_before_dispatch
PASS LimitBudgets.test_main_invalid_arity_before_limits
PASS LimitBudgets.test_main_invalid_command_before_limits
PASS LimitBudgets.test_main_validate_applies_limits_before_dispatch
PASS LimitBudgets.test_manifest_coherence_and_profile_rejects_budget_changes
PASS LimitBudgets.test_old_count_or_old_pin_receipt_blocks_compiler
PASS LimitBudgets.test_preflight_all_resources_before_first_set
PASS LimitBudgets.test_setrlimit_errors_stop_without_retry
PASS LimitBudgets.test_validate_selection_and_unchanged_limits
```

Limite residuo già dichiarato: nessun nuovo sottocaso getrlimit post-set fallito.
Questi mock provano i discriminatori eseguiti, non power loss, kernel cleanup in
ogni condizione, bisogno memoria GCC o correttezza numerica FFT.
Nessun nuovo test o red/green su una correzione candidata in questo turno.

## Ambiente e handoff

Python osservato: `3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]`,
eseguibile `/usr/bin/python3`. Linux `7.0.0-38-generic`, macchina `x86_64`.
Modello dichiarato dall'ambiente: GPT-6.1 Sol, `openai/gpt-6.1-sol`;
nessuna attestazione runtime separata del failover, nessun override impostato.
Costi e token non esposti. Nessuna delega, rete, Git, installazione o indice.
Progress/index invariati. Fonti lette semanticamente: piano locale 277 righe,
preparazione budget 161, manifest003 221, static003 70; sorgenti letti solo nelle
porzioni CLI/limiti/fixture necessarie, oltre alle verifiche byte/stat complete.

Handoff stabile: PASS dei soli 36 mock, PID raccolto e slot libero. Restano
necessarie review dei risultati e ammissione separata prima di qualsiasi build.
