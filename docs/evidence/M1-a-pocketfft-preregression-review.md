# Pocketfft: review delle venti preregressioni predisposte

2026-10-25. NON READY per il primo turno; BUILD ancora bloccata.

Revisore indipendente `ses_e8c14fabaffeCEsreEF1hHtERa`, modello autodichiarato `openai/gpt-6.1-sol`; coordinatore persiste il verbale. Nove file letti integralmente,2558 righe; cinque sorgenti, otto snapshot hash/mode e pin upstream/header/LICENSE verificati. Due Python analizzati con AST,20 metodi predisposti, zero eseguiti. Header solo hashato, non audit semantico completo. Costi/token non esposti.

MED concreto: `regressions.py:24-27,152-157`, FakePopen espone `.fileno()` come Mock, ma `os.set_blocking` reale nel supervisore pretende un intero. Il test positivo provocherebbe TypeError. Inoltre la diagnostica può scrivere HERE/child-stop.json anziché nel case fresco. Correzione assegnata: FD sintetici interi, spy set_blocking con argomenti verificati e diagnostica confinata per caso; nessuna modifica al gate o ai risultati attesi.

Due rilievi originari coerenti staticamente: WNOWAIT conserva l'identità fino ai segnali/reap, identità perduta vieta segnali; cleanup indipendenti/errori bloccano nuovi lanci. Barriere fsync directory prima e dopo rimozione; errori impediscono COMPLETE. Mancano ancora prove runtime di questi rami.

Sette funzioni scientifiche con AST identico;232 casi numerici ancora pianificati. Nessuna compilazione o FFT eseguita. I mock non certificano PID reali, blackout, budget256MiB o conformità ISO C++ dei lifetime. Dopo fix/review, prima ammissione limitata alle preregressioni, non GCC/worker/FFT.
