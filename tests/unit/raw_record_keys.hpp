// Chiavi del record del decoder (ADR-08.b §2.9) nei test di U4. Il contratto fissa i valori e i nomi segnati «contratto»;
// gli altri nomi sono la scelta dell'autore dei test, da riportare nel codice. Il record sta in `details.decoder`
// della provenance del job riuscito. Regola: il job decodifica nella preparazione (lettura verificata, decodifica, record)
// e costruisce il nodo con `make_raw_import_node(digest, record)`.
#pragma once

#include <array>
#include <string_view>

namespace raw_record {

// Riga «Libreria» (§2.9). Valori per LibRaw 0.22.3 (U12-third-party.md §1, §2, obblighi 5 e 6).
constexpr std::string_view libraw_version = "libraw_version";                  // LibRaw::version()
constexpr std::string_view libraw_version_number = "libraw_version_number";    // LibRaw::versionNumber()
constexpr std::string_view libraw_capabilities = "libraw_capabilities";        // libraw_capabilities()
constexpr std::string_view libraw_tarball_sha256 = "libraw_tarball_sha256";    // SHA-256 del tarball di third_party/libraw
// Righe «Offset o nero applicato» e «Scala applicata».
constexpr std::string_view black_applied = "black_applied_by_decoder";         // contratto
constexpr std::string_view scale_applied = "scale_applied_by_decoder";
constexpr std::string_view black = "black";            // {scalar, per_color[4], pattern_rows, pattern_cols, pattern[]}
constexpr std::string_view white = "white";
constexpr std::string_view data_maximum = "data_maximum";  // intero o null
constexpr std::string_view linear_max = "linear_max";      // array di 4 interi o null
constexpr std::string_view linearization = "linearization";                    // contratto: identity | decoder_curve
constexpr std::string_view curve_sha256 = "linearization_curve_sha256";        // esadecimale per decoder_curve, altrimenti null
// Riga «Crop e geometria CFA».
constexpr std::string_view crop = "crop";                                      // contratto: {x0, y0, w, h}
constexpr std::string_view margin_parity = "margin_parity";                    // [left_margin mod 2, top_margin mod 2]
constexpr std::string_view cfa_pattern = "cfa_pattern";                        // rggb | bggr | grbg | gbrg
constexpr std::string_view raw_inset_crops = "raw_inset_crops";
constexpr std::string_view flip = "flip";
// Righe «Metadati conservati o eliminati», «Stato del dato», «Avvisi».
constexpr std::string_view metadata_present = "metadata_present";              // nomi dei campi interpretati e presenti
constexpr std::string_view metadata_not_kept = "metadata_not_kept";            // categorie non conservate (§2.6)
constexpr std::string_view data_state = "data_state";                          // "raw"
constexpr std::string_view camera_processing = "camera_processing";            // contratto: "unknown"
constexpr std::string_view process_warnings = "process_warnings";
// Riga «Metadati assenti di F-IO-04» (L4), nomi del contratto.
constexpr std::string_view sensor = "sensor";
constexpr std::string_view pixel_size_um = "pixel_size_um";
constexpr std::string_view pixel_scale_arcsec = "pixel_scale_arcsec";
constexpr std::string_view wcs = "wcs";
constexpr std::string_view absent_reason = "absent_reason";                    // "not_provided_by_decoder"
// Riga «Ora di acquisizione», nomi del contratto.
constexpr std::string_view capture_time_libraw = "capture_time_libraw";        // intero o null
constexpr std::string_view capture_time_clock = "capture_time_clock";          // "camera_no_zone"
constexpr std::string_view process_tz = "process_tz";                          // "UTC0"

constexpr std::array<std::string_view, 30> all_keys = {
    libraw_version, libraw_version_number, libraw_capabilities, libraw_tarball_sha256, black_applied, scale_applied,
    black, white, data_maximum, linear_max, linearization, curve_sha256, crop, margin_parity, cfa_pattern,
    raw_inset_crops, flip, metadata_present, metadata_not_kept, data_state, camera_processing, process_warnings, sensor,
    pixel_size_um, pixel_scale_arcsec, wcs, absent_reason, capture_time_libraw, capture_time_clock, process_tz};

// U13 §8 (docs/contracts/U13-dng-decoder.md): decoder scelto dall'instradamento e campi dell'IFD raw scelto. Stessi nomi
// nel record di una decodifica riuscita e nel contesto dello stop del decoder del progetto (provenance dell'arresto);
// i valori numerici nel contesto sono interi decimali.
constexpr std::string_view decoder = "decoder";                  // contratto: libraw | astro_dng
constexpr std::string_view route_reason = "route_reason";        // per libraw: not_tiff | tiff_not_dng | dng_libraw_compression
constexpr std::string_view compression = "compression";          // Compression (259) dell'IFD raw scelto
constexpr std::string_view sample_format = "sample_format";      // SampleFormat (339), predefinito 1
constexpr std::string_view bits_per_sample = "bits_per_sample";  // BitsPerSample (258)
constexpr std::string_view decoder_libraw = "libraw";
constexpr std::string_view decoder_astro_dng = "astro_dng";

// Valori fissati dal contratto o da U12-third-party.md §1 (LibRaw 0.22.3, build del progetto).
constexpr std::string_view expected_libraw_version = "0.22.3-Release";
constexpr long expected_libraw_version_number = 0x001603;
constexpr long expected_libraw_capabilities = 0xc0;
constexpr std::string_view expected_libraw_tarball_sha256 = "789f3c33466210a1a0907f747a97c715d21bbf4af712f257fcbbd90111b227e4";

}  // namespace raw_record
