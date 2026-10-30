// Casi di OB-07 e OB-11 (docs/contracts/BASE-confini.md) che isolano un controllo alla volta: ogni campo dell'header del
// piano, il file aperto invariato, la riverifica al percorso, il cancel nei prepare di piu' blocchi e nel finalize.
// Il banco di prova e' quello di test_conformance.cpp, incluso senza modificarlo; il filtro di ctest seleziona solo i
// casi di questo file.

#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "test_conformance.cpp"

#include <fcntl.h>
#include <sys/mman.h>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/import_job.hpp"
#include "astro/master.hpp"
#include "expect_stop.hpp"

namespace {

namespace fx = astro_fixture;

constexpr std::int64_t gap_epoch = 1'000'000'000;

fs::path gap_file(const std::string& name) {
  const fs::path dir = fs::absolute(fs::path(ASTRO_TEST_TMP) / "conformance_gaps");
  fs::create_directories(dir);
  return dir / name;
}

void set_mtime(const fs::path& p, std::int64_t seconds) {
  const timespec ts[2] = {{seconds, 0}, {seconds, 0}};
  if (::utimensat(AT_FDCWD, p.c_str(), ts, 0) != 0) throw std::runtime_error("utimensat non riuscita: " + p.string());
}

// Cambia un byte nello stesso inode, senza cambiare la dimensione.
void flip_byte_in_place(const fs::path& p, std::size_t offset) {
  std::vector<std::uint8_t> b = read_bytes(p);
  b.at(offset) ^= 0x01;
  overwrite(p, b);
}

std::string context_value(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return {};
}

// Frame light f32 largo e alto a sufficienza da occupare piu' blocchi di digest (1 MiB ciascuno).
fx::bytes large_light_frame(std::uint32_t w, std::uint32_t h) {
  fx::calib_header hd;
  hd.egain = 2.0;
  hd.imagetyp = "Light Frame";
  hd.exposure = 60;
  std::vector<double> v(static_cast<std::size_t>(w) * h);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = 1000.0 + static_cast<double>((i * 7) % 4096);
  return fx::float_frame(w, h, -32, v, fx::calib_cards(hd));
}

// Sostituisce il file del ruolo e ricostruisce nodo e slot sul nuovo digest.
void replace_input(type_case& c, const std::string& role, const fx::bytes& b) {
  overwrite(c.inputs.at(role).path, b);
  c.inputs.at(role).digest = digest_of(b);
  c.job.emplace(c.entry.make(c.inputs, c.variant.name, c.store->root()));
  c.store->put_node(c.job->node);
  (void)c.store->retarget_slot(slot, c.job->node.key());
  c.snapshot();
}

class ConformanceGapsIo : public testing::TestWithParam<type_param> {};
class ConformanceGapsMaster : public testing::TestWithParam<type_param> {};
class ConformanceGapsBlocks : public testing::TestWithParam<type_param> {};

// Mappa un file in lettura e scrittura condivisa: una scrittura su una pagina gia' sporca non aggiorna mtime e ctime.
class shared_mapping {
 public:
  explicit shared_mapping(const fs::path& p) {
    fd_ = ::open(p.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) throw std::runtime_error("open O_RDWR non riuscita");
    void* m = MAP_FAILED;
    try {
      size_ = fs::file_size(p);
      m = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    } catch (...) {
      ::close(fd_);
      throw;
    }
    if (m == MAP_FAILED) {
      ::close(fd_);
      throw std::runtime_error("mmap non riuscita");
    }
    base_ = static_cast<volatile std::uint8_t*>(m);
  }
  ~shared_mapping() {
    ::munmap(const_cast<std::uint8_t*>(base_), size_);
    ::close(fd_);
  }
  shared_mapping(const shared_mapping&) = delete;
  shared_mapping& operator=(const shared_mapping&) = delete;
  void touch(std::size_t offset) { base_[offset] = base_[offset]; }
  void flip(std::size_t offset) { base_[offset] = static_cast<std::uint8_t>(base_[offset] ^ 0x01); }

 private:
  int fd_ = -1;
  std::size_t size_ = 0;
  volatile std::uint8_t* base_ = nullptr;
};

}  // namespace

// ---------------------------------------------------------------- T-M3BASE-11 (OB-11): confronto dell'header, un campo alla volta

namespace {

struct planned_case {
  fs::path path;
  fx::bytes bytes;
  fits_plan plan;
};

planned_case make_planned_case(const std::string& name) {
  planned_case c;
  c.path = gap_file(name);
  c.bytes = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  overwrite(c.path, c.bytes);
  set_mtime(c.path, gap_epoch);
  c.plan = plan_fits(c.bytes);
  return c;
}

// `read_verified_input` sul file di `c` con il piano `planned`: lo stop atteso e' integrity `code` sul campo `field`.
void expect_header_stop(const planned_case& c, const fits_plan& planned, const char* code, const char* field) {
  const fits_input input = open_fits_input(c.path.string());
  try {
    (void)read_verified_input(input, c.path.string(), digest_of(c.bytes), std::uint64_t{1} << 30, planned);
    ADD_FAILURE() << "nessuno stop per il campo " << field;
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::integrity) << field << ": " << e.what();
    EXPECT_EQ(e.code(), std::string(code)) << field << ": " << e.what();
    EXPECT_EQ(context_value(e, "campo"), std::string(field)) << e.what();
  }
}

}  // namespace

TEST(HashGaps, T_M3BASE_11_each_planned_header_field_alone_stops_the_read) {
  const planned_case c = make_planned_case("header_fields.fits");
  {  // controllo: il piano del file stesso passa
    const fits_input input = open_fits_input(c.path.string());
    EXPECT_NO_THROW((void)read_verified_input(input, c.path.string(), digest_of(c.bytes), std::uint64_t{1} << 30, c.plan));
  }
  {  // dimensione sola: ogni altro valore del piano e' quello del file
    fits_plan p = c.plan;
    p.file_bytes += 2880;
    expect_header_stop(c, p, "input.size_changed", "dimensione");
  }
  {  // tipo del campione solo: BITPIX uguale
    fits_plan p = c.plan;
    ASSERT_EQ(p.type, sample_type::f32);
    p.type = sample_type::f64;
    expect_header_stop(c, p, "input.header_changed", "tipo");
  }
  {  // BITPIX solo: tipo uguale. BITPIX 8, 16 e -32 danno lo stesso tipo f32.
    fits_plan p = c.plan;
    p.bitpix = p.bitpix == -32 ? 16 : -32;
    ASSERT_EQ(p.type, c.plan.type);
    expect_header_stop(c, p, "input.header_changed", "BITPIX");
  }
  {  // INVALID solo
    fits_plan p = c.plan;
    p.has_invalid = !p.has_invalid;
    expect_header_stop(c, p, "input.header_changed", "invalid");
  }
  {  // VARIANCE sola
    fits_plan p = c.plan;
    p.has_variance = !p.has_variance;
    expect_header_stop(c, p, "input.header_changed", "varianza");
  }
}

// BITPIX diverso a tipo uguale con file reali: il piano di un file a 16 bit contro un file -32 con la stessa geometria.
TEST(HashGaps, T_M3BASE_11_bitpix_change_with_the_same_sample_type_is_header_changed) {
  const fs::path int16_path = gap_file("bitpix16.fits");
  const fx::bytes int16 = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  overwrite(int16_path, int16);
  const fits_plan planned = plan_fits(int16);
  ASSERT_EQ(planned.bitpix, 16);

  const fs::path float_path = gap_file("bitpix_f32.fits");
  fx::calib_header hd;
  hd.imagetyp = "Dark Frame";
  hd.exposure = 60;
  const fx::bytes f32 = fx::float_frame(8, 6, -32, std::vector<double>(48, 1000.0), fx::calib_cards(hd));
  overwrite(float_path, f32);
  const fits_plan verified = plan_fits(f32);
  ASSERT_EQ(verified.bitpix, -32);
  ASSERT_EQ(planned.type, verified.type) << "BITPIX 16 e -32 devono dare lo stesso tipo del campione";
  ASSERT_EQ(planned.width, verified.width);
  ASSERT_EQ(planned.height, verified.height);
  ASSERT_EQ(planned.planes, verified.planes);
  fits_plan same_size = planned;
  same_size.file_bytes = verified.file_bytes;
  try {
    require_planned_header(same_size, verified, float_path.string());
    ADD_FAILURE() << "BITPIX diverso non fermato";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::integrity);
    EXPECT_EQ(e.code(), std::string("input.header_changed")) << e.what();
    EXPECT_EQ(context_value(e, "campo"), "BITPIX");
  }
}

// ---------------------------------------------------------------- T-M3BASE-11 (OB-11): file aperto invariato

// Il digest e l'header del file aperto coincidono con quelli attesi (le attese sono quelle del contenuto nuovo), ma
// il file e' cambiato dopo l'apertura: lo stop e' input.modified. Il contenuto nuovo ha lo stesso header e la stessa
// dimensione, quindi nessun altro confronto lo ferma.
TEST(HashGaps, T_M3BASE_11_read_verified_input_stops_a_file_modified_after_the_open) {
  planned_case c = make_planned_case("read_modified.fits");
  const fits_input input = open_fits_input(c.path.string());
  const fits_plan planned = plan_fits(input);
  const std::size_t offset = data_offset(c.bytes);
  fx::bytes changed = c.bytes;
  changed[offset] ^= 0x01;
  flip_byte_in_place(c.path, offset);
  set_mtime(c.path, gap_epoch + 10);
  EXPECT_STOP(read_verified_input(input, c.path.string(), digest_of(changed), std::uint64_t{1} << 30, planned),
              error_category::integrity, "input.modified");
}

// Il percorso riaperto e' invariato e ha il digest atteso; l'input aperto prima del lavoro e' cambiato.
TEST(HashGaps, T_M3BASE_11_reverify_input_stops_an_opened_input_modified_since_the_open) {
  planned_case c = make_planned_case("reverify_opened.fits");
  const fits_input opened = open_fits_input(c.path.string());
  const std::size_t offset = data_offset(c.bytes);
  fx::bytes changed = c.bytes;
  changed[offset] ^= 0x01;
  std::vector<std::byte> work(2880);
  {  // controllo: input aperto invariato
    EXPECT_NO_THROW(reverify_input(c.path.string(), &opened, digest_of(c.bytes), work, {}));
  }
  flip_byte_in_place(c.path, offset);
  set_mtime(c.path, gap_epoch + 10);
  EXPECT_NO_THROW(reverify_input(c.path.string(), nullptr, digest_of(changed), work, {}));
  EXPECT_STOP(reverify_input(c.path.string(), &opened, digest_of(changed), work, {}), error_category::integrity,
              "input.modified");
}

// ---------------------------------------------------------------- T-M3BASE-07 (OB-07): prepare dei tipi di I/O, input di piu' blocchi

// Il file occupa piu' blocchi di digest: il cancel richiesto all'ingresso del prepare ferma prima di ogni lettura, e il
// cancel richiesto durante la lettura del digest ferma prima del blocco successivo.
TEST_P(ConformanceGapsIo, T_M3BASE_07_prepare_cancel_in_a_multi_block_input) {
  const fx::bytes big = large_light_frame(1000, 1000);
  ASSERT_GT(big.size(), 3 * MiB);
  {  // controllo: senza cancel il job riesce
    type_case c(GetParam(), "_control");
    replace_input(c, "file", big);
    const ran r = c.run_default();
    EXPECT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << describe(r);
  }
  {  // cancel all'ingresso del prepare
    type_case c(GetParam(), "_entry");
    replace_input(c, "file", big);
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    std::atomic<std::size_t> mark{0};
    pr.action = [&](std::string_view p, std::string_view) {
      if (p == "sched.prepare" && !cancel.requested()) {
        mark = pr.mark();
        cancel.request();
      }
    };
    const ran r = c.run(c.env(pool, cancel));
    EXPECT_TRUE(cancel.requested()) << "punto di iniezione sched.prepare non raggiunto";
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::cancelled, "sched.cancelled", "", {}, false}));
    EXPECT_EQ(pr.count("input.read", mark), 0u) << "blocchi letti nel prepare dopo la richiesta";
    EXPECT_EQ(pr.count("sched.task", mark), 0u);
    EXPECT_EQ(pr.count("sched.finalize"), 0u);
  }
  {  // cancel durante la lettura del digest
    type_case c(GetParam(), "_during");
    replace_input(c, "file", big);
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    std::atomic<bool> in_prepare{false};
    std::atomic<std::size_t> mark{0};
    pr.action = [&](std::string_view p, std::string_view) {
      if (p == "sched.prepare") in_prepare = true;
      if (p == "input.read" && in_prepare && !cancel.requested()) {
        mark = pr.mark();
        cancel.request();
      }
    };
    const ran r = c.run(c.env(pool, cancel));
    EXPECT_TRUE(cancel.requested()) << "nessuna lettura nel prepare";
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::cancelled, "sched.cancelled", "", {}, false}));
    EXPECT_EQ(pr.count("input.read", mark), 0u) << "blocchi letti dopo la richiesta, durante il digest";
    EXPECT_EQ(pr.count("sched.task", mark), 0u);
    EXPECT_EQ(pr.count("sched.finalize"), 0u);
  }
}

// ---------------------------------------------------------------- T-M3BASE-07 (OB-07): riverifica del master

// Il cancel richiesto subito prima del finalize (e quello richiesto all'apertura del primo input da riverificare) ferma
// la riverifica prima di aprire o leggere altri input.
TEST_P(ConformanceGapsMaster, T_M3BASE_07_finalize_reverify_stops_at_the_cancel) {
  {  // richiesta prima del finalize
    type_case c(GetParam(), "_before");
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    std::atomic<std::size_t> mark{0};
    pr.action = [&](std::string_view p, std::string_view) {
      if (p == "sched.finalize" && !cancel.requested()) {
        mark = pr.mark();
        cancel.request();
      }
    };
    const ran r = c.run(c.env(pool, cancel));
    EXPECT_TRUE(cancel.requested()) << "punto sched.finalize non raggiunto";
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::cancelled, "sched.cancelled", "", {}, false}));
    EXPECT_EQ(pr.count("input.open", mark), 0u) << "input riaperti dalla riverifica dopo la richiesta";
    EXPECT_EQ(pr.count("input.read", mark), 0u) << "blocchi letti dalla riverifica dopo la richiesta";
  }
  {  // richiesta all'apertura del primo input riverificato
    type_case c(GetParam(), "_during");
    probe pr;
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    std::atomic<bool> in_finalize{false};
    std::atomic<std::size_t> mark{0};
    pr.action = [&](std::string_view p, std::string_view) {
      if (p == "sched.finalize") in_finalize = true;
      if (p == "input.open" && in_finalize && !cancel.requested()) {
        mark = pr.mark();
        cancel.request();
      }
    };
    const ran r = c.run(c.env(pool, cancel));
    EXPECT_TRUE(cancel.requested()) << "nessuna riapertura nel finalize";
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::cancelled, "sched.cancelled", "", {}, false}));
    EXPECT_EQ(pr.count("input.open", mark), 0u) << "altri input riaperti dopo la richiesta";
    EXPECT_EQ(pr.count("input.read", mark), 0u) << "blocchi letti dopo la richiesta";
  }
}

// ---------------------------------------------------------------- T-M3BASE-11 (OB-11): riverifica della residenza a blocchi

// Un input riscritto da una mappatura condivisa dopo l'ultimo task e prima del finalize non cambia dimensione, mtime
// ne' ctime (la pagina e' gia' sporca): lo ferma solo la riverifica del digest al percorso, integrity
// input.digest_mismatch. Il file e' lo stesso inode, quindi il controllo del percorso passa.
TEST_P(ConformanceGapsBlocks, T_M3BASE_11_finalize_reverifies_the_digest_of_every_input) {
  for (const std::string& role : registered_job_types()[GetParam().entry].input_roles) {
    SCOPED_TRACE(role);
    type_case c(GetParam(), "_" + role);
    const std::string path = c.inputs.at(role).path;
    const std::size_t offset = data_offset(read_bytes(path));
    shared_mapping map(path);
    map.touch(offset);
    probe pr;
    std::atomic<bool> flipped{false};
    pr.action = [&](std::string_view p, std::string_view) {
      if (p == "sched.finalize" && !flipped.exchange(true)) map.flip(offset);
    };
    memory_pool pool(pool_bytes);
    cancel_token cancel;
    const ran r = c.run(c.env(pool, cancel));
    EXPECT_TRUE(flipped.load()) << "punto sched.finalize non raggiunto";
    c.refresh(role);
    EXPECT_NO_FAILURES(check_common(c, r, {error_category::integrity, "input.digest_mismatch", "", {}, false}));
  }
}

INSTANTIATE_TEST_SUITE_P(Types, ConformanceGapsIo,
                         testing::ValuesIn(params_where([](const job_type_entry& e, const job_type_variant&) {
                           const std::string id = e.operator_id;
                           return id == import_operator_id || id == thumbnail_operator_id || id == cache_rebuild_operator_id;
                         })),
                         param_name);
INSTANTIATE_TEST_SUITE_P(Types, ConformanceGapsMaster,
                         testing::ValuesIn(params_where([](const job_type_entry& e, const job_type_variant&) {
                           return std::string(e.operator_id) == master_operator_id;
                         })),
                         param_name);
INSTANTIATE_TEST_SUITE_P(Types, ConformanceGapsBlocks,
                         testing::ValuesIn(params_where([](const job_type_entry& e, const job_type_variant& v) {
                           return std::string(e.operator_id) == calibration_operator_id && v.name == "blocchi";
                         })),
                         param_name);
