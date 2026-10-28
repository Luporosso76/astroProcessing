# M1.e: review indipendente della preparazione batch003

Esito: due rilievi cleanup chiusi staticamente; un rilievo sidecar ancora aperto.
Nessun test, candidato o fase scientifica eseguito da questa review.

Revisore: agente security, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello
autodichiarato `openai/gpt-6-astra`; costi e token non esposti.
Verbale persistito dal coordinatore, non scritto dal revisore.

Verificati diff effettivo di 700 righe (+466/-31), dieci fonti e dodici copie
snapshot per hash/mode. Letti report152, diff700, ricevuta147, snapshot80,
supervisor60-362 e controller530-638; altri delta e test tramite diff conforme.
Baseline PREP004, otto sorgenti, engine, tre shader, indice e protocolli conformi.
GT e lock non riletti. Nessun import, processo candidato o modifica del revisore.

- Escalation OWN prosegue dopo errore di censimento soltanto con identità valida.
- Sessioni irrisolte bloccano nuovi lanci anche dopo il reap del leader.
- Close indipendenti e verifica prima dell'unica pubblicazione finale presenti.
- Resta MED in `controller.py:537-550`: `os.walk(followlinks=False)` non visita
  un symlink verso directory ammessa presente in `dirs`. Il nome può passare
  senza verifica dell'entry figlia, contro il contratto sidecar dichiarato.

Richiesta verifica dirfd/NOFOLLOW di ogni directory figlia e regressione dedicata.
Le cinquanta regressioni predisposte restano NONRUN; la correzione è assegnata
come nuova preparazione prima del primo turno mock. Non è ammesso il batch66.
La riserva storica286240768 B non rientrava nel cap globale con lo storage003;
nessuna cancellazione, aumento del cap o riduzione delle tuple viene autorizzata.
