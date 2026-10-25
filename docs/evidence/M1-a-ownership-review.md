# Review indipendente del ponte ownership M1.a

Data: 25 ottobre 2026. Revisore: agente security, sessione `ses_e8c4fd1faffex3f7sPIaEJ0Tce`, modello autodichiarato `openai/gpt-6-astra`. Il coordinatore registra il risultato; nessuna attestazione del provider.

## Esito e copertura

PASS limitato al ponte C ABI single-thread e alle prove registrate in `M1-a-ownership.md`. Non chiude M1.a complessiva, non certifica maschere M3 o sicurezza contro chiamanti C arbitrari. Nessuna riesecuzione, importazione, compilazione o scrittura da parte del revisore.

Letti integralmente: `owner.rs` 450 righe, `consumer.cpp` 24, `bridge.h` 48, `test_bridge.py` 302, `run_all.py` 114, `verify.py` 64, `env.sh` 8, `CONTRACT.md` 37, report 83, handoff 15, eventi 75 e summary 164. Esaminati manifest e log dei tre tentativi.

Il revisore ha ricontrollato i sette sorgenti correnti, i sette membri di ciascuno dei tre snapshot ZIP, i due binari 003 e il legame fra report e handoff. Non ha ricalcolato i dodici hash esterni congelati, esclusi dal suo incarico.

## Riscontri

- Il tentativo 001 conserva l'errore const e il build C++ con uscita 1. Il tentativo 003 registra build e validazione con uscita 0 e 75 eventi coerenti con il summary.
- Tre owner e tre lease creati e chiusi; sei payload, 432 byte creati/liberati, nessuno attivo alla fine. I contatori provengono dai percorsi reali di create, destroy, lease e kernel.
- Le basi ctypes trattengono guard e owner attraverso le slice. Destroy con lease attivo restituisce BUSY; generation e lease escludono metadati scaduti nei percorsi conformi.
- Le verifiche complete precedono le scritture, compresi gli errori tardivi. Esclusi e padding restano invariati; nessuna conversione implicita di tipo, endian o maschera.
- Indirizzi, basi non owning, `shares_memory` e mutazioni causalmente visibili sostengono lo zero-copy del ponte. Non dimostrano zero allocazioni di runtime. Il ledger distingue 2976 byte di copie esplicite del test dalla fixture errata di 15 byte.

## Errata descrittiva del contratto congelato

La frase «revision e layout sono verificati a ogni chiamata nativa» in `tmp/m1/ownership/CONTRACT.md:12` è troppo ampia. `ap_end_lease` verifica owner, generation e lease ID, non revision e layout. Non va descritto come validatore completo della view. Il binding conforme chiude il lease dopo la scomparsa dell'ultima base.

Questa rettifica documenta la copertura effettiva; il contratto e i manifest storici non vengono riscritti. Il PASS non attribuisce alla chiusura del lease verifiche assenti. Nessuna prova fallita viene rimossa o dichiarata superata mediante questa annotazione.

## Limiti

Accessi concorrenti esclusi; sanitizer/Miri, abuso di raw pointer e sicurezza di alias creati fuori dal binding non verificati. Gli zeri dei contatori alternativi non attestano tutti i dispatch interni alle librerie. Nessuna misura comparativa del ponte è stata eseguita.
