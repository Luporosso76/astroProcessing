# M1.a: review statica della preparazione pocketfft

Esito: NON READY per il primo BUILD; due rilievi assegnati come nuova preparazione.
Revisore tests, sessione `ses_e8c3e6e74ffeorhNv9IT7nj9CR`, modello autodichiarato
`openai/gpt-6.1-sol`. Verbale persistito dal coordinatore; costi non esposti.

Letti integralmente report148, piano205, licenze94, adapter93, worker126,
supervisor397, manifest129 e receipt34/54, più213 righe mirate dell'header.
Verificati pin, AST e aritmetica minima; nessun import candidato, compilazione,
fixture, FFT, test o benchmark eseguito dal revisore.

- ALTO nella versione iniziale `supervisor.py:99,119,129-135`: poll/wait possono
  raccogliere il leader prima del killpg finale incondizionato. L'identità del
  gruppo può essere persa; serve WNOWAIT, segnali OWN prima del reap e cleanup
  indipendenti anche quando una chiusura fallisce.
- MED `supervisor.py:43-48,336-343` e `worker.cpp:90-94`: mancano fsync delle
  directory prima della rimozione degli intermedi dichiarati durevoli.

Senza rilievi statici su oggetti std::complex reali, overflow/stride/dtype,
controllo dei soli elementi attivi, segni e fattore inverse, oracle indipendente
su tutte le componenti/bin, macro seriali e flag. Matrice232 casi ancora NONRUN:
192 finiti,14 errori base,24 non finiti,2 collisioni. ZERO non viene allentato.
Massimo dei due buffer espliciti34111488 B; allocazioni interne non dichiarate zero.
L'alias interno upstream detail::cmplx resta limite del profilo GCC dichiarato
con -fno-strict-aliasing, non certificazione di assenza di UB ISO o portabilità.
Nessuna adozione della dipendenza, qualifica Rust/FFI o scelta dello stack.
