"""T-U12-05 (ordine, CFITSIO): third_party::verify_once() precede ogni ingresso in CFITSIO.

Prova STATICA: nel prodotto non esiste un punto osservabile tra la verifica e fits_open_memfile / fits_create_memfile (nessun gancio,
contatore o iniettore in quel tratto, e lo scanner interno rifiuta ogni input che CFITSIO rifiuterebbe all'apertura), quindi il
comportamento non distingue l'ordine. Il test legge src/core/fits.cpp: per ciascuna chiamata di apertura/creazione, l'ultima
istruzione che la precede nella stessa funzione e' verify_once. Per LibRaw l'ordine e' provato dal comportamento
(test_third_party_verify, U12VerifiedInjected.DecodeVerifiesBeforeEnteringOpenBuffer).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from u12_common import ROOT, fail, read_text  # noqa: E402

CALLS = ("fits_open_memfile", "fits_create_memfile")
text = read_text(os.path.join(ROOT, "src", "core", "fits.cpp"))
for call in CALLS:
    sites = [m.start() for m in re.finditer(r"\b%s\s*\(" % call, text)]
    if len(sites) != 1:
        fail("%s compare %d volte in fits.cpp, atteso 1" % (call, len(sites)))
    prefix = text[:sites[0]]
    statements = [s.strip() for s in re.split(r"[;{}]", prefix)]
    statements = [s for s in statements if s and not s.startswith("//")]
    # L'ultima istruzione prima della chiamata e' `int st = 0`; quella prima ancora e' la verifica.
    tail = [s for s in statements[-3:] if "int st = 0" not in s]
    if not tail or "third_party::verify_once()" not in tail[-1]:
        print("CHECK-RESULT: defect")
        fail("%s non e' preceduta da third_party::verify_once() (ultime istruzioni: %r)" % (call, statements[-3:]))
sys.exit(0)
