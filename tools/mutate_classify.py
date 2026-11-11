#!/usr/bin/env python3
"""Classifica il junit di ctest per tools/mutate.sh.

Uso: mutate_classify.py <junit> <numero-test-attesi>
Uscita: 0 almeno un test selezionato fallito per asserzione (marcatore gtest o CHECK-RESULT: defect);
        1 nessun test fallito (mutazione sopravvissuta);
        2 errore d'infrastruttura (junit illeggibile, casi diversi dagli attesi, test non eseguiti, fallimento senza
          marcatore, CHECK-RESULT: tool-error, output troncato da ctest, argomenti errati, qualunque eccezione).
Contratto con mutate.sh: la riga «junit: » stampata dopo la lettura del junit è obbligatoria; senza di essa mutate.sh
tratta le uscite 0 e 1 come avvio fallito (errore dello strumento).
Vincolo: ctest 4.2.3 scrive nel junit «[This part of the test output was removed since it exceeds the threshold of N
bytes.]» quando tronca; il rilevamento di quella stringa va riverificato se cambia la versione di ctest
(tools/mutate.sh --selftest ne prova la presenza).
"""
import sys
import xml.etree.ElementTree as ET

TRUNCATED = "removed since it exceeds the threshold"


def main(argv):
    if len(argv) != 2:
        print("INFRASTRUTTURA: uso: mutate_classify.py <junit> <numero-test-attesi>")
        return 2
    try:
        root = ET.parse(argv[0]).getroot()
    except (OSError, ET.ParseError) as e:
        print("INFRASTRUTTURA: junit illeggibile: %s" % e)
        return 2
    expected = int(argv[1])
    cases = list(root.iter("testcase"))
    notrun = [c.get("name") for c in cases if c.get("status") == "notrun"]
    failed = [c for c in cases if c.get("status") == "fail" or c.find("failure") is not None or c.find("error") is not None]
    print("junit: %d casi, %d falliti, %d non eseguiti" % (len(cases), len(failed), len(notrun)))
    if len(cases) != expected or notrun:
        print("INFRASTRUTTURA: casi %d attesi %d, non eseguiti %s" % (len(cases), expected, notrun))
        return 2
    detected = 0
    for c in failed:
        out = "".join((c.findtext("system-out") or "", c.findtext("system-err") or ""))
        if TRUNCATED in out:
            print("INFRASTRUTTURA: %s ha l'output troncato da ctest: il marcatore non è affidabile" % c.get("name"))
            return 2
        if "CHECK-RESULT: tool-error" in out:
            print("INFRASTRUTTURA: %s ha segnalato un errore dello strumento (CHECK-RESULT: tool-error)" % c.get("name"))
            return 2
        if "CHECK-RESULT: defect" in out or "[  FAILED  ] " in out:
            detected += 1
        else:
            print("INFRASTRUTTURA: %s fallito senza marcatore CHECK-RESULT né fallimento gtest" % c.get("name"))
            return 2
    return 0 if detected else 1


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Exception as exc:
        print("INFRASTRUTTURA: %s: %s" % (type(exc).__name__, exc))
        sys.exit(2)
