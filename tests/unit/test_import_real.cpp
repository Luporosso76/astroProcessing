// R1: campioni pubblici licenziati (PUB-LFC-01 mono, PUB-CFA-01 CFA) importati, inventariati e riscritti bit-esatti.
// T-M2R1-09. I file non sono versionati (data/): il test fallisce se mancano o se l'hash non e' quello registrato.
#include <gtest/gtest.h>

#include "astro/hash.hpp"
#include "astro/import_job.hpp"
#include "astro/inventory.hpp"
#include "import_support.hpp"

namespace {

using namespace imp;

struct sample {
  const char* name;
  const char* path;
  const char* sha256;  // registrato in docs/datasets/PUB-*.md prima del test
  std::uint32_t width, height;
  const char* bayer;  // vuoto = mono
};

constexpr sample samples[] = {
    {"PUB-LFC-01", ASTRO_SOURCE_DIR "/data/public/zenodo-3254683/raw/example-cryo-LFC/ccd.037.0.fits",
     "5d0e92f0c6a5749e2a264129d1821d29f8941b09adb36ce11a8e7cf5067296c3", 2080, 4128, ""},
    {"PUB-CFA-01", ASTRO_SOURCE_DIR "/data/public/zenodo-17843622/first_sub.fits",
     "760b9aa547c9192919749cd0bdb93b034a72787ef081bb558713b3b653aedd06", 1080, 1920, "GRBG"},
};

TEST(ImportReal, T_M2R1_09_public_samples_are_imported_inventoried_and_roundtrip_bit_exact) {
  const cancel_token cancel;
  for (const sample& smp : samples) {
    SCOPED_TRACE(smp.name);
    memory_pool pool(512 * MiB);
    fits_input input = open_fits_input(smp.path);
    const content_digest digest = digest_fits_input(input, pool);
    ASSERT_EQ(digest.sha256, smp.sha256) << "il file non e' quello registrato";

    fits_read_options options;
    options.declared_phase = process_phase::raw;
    if (smp.bayer[0] != '\0') {
      options.declared_bayer_x_offset = 0;
      options.declared_bayer_y_offset = 0;
    }
    const fits_inventory inv = inventory_fits(input, options, pool);
    EXPECT_EQ(inv.plan.width, smp.width);
    EXPECT_EQ(inv.plan.height, smp.height);
    EXPECT_EQ(inv.hdus.size(), 1u);
    EXPECT_EQ(inv.hdus[0].bitpix, 16);
    EXPECT_EQ(inv.hdus[0].bzero, 32768.0);
    EXPECT_EQ(inv.header_bytes_read, inv.hdus[0].data_offset);
    if (smp.bayer[0] != '\0') {
      ASSERT_TRUE(inv.cfa.has_value());
      EXPECT_EQ(inv.layout, plane_layout::cfa);
      EXPECT_EQ(inv.cfa->pattern, cfa_pattern::grbg);
      EXPECT_EQ(inv.metadata.camera->value, "Seestar S30");
    } else {
      EXPECT_FALSE(inv.cfa.has_value());
      EXPECT_EQ(inv.layout, plane_layout::mono);
    }
    EXPECT_FALSE(inv.uninterpreted_keywords.empty());

    // Job di import su uno store.
    const fs::path dir = fresh(std::string("t09_") + smp.name);
    auto store = sqlite_dag_store::create(dir / "project");
    import_request request;
    request.path = smp.path;
    request.digest = digest;
    request.read_options = options;
    const node_spec node = make_import_node(request);
    store->put_node(node);
    store->create_slot("imp", node.key());
    testing::internal::CaptureStderr();
    const job_outcome out = run_import_job(*store, "imp", request, pool, cancel);
    testing::internal::GetCapturedStderr();
    ASSERT_EQ(out.exit_code, 0);
    const slot_view view = store->read_slot("imp");
    ASSERT_EQ(view.state, freshness::current);
    const json report = parse_json(text_of(store->artifact_path(view.artifact->rel_path)));
    EXPECT_EQ(report.find("source")->find("sha256")->as_string(), smp.sha256);
    EXPECT_EQ(report.find("image")->find("width")->as_integer(), static_cast<std::int64_t>(smp.width));

    // Roundtrip: decodifica, riscrittura con lo stesso encoding, stessa data unit byte per byte.
    const fx::bytes original = read_file(smp.path);
    const image img = decode_fits(original, 512 * MiB, options);
    EXPECT_EQ(img.geometry.width, smp.width);
    const fx::bytes written = encode_fits(img);
    const fx::raw_primary_view a = fx::parse_hdu(original);
    const fx::raw_primary_view b = fx::parse_hdu(written);
    EXPECT_EQ(a.bitpix, b.bitpix);
    EXPECT_EQ(a.axes, b.axes);
    EXPECT_TRUE(a.data == b.data) << "data unit diversa";
    const image back = decode_fits(written, 512 * MiB);
    EXPECT_TRUE(equivalent_content(back, img));
    EXPECT_EQ(encode_fits(back), written);
  }
}

}  // namespace
