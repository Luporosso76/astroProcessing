"""Componente additiva dei flat di PUB-LFC-01 per banda (docs/datasets/PUB-LFC-01.md, «Parametri d'uso in M3.b»).

Uso: python3 -S tools/datasets/pub_lfc_01_additive_bound.py [radice del dato]

Prerequisiti: i file letti hanno lo SHA-256 del record, BITPIX 16, NAXIS 2, BSCALE 1, BZERO 32768 e larghezza 2080
(regione dati DATASEC = colonne 1:2048); altrimenti esce con `PREREQUISITO MANCANTE: PUB-LFC-01 <percorso>`.
Offset dichiarato: mediana dei campioni di ccd.001 nella regione dati (colonne 0..2047, tutte le righe).
Per ogni banda: d(p) = |media dei tre dark-flat della stessa esposizione in p - offset| sui pixel della regione dati
in cui nessuno dei tre campioni è saturo (>= 65535). A = max_p d(p), valore esatto; informazione: quantile 99,9 %
(rango più vicino, ceil(0.999 N)-esimo valore ordinato) e frazione dei pixel con d > quantile.
Aritmetica esatta con `fractions.Fraction`: la media di tre interi è un razionale; la stampa dà il razionale e il
valore decimale.
"""
import array
import hashlib
import sys
from fractions import Fraction

ROOT_DEFAULT = "data/public/zenodo-3254683/raw/example-cryo-LFC/"
DATA_COLS = 2048
FULL_WIDTH = 2080
SHA256 = {
    "ccd.001.0.fits": "be73dedce9727aa708a5f9427328b6711d697e9d7935053bbd6a1bbd093bf8b9",
    "darks/ccd.017.0.fits": "6a50cd57544b9fd2f5f3d5163724789af306874afe85a9e10edaf75bdebb7f76",
    "darks/ccd.018.0.fits": "fc9f1bf817a62d89446743880165340c8cd1624b83c5c7540bb40b5d2d56d364",
    "darks/ccd.019.0.fits": "8f25b570f4f59fdd48f5a3bdf57dd7904801093e5beb77c8f30924858facd42b",
    "darks/ccd.023.0.fits": "cc6dfea9a17641065910bf6b4288e2d31fc3dc36e57b71c37c65d659ade45bb2",
    "darks/ccd.024.0.fits": "7ce339d1aa9fce9409bb9fb2b2e8277946dcb7a4d72e7fdbc4ae93ff5e0def46",
    "darks/ccd.025.0.fits": "a253adca4a93ef09dd6b92a970f6881bca847f44b97ad21415cd1a39d64a84c7",
}
SATURATION = 65535
BANDS = (
    ("g", ("darks/ccd.017.0.fits", "darks/ccd.018.0.fits", "darks/ccd.019.0.fits")),
    ("i", ("darks/ccd.023.0.fits", "darks/ccd.024.0.fits", "darks/ccd.025.0.fits")),
)


def card_value(cards, key):
    for card in cards:
        if card[:8].strip() == key and card[8:10] == "= ":
            return card[10:].split("/")[0].strip()
    sys.exit("card assente: " + key)


def missing(path, why):
    sys.exit("PREREQUISITO MANCANTE: PUB-LFC-01 " + path + " (" + why + ")")


def load(root, rel):
    path = root + rel
    try:
        with open(path, "rb") as f:
            blob = f.read()
    except OSError as e:
        missing(path, str(e))
    if hashlib.sha256(blob).hexdigest() != SHA256[rel]:
        missing(path, "SHA-256 diverso da quello del record")
    cards = []
    pos = 0
    while True:
        if pos + 80 > len(blob):
            sys.exit("header senza END: " + path)
        card = blob[pos:pos + 80].decode("ascii")
        pos += 80
        if card[:8].strip() == "END":
            break
        cards.append(card)
    header_bytes = ((pos + 2879) // 2880) * 2880
    try:
        bitpix, naxis = int(card_value(cards, "BITPIX")), int(card_value(cards, "NAXIS"))
        w, h = int(card_value(cards, "NAXIS1")), int(card_value(cards, "NAXIS2"))
        bzero, bscale = float(card_value(cards, "BZERO")), float(card_value(cards, "BSCALE"))
    except ValueError as e:
        sys.exit("card dell'header non numerica in " + path + ": " + str(e))
    if bitpix != 16 or naxis != 2:
        sys.exit("formato inatteso (BITPIX 16 e NAXIS 2 richiesti): " + path)
    if bscale != 1.0 or bzero != 32768.0:
        sys.exit("BSCALE/BZERO inattesi in " + path + ": " + str(bscale) + ", " + str(bzero))
    if w != FULL_WIDTH:
        sys.exit("larghezza " + str(w) + " diversa da " + str(FULL_WIDTH) + " (regione dati non definita): " + path)
    if len(blob) < header_bytes + 2 * w * h:
        sys.exit("file più corto dei dati dichiarati da NAXIS1/NAXIS2: " + path)
    raw = array.array("h")
    raw.frombytes(blob[header_bytes:header_bytes + 2 * w * h])
    if sys.byteorder == "little":
        raw.byteswap()
    return w, h, [v + 32768 for v in raw]


def region(w, h, v):
    return [v[y * w + x] for y in range(h) for x in range(DATA_COLS)]


def median(v):
    s = sorted(v)
    n = len(s)
    return Fraction(s[n // 2]) if n % 2 else Fraction(s[n // 2 - 1] + s[n // 2], 2)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else ROOT_DEFAULT
    if not root.endswith("/"):
        root += "/"
    w, h, b = load(root, "ccd.001.0.fits")
    offset = median(region(w, h, b))
    print("offset", offset)
    for band, rels in BANDS:
        frames = []
        for rel in rels:
            fw, fh, v = load(root, rel)
            if (fw, fh) != (w, h):
                sys.exit("dimensioni diverse da ccd.001: " + rel)
            frames.append(region(fw, fh, v))
        d = []
        for a, b2, c in zip(*frames):
            if a >= SATURATION or b2 >= SATURATION or c >= SATURATION:
                continue
            d.append(abs(Fraction(a + b2 + c, 3) - offset))
        if not d:
            sys.exit("nessun pixel non saturo, banda " + band)
        d.sort()
        a_max = d[-1]
        rank = -(-999 * len(d) // 1000)
        q = d[rank - 1]
        above = sum(1 for x in d if x > q)
        print("banda %s: pixel %d, A = %s (%.6f), quantile 99.9%% = %s (%.6f), frazione sopra = %.3e (%d pixel)"
              % (band, len(d), a_max, float(a_max), q, float(q), above / len(d), above))


main()
