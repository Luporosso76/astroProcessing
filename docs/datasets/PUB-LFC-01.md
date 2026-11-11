# PUB-LFC-01: Palomar LFC, chip 0 (Astropy CCD Guide, esempio 1)

- Stato: ACQUISITO. Licenza SPDX: `CC-BY-4.0`.
- Fonte: Zenodo, DOI 10.5281/zenodo.3254683, https://zenodo.org/records/3254683, file `example-cryo-LFC.tar.bz2` (161627346 byte, MD5 pubblicato `6ea21afd854eac1da770daa1a548cf6e`, verificato).
- Licenza: Creative Commons Attribution 4.0 International, dichiarata nella pagina del record. Ridistribuibile con attribuzione. Attribuzione: Matt Craig et al., "CCD Data Reduction Guide", dati di esempio LFC.
- Copia locale: `data/public/zenodo-3254683/` (esclusa da git). Estrazione senza file `._*` e `.DS_Store`, senza owner né permessi dell'archivio.
- Sensore: CCD mono raffreddato criogenicamente, BITPIX 16, 2080×4128 con regione di overscan.

| Ruolo | File | EXPTIME s | Filtro |
|---|---|---|---|
| light | `ccd.037.0.fits` | 300,062 | g |
| light | `ccd.043.0.fits` | 300,014 | i |
| dark per light | `darks/ccd.013-015.0.fits` | 300 | r (header) |
| dark per flat g (ruolo `dark_flat` dichiarato) | `darks/ccd.017-019.0.fits` | 70 | r (header) |
| dark per flat i (ruolo `dark_flat` dichiarato) | `darks/ccd.023-025.0.fits` | 7 | r (header) |
| flat g | `ccd.014-016.0.fits` | 70 | g |
| flat i | `ccd.017-019.0.fits` | 7 | i |
| bias | `ccd.001-006.0.fits`, `darks/ccd.002.0.fits` | 0 | i, r |

Limiti: un solo light per filtro; nessun dato CFA; il filtro nell'header dei dark non è significativo (otturatore chiuso). Hash SHA-256 di archivio e file:

```
373cfadcbfa4172516c146c8d07b0485e77f73a7af02712d71fc775e33fce659  example-cryo-LFC.tar.bz2
be73dedce9727aa708a5f9427328b6711d697e9d7935053bbd6a1bbd093bf8b9  raw/example-cryo-LFC/ccd.001.0.fits
f00343b4bf78bbf84361720e0ffeeda7d936cc025408949e043dc601b60de0e1  raw/example-cryo-LFC/ccd.002.0.fits
a315482f22198521752ac62f11c29ae4032c804d48f7872255201187e5f6c18b  raw/example-cryo-LFC/ccd.003.0.fits
9c2bb15116593db1936ae6d33b6a7511ccbcadd1d0f89403b46ef961fe008d0e  raw/example-cryo-LFC/ccd.004.0.fits
52a02cbca2fdd6cb8159f6525c4e453492fb41e9af30c5a845f3620efa9cea45  raw/example-cryo-LFC/ccd.005.0.fits
1112e59f153ac5e69648983310e72ef9ca969e1aa36510e286ecdf69d3f70532  raw/example-cryo-LFC/ccd.006.0.fits
b88697d8343b0910a23e108a8fcf7cbf63ad3872fcc0edee4fe12447ad4ddc07  raw/example-cryo-LFC/ccd.014.0.fits
74b6b0cb4d39e7ec6722bb78baa988590eae5876adb711a10f6f44fe5b7d73c8  raw/example-cryo-LFC/ccd.015.0.fits
67ed4f32553413c6835a07ee2bf40cd1ee6cf169285f0b4ac2373c41879ee20b  raw/example-cryo-LFC/ccd.016.0.fits
8f730c5ac91708a9fd5ba23ea7e7fb55aef769d54057f884b8fe826cdc798aae  raw/example-cryo-LFC/ccd.017.0.fits
c308de11075112f3fbc705a00758bff74ae2a6e055e07637219f35aa5e7eadc6  raw/example-cryo-LFC/ccd.018.0.fits
db6662c74f97490dcbb552145f759372272497f76ea99e88f0860032484c17e8  raw/example-cryo-LFC/ccd.019.0.fits
5d0e92f0c6a5749e2a264129d1821d29f8941b09adb36ce11a8e7cf5067296c3  raw/example-cryo-LFC/ccd.037.0.fits
d0322c84eeecb7f35485d0ef36b0cb87f2c2db5423bcec67f44f6d43048a41ab  raw/example-cryo-LFC/ccd.043.0.fits
745948e93f5f4ba37f01d7e0182a861bde0012b457c4312e71946718bd182216  raw/example-cryo-LFC/darks/ccd.002.0.fits
f62a979edeab66206be26247951e6711f3e65fd73469a72a858025375255e198  raw/example-cryo-LFC/darks/ccd.013.0.fits
0817828607ce8bd435ce3a1e66b4f69cb0394c3d3afb7a1bf341188631cb4782  raw/example-cryo-LFC/darks/ccd.014.0.fits
e8cf7d754819d91ddb7f66856ad4faa09498d27d50ba1ceab0fc292e328a70d4  raw/example-cryo-LFC/darks/ccd.015.0.fits
6a50cd57544b9fd2f5f3d5163724789af306874afe85a9e10edaf75bdebb7f76  raw/example-cryo-LFC/darks/ccd.017.0.fits
fc9f1bf817a62d89446743880165340c8cd1624b83c5c7540bb40b5d2d56d364  raw/example-cryo-LFC/darks/ccd.018.0.fits
8f25b570f4f59fdd48f5a3bdf57dd7904801093e5beb77c8f30924858facd42b  raw/example-cryo-LFC/darks/ccd.019.0.fits
cc6dfea9a17641065910bf6b4288e2d31fc3dc36e57b71c37c65d659ade45bb2  raw/example-cryo-LFC/darks/ccd.023.0.fits
7ce339d1aa9fce9409bb9fb2b2e8277946dcb7a4d72e7fdbc4ae93ff5e0def46  raw/example-cryo-LFC/darks/ccd.024.0.fits
a253adca4a93ef09dd6b92a970f6881bca847f44b97ad21415cd1a39d64a84c7  raw/example-cryo-LFC/darks/ccd.025.0.fits
```

## Parametri d'uso in M3.b (T-M3B-C02-16, T-M3B-FLAT-09)

Decisione: `docs/decisions/M3b-lacune.md`, «Decisioni successive (B17)». I valori sono quelli già usati sullo stesso dato da T-M2E-13 e T-M2R2-09 (`tests/unit/test_calibration_real.cpp`); il test `tests/unit/test_master_b17.cpp` li cita.

| Parametro | Valore |
|---|---|
| `exposure_rel_tol` | 1e-3 |
| `temperature_abs_tol_c` / `temp_policy` | 1.0 / `not_applicable_declared` |
| `saturation_adu` / `saturated_fraction_max` | 65535 / 0.05 |
| `flat_min_signal_adu` | 1000 |
| `flat_level_rel_tol` (solo `none`) | 0.2 |
| `offset_adu` dichiarato | 1137, mediana del livello di `ccd.001` nelle colonne 0…2047 |
| Campi dichiarati | binning 1×1, ADU, lineare, fase raw |

`A` (`flat_additive_bound_adu`) per banda: limite per pixel della componente additiva non sottratta nei flat (ipotesi `additive_bounded` di C-02.b-flat §f.1, |β(p)| ≤ A per ogni pixel p). Algoritmo: d(p) = |media dei tre dark-flat della stessa esposizione in p − offset dichiarato|, sui pixel della regione dati (colonne 0…2047, tutte le righe) in cui nessuno dei tre campioni è saturo (≥ 65535). `A` = max_p d(p), valore esatto (razionale: la media di tre interi). È una stima conservativa da 3 frame: il massimo osservato su quei frame, non un limite dimostrato per ogni altro frame. Informazione, non limite: quantile 99,9 % (rango più vicino: il ⌈0,999 N⌉-esimo valore ordinato) e frazione dei pixel con d oltre il quantile.

Comando: `python3 -S tools/datasets/pub_lfc_01_additive_bound.py` dalla radice del repository (lettura dei file FITS allineata alle card da 80 byte, dimensioni da NAXIS1/NAXIS2, BZERO e BSCALE letti e controllati; circa 110 s).

| Banda | Dark-flat | Pixel | `A` (ADU) | Quantile 99,9 % (ADU) | Frazione oltre il quantile |
|---|---|---|---|---|---|
| g | `darks/ccd.017–019` (70 s) | 8 454 143 | 6956/3 = 2318,666667 | 625 | 1,000e-3 (8454 pixel) |
| i | `darks/ccd.023–025` (7 s) | 8 454 143 | 8390/3 = 2796,666667 | 284 | 9,712e-4 (8211 pixel) |

Offset misurato dallo stesso comando: 1137. Il quantile è circa un quarto del massimo in g e un decimo in i: il massimo è determinato dalla coda dei pixel con la componente più alta (si ipotizzano pixel caldi, non verificato qui). Su 8 454 144 pixel della regione, uno è saturo in almeno un dark-flat in ciascuna banda ed è escluso dal calcolo di `A`; nei master del flat il pixel saturo è invalido (`saturated`), come asserisce il test.
