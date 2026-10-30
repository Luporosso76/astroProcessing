// Residenza a blocchi della calibrazione (ADR-06, estensione R3.2): ingressi letti a blocchi di righe e verificati,
// normalizzazione con selezione esatta in passate, uscita nello staging dello scratch e FITS in streaming.
#include <algorithm>
#include <array>
#include <cstring>

#include "astro/error.hpp"
#include "astro/fits_blocks.hpp"
#include "astro/scheduler.hpp"
#include "calibration_internal.hpp"

namespace astro {

namespace calib_detail {

namespace {

std::uint64_t key_of(double v) noexcept {
  if (v == 0) v = 0.0;
  std::uint64_t bits = 0;
  std::memcpy(&bits, &v, sizeof bits);
  return (bits >> 63) != 0 ? ~bits : bits | (std::uint64_t{1} << 63);
}

}  // namespace

void kth_selector::add(double v) noexcept {
  const std::uint64_t key = key_of(v);
  if (pass_ > 0 && (key >> (64 - 8 * pass_)) != (prefix_ >> (64 - 8 * pass_))) return;
  ++hist_[(key >> (56 - 8 * pass_)) & 0xFF];
}

void kth_selector::end_pass(std::uint64_t rank) {
  ASTRO_ASSERT(pass_ < passes && rank >= below_);
  std::uint64_t r = rank - below_;
  std::size_t digit = 0;
  for (; digit < hist_.size(); ++digit) {
    if (r < hist_[digit]) break;
    r -= hist_[digit];
    below_ += hist_[digit];
  }
  ASTRO_ASSERT(digit < hist_.size());
  prefix_ |= std::uint64_t{digit} << (56 - 8 * pass_);
  hist_.fill(0);
  ++pass_;
}

double kth_selector::result() const noexcept {
  ASTRO_ASSERT(done());
  const std::uint64_t bits = (prefix_ >> 63) != 0 ? prefix_ ^ (std::uint64_t{1} << 63) : ~prefix_;
  double v = 0;
  std::memcpy(&v, &bits, sizeof v);
  return v;
}

namespace {

constexpr const char* input_names[4] = {"light", "dark", "flat", "dark_flat"};

[[noreturn]] void fail(error_category c, const char* code, const char* message, stop_error::context_list ctx = {}) {
  throw stop_error(c, code, message, std::move(ctx));
}

void swap_in_place(std::byte* data, std::uint64_t count, std::size_t width) {
  for (std::uint64_t i = 0; i < count; ++i) std::reverse(data + i * width, data + (i + 1) * width);
}

std::span<const std::uint8_t> as_u8(const std::byte* p, std::uint64_t n) {
  return {reinterpret_cast<const std::uint8_t*>(p), static_cast<std::size_t>(n)};
}

struct input_bands {
  std::array<row_band, 4> bands{};
  std::array<std::span<std::byte>, 4> raw{};
};

struct output_band {
  std::byte* samples = nullptr;
  std::byte* variance = nullptr;
  std::uint16_t* mask = nullptr;
};

// Disposizione dei buffer di `t` righe in un pool_buffer: array a 8 byte, poi a 4, poi maschere, poi byte grezzi;
// con la base allineata a 64 byte ogni array resta allineato al suo tipo senza riempimento.
struct block_layout {
  std::uint32_t w = 0, t = 0;
  std::array<std::uint64_t, 4> elem{}, raw_per_row{};
  std::array<bool, 4> variance{};
  std::uint64_t out_elem = 4;

  std::uint64_t inputs_bytes() const {
    std::uint64_t n = 0;
    for (std::size_t k = 0; k < 4; ++k) {
      n += std::uint64_t{t} * (std::uint64_t{w} * (elem[k] + 2 + (variance[k] ? elem[k] : 0)) + raw_per_row[k]);
    }
    return n;
  }
  std::uint64_t out_bytes() const { return std::uint64_t{t} * w * (2 * out_elem + 2); }
  std::uint64_t scratch_bytes() const { return std::uint64_t{w} * (2 * sizeof(double) + sizeof(std::uint16_t)); }

  // Con `out` assegna anche campioni, varianza e maschera dell'uscita (stesse regole di allineamento).
  std::byte* place(std::byte* cur, input_bands& in, output_band* out = nullptr) const {
    const std::uint64_t n = std::uint64_t{t} * w;
    for (const std::uint64_t width : {std::uint64_t{8}, std::uint64_t{4}}) {
      if (out != nullptr && out_elem == width) {
        out->samples = cur;
        cur += n * out_elem;
        out->variance = cur;
        cur += n * out_elem;
      }
      for (std::size_t k = 0; k < 4; ++k) {
        if (elem[k] != width) continue;
        in.bands[k].samples = cur;
        cur += n * elem[k];
        in.bands[k].variance = nullptr;
        if (variance[k]) {
          in.bands[k].variance = cur;
          cur += n * elem[k];
        }
      }
    }
    for (std::size_t k = 0; k < 4; ++k) {
      in.bands[k].mask = reinterpret_cast<std::uint16_t*>(cur);
      cur += n * 2;
    }
    if (out != nullptr) {
      out->mask = reinterpret_cast<std::uint16_t*>(cur);
      cur += n * 2;
    }
    for (std::size_t k = 0; k < 4; ++k) {
      in.raw[k] = std::span<std::byte>(cur, static_cast<std::size_t>(std::uint64_t{t} * raw_per_row[k]));
      cur += std::uint64_t{t} * raw_per_row[k];
    }
    return cur;
  }
};

input_view band_view(const input_view& whole, const row_band& b) {
  input_view v = whole;
  v.samples = b.samples;
  v.mask = b.mask;
  if (v.plane) v.variance_plane = b.variance;
  return v;
}

json admission_json(const job_report& r) {
  json samples = json::array();
  for (const admission_sample& s : r.admission) {
    json e = json::object();
    e.set("psi", s.psi ? json::number(*s.psi) : json::null());
    e.set("admitted", json::boolean(s.admitted));
    samples.push(std::move(e));
  }
  json o = json::object();
  o.set("samples", std::move(samples));
  o.set("pressure_wait_samples", json::uinteger(r.pressure_wait_samples));
  return o;
}

}  // namespace

json scheduler_details(const job_report& r) {
  json o = json::object();
  o.set("tasks_run", json::uinteger(r.tasks_run));
  o.set("tasks_resumed", json::uinteger(r.tasks_resumed));
  o.set("checkpoints_written", json::uinteger(r.checkpoints_written));
  return o;
}

json admission_details(const job_report& r) { return admission_json(r); }

job_files_result run_block_calibration(sqlite_dag_store& store, memory_pool& pool, const block_job_request& req,
                                       const cancel_token& cancel) {
  const calibration_recipe& recipe = req.recipe;
  const resource_plan& rp = req.plan;
  const std::array<const calibration_input_spec*, 4> specs = {&recipe.light, &recipe.dark, &recipe.flat,
                                                              &recipe.dark_flat};
  const std::uint32_t w = req.plans[0].width, h = req.plans[0].height;
  const std::uint32_t t = rp.tile_rows;
  const std::uint64_t samples = std::uint64_t{w} * h;
  const std::uint64_t out_elem = sample_size(req.plans[0].type);

  block_layout layout;
  layout.w = w;
  layout.t = t;
  layout.out_elem = out_elem;
  for (std::size_t k = 0; k < 4; ++k) {
    layout.elem[k] = sample_size(req.plans[k].type);
    layout.variance[k] = *specs[k]->variance.source == variance_source::plane && req.plans[k].has_variance;
    layout.raw_per_row[k] = std::uint64_t{w} * (primary_sample_bytes(open_fits_input(specs[k]->path)) +
                                               (req.plans[k].has_invalid ? 2 : 0) +
                                               (layout.variance[k] ? layout.elem[k] : 0));
  }

  std::array<std::optional<fits_block_source>, 4> sources;
  std::unique_ptr<prepared_calibration::impl> prepared;
  spill_file* staging = nullptr;
  reservation* shared = nullptr;
  json observed = json::array();
  std::vector<stored_file> files;
  std::vector<std::string> warnings;
  const std::uint64_t region_samples = samples * out_elem;          // inizio della maschera nello staging
  const std::uint64_t region_variance = samples * (out_elem + 2);  // inizio della varianza nello staging

  job_plan plan;
  plan.shared_bytes = rp.shared_bytes;
  plan.per_task_bytes = rp.per_task_bytes;
  plan.worker_count = rp.worker_count;
  plan.queue_capacity = rp.task_queue_capacity;
  plan.tile_rows = t;
  plan.spill = rp.spill;
  if (req.options.checkpoint) plan.checkpoint_every = req.options.checkpoint->every;

  // Byte dello staging di un blocco: le tre regioni nell'ordine campioni, maschera, varianza.
  const auto tile_regions = [&](std::uint64_t tile) {
    const std::uint64_t y0 = tile * t, y1 = std::min<std::uint64_t>(h, y0 + t);
    const std::uint64_t first = y0 * w, n = (y1 - y0) * w;
    return std::array<std::pair<std::uint64_t, std::uint64_t>, 3>{
        {{first * out_elem, n * out_elem}, {region_samples + first * 2, n * 2}, {region_variance + first * out_elem, n * out_elem}}};
  };

  job_spec spec(req.node, (std::uint64_t{h} + t - 1) / t, [&](task_context& ctx) -> std::string {
    const prepared_calibration::impl& p = *prepared;
    pool_buffer scratch(ctx.memory, static_cast<std::size_t>(layout.scratch_bytes()));
    pool_buffer buffer(ctx.memory, static_cast<std::size_t>(layout.inputs_bytes() + layout.out_bytes()));
    void* kernel_scratch = scratch.data();
    input_bands in;
    output_band out;
    const std::byte* end = layout.place(buffer.data(), in, &out);
    ASTRO_ASSERT(end == buffer.data() + buffer.size());
    std::byte* out_samples = out.samples;
    std::byte* out_variance = out.variance;
    std::uint16_t* out_mask = out.mask;
    for (std::size_t k = 0; k < 4; ++k) {
      ctx.check_cancel();
      sources[k]->read_tile(ctx.index, in.raw[k], in.bands[k]);
    }
    const std::uint32_t y0 = in.bands[0].row_begin, y1 = in.bands[0].row_end;
    const std::uint64_t n = std::uint64_t{y1 - y0} * w;
    const input_view L = band_view(p.light, in.bands[0]), D = band_view(p.dark, in.bands[1]),
                     F = band_view(p.flat, in.bands[2]), DF = band_view(p.dark_flat, in.bands[3]);
    std::fill(out_mask, out_mask + n, std::uint16_t{0});
    const kernel_io io{&L, &D, &F, &DF, out_samples, out_variance, out_mask, y0};
    for (std::uint32_t y = y0; y < y1; ++y) {
      ctx.check_cancel();
      calibrate_band(p, io, y, y + 1, kernel_scratch);
    }
    for (std::uint64_t i = 0; i < n; ++i) out_mask[i] = static_cast<std::uint16_t>(out_mask[i] ^ 0x8000u);
    auto* mask_bytes = reinterpret_cast<std::byte*>(out_mask);
    swap_in_place(out_samples, n, out_elem);
    swap_in_place(mask_bytes, n, 2);
    swap_in_place(out_variance, n, out_elem);
    const auto regions = tile_regions(ctx.index);
    const std::byte* parts[3] = {out_samples, mask_bytes, out_variance};
    sha256_stream digest;
    for (std::size_t r = 0; r < 3; ++r) {
      const std::span<const std::byte> bytes(parts[r], static_cast<std::size_t>(regions[r].second));
      staging->write(regions[r].first, bytes);
      digest.update(as_u8(parts[r], regions[r].second));
    }
    return digest.finish().sha256;
  });

  spec.prepare = [&](job_context& jc) {
    shared = &jc.shared;
    std::uint64_t pass_bytes = layout.inputs_bytes();
    pool_buffer pass(jc.shared, static_cast<std::size_t>(pass_bytes));
    // P0 di ogni ingresso, nell'ordine della residenza intera.
    std::array<image, 4> headers;
    for (std::size_t k = 0; k < 4; ++k) {
      sources[k].emplace(specs[k]->path, specs[k]->digest, specs[k]->read_options, t, layout.variance[k]);
      fits_block_source& src = *sources[k];
      // Header del piano uguali a quelli che la passata P0 verifica, prima che P0 legga un campione.
      require_planned_header(req.plans[k], src.plan(), specs[k]->path);
      src.verify(std::span<std::byte>(pass.data(), pass.size()), [&jc] { jc.check_cancel(); });
      headers[k] = src.header();
    }
    prepared = prepare_headers(recipe, calibration_images{std::move(headers[0]), std::move(headers[1]),
                                                          std::move(headers[2]), std::move(headers[3])});
    prepared_calibration::impl& p = *prepared;
    const std::array<const input_view*, 4> views = {&p.light, &p.dark, &p.flat, &p.dark_flat};
    input_bands in;
    layout.place(pass.data(), in);
    const std::uint64_t tiles = sources[0]->tile_count();
    // Piani di varianza, ingresso per ingresso come make_view.
    for (std::size_t k = 0; k < 4; ++k) {
      if (!views[k]->plane) continue;
      for (std::uint64_t tile = 0; tile < tiles; ++tile) {
        jc.check_cancel();
        sources[k]->read_tile(tile, in.raw[k], in.bands[k]);
        const input_view v = band_view(*views[k], in.bands[k]);
        const std::uint64_t first = std::uint64_t{in.bands[k].row_begin} * w;
        check_variance_rows(v, input_names[k], first, std::uint64_t{in.bands[k].row_end - in.bands[k].row_begin} * w);
      }
    }
    // Normalizzazione: prima passata con conteggi e Welford, poi 8 passate di selezione per Fc e per i termini di varianza.
    const std::size_t channels = p.per_channel ? 4 : 1;
    std::vector<normalization_acc> acc(channels);
    std::vector<std::array<kth_selector, 4>> sel(channels);  // fc basso, fc alto, vfc basso, vfc alto
    std::optional<std::pair<std::uint32_t, std::uint32_t>> bad_variance;
    for (int pass_index = 0; pass_index < kth_selector::passes; ++pass_index) {
      for (std::uint64_t tile = 0; tile < tiles; ++tile) {
        jc.check_cancel();
        sources[2]->read_tile(tile, in.raw[2], in.bands[2]);
        sources[3]->read_tile(tile, in.raw[3], in.bands[3]);
        const input_view F = band_view(p.flat, in.bands[2]), DF = band_view(p.dark_flat, in.bands[3]);
        const std::uint32_t y0 = in.bands[2].row_begin, y1 = in.bands[2].row_end;
        for_valid_n(p, F, DF, y0, y0, y1, [&](std::size_t c, std::uint32_t x, std::uint32_t y, std::size_t i) {
          const double v = F.value(i) - DF.value(i);
          const double term = fc_variance_term(F, DF, i);
          if (pass_index == 0) {
            acc[c].add(v);
            if (!std::isfinite(term) && !bad_variance) bad_variance = std::make_pair(x, y);
          }
          sel[c][0].add(v);
          sel[c][1].add(v);
          if (!bad_variance && std::isfinite(term)) {
            sel[c][2].add(term);
            sel[c][3].add(term);
          }
        });
      }
      for (std::size_t c = 0; c < channels; ++c) {
        const std::uint64_t n = acc[c].n;
        if (n == 0) continue;
        const std::uint64_t hi = n / 2, lo = n % 2 == 1 ? hi : hi - 1;
        sel[c][0].end_pass(lo);
        sel[c][1].end_pass(hi);
        if (!bad_variance) {
          sel[c][2].end_pass(lo);
          sel[c][3].end_pass(hi);
        }
      }
    }
    const auto median = [&](std::size_t c, std::size_t first) {
      const std::uint64_t n = acc[c].n;
      const double hi = sel[c][first + 1].result();
      const double r = n % 2 == 1 ? hi : (sel[c][first].result() + hi) * 0.5;
      return r == 0 ? 0.0 : r;
    };
    finish_normalization(p, acc, [&](std::size_t c) { return median(c, 0); });
    if (bad_variance) stop_fc_variance_nonfinite(bad_variance->first, bad_variance->second);
    finish_variance(p, [&](std::size_t c) { return median(c, 2); });
    observed = observed_inputs(p);
    if (req.options.fault) jc.spill->set_fault_injector(req.options.fault);
    staging = req.options.checkpoint
                  ? &jc.spill->open_named_file(req.options.checkpoint->name + ".stage", req.options.checkpoint->resume)
                  : &jc.spill->create_shared_file();
    if (req.options.hook) req.options.hook("preflight.done");
  };

  spec.verify_partial = [&](std::uint64_t tile) {
    pool_buffer buffer(*shared, static_cast<std::size_t>(layout.out_bytes()));
    sha256_stream digest;
    for (const auto& [offset, bytes] : tile_regions(tile)) {
      const std::span<std::byte> dst(buffer.data(), static_cast<std::size_t>(bytes));
      staging->read(offset, dst);
      digest.update(as_u8(dst.data(), bytes));
    }
    return digest.finish().sha256;
  };

  spec.finalize = [&](job_context& jc) {
    prepared_calibration::impl& p = *prepared;
    pool_buffer buffer(jc.shared, static_cast<std::size_t>(layout.inputs_bytes()));
    const std::uint64_t chunk = buffer.size() / 2 * 2;
    std::array<std::uint64_t, 9> counts{};
    std::uint64_t invalid_pixels = 0;
    for (std::uint64_t done = 0; done < samples * 2;) {
      const std::uint64_t n = std::min(chunk, samples * 2 - done);
      staging->read(region_samples + done, std::span<std::byte>(buffer.data(), static_cast<std::size_t>(n)));
      const auto* b = reinterpret_cast<const std::uint8_t*>(buffer.data());
      for (std::uint64_t i = 0; i < n; i += 2) {
        const auto bits = static_cast<std::uint16_t>(((b[i] << 8) | b[i + 1]) ^ 0x8000u);
        if ((bits & invalid_reason::reserved_mask) != 0) {
          fail(error_category::invalid_input, "image.mask_reserved_bits", "bit riservato acceso in invalid_mask");
        }
        if (bits != 0) ++invalid_pixels;
        for (std::size_t k = 0; k < counts.size(); ++k) counts[k] += (bits >> k) & 1u;
      }
      done += n;
    }
    warnings = p.warnings;
    json report = calibration_report(p, counts, invalid_pixels);
    for (std::size_t k = 0; k < 4; ++k) {
      sources[k]->check_identity();
      reverify_input(specs[k]->path, nullptr, specs[k]->digest, {buffer.data(), buffer.size()},
                     [&jc] { jc.check_cancel(); });
    }
    const fits_stream_writer writer(p.out, invalid_pixels > 0);
    const auto read_staging = [&](std::uint64_t offset, std::span<std::byte> dst) { staging->read(offset, dst); };
    sha256_stream hash;
    {
      auto reader = writer.reader(read_staging);
      const std::span<std::uint8_t> out(reinterpret_cast<std::uint8_t*>(buffer.data()), buffer.size());
      while (const std::size_t n = reader(out)) hash.update(out.first(n));
    }
    const content_digest fits_digest = hash.finish();
    json image_out = json::object();
    image_out.set("sha256", json::string(fits_digest.sha256));
    image_out.set("size_bytes", json::uinteger(fits_digest.size_bytes));
    image_out.set("unit", json::string("adu"));
    json outputs = json::object();
    outputs.set("calibrated_image", std::move(image_out));
    report.set("outputs", std::move(outputs));
    const std::string report_json = canonical_json(report);
    const std::string report_text = calibration_report_text(report);
    const auto span = [](const std::string& s) {
      return std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
    };
    artifact_content image_content{{}, "fits", "image", fits_digest, calibration_image_role};
    image_content.source = writer.reader(read_staging);
    files = store.write_files(
        {std::move(image_content),
         artifact_content{span(report_json), "json", "calibration_report", std::nullopt, calibration_report_role},
         artifact_content{span(report_text), "txt", "calibration_report_text", std::nullopt,
                          calibration_report_text_role}});
  };

  if (req.options.checkpoint) {
    spec.checkpoint_dir = req.config.scratch->dir;
    spec.checkpoint_name = req.options.checkpoint->name;
    spec.resume = req.options.checkpoint->resume;
  }
  spec.fault = req.options.fault;
  if (req.options.on_job_plan) req.options.on_job_plan(plan);
  scheduler_tuning tuning = scheduler_tuning_of(req.config);
  tuning.pressure = req.options.pressure;
  job_scheduler scheduler(pool, system_affinity, std::move(tuning));
  const job_report report = scheduler.run(plan, spec, cancel);
  json details = json::object();
  details.set("observed_inputs", std::move(observed));
  details.set("scheduler", scheduler_details(report));
  details.set("admission", admission_json(report));
  return job_files_result{std::move(files), std::move(warnings), std::move(details)};
}

}  // namespace calib_detail

}  // namespace astro
