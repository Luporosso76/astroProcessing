#!/usr/bin/env bash
set -euo pipefail

forbidden_symbols='^(socket|connect|bind|listen|accept|accept4|getaddrinfo|gethostbyname|gethostbyname_r|curl_.*|BIO_s_connect|BIO_new_connect|OSSL_HTTP_.*|ffopen|ffopentest|ffdkopn|ffdopn|fftopn|ffiopn|ffeopn|ffinit|ffdkinit)(@.*)?$'
forbidden_needed='^(libcurl|libssl|libgnutls)'

[[ $# -gt 0 ]] || { echo "uso: $0 <binario|archivio|directory di oggetti>..." >&2; exit 2; }
status=0
for bin in "$@"; do
  if [[ -d "$bin" ]]; then
    mapfile -t objs < <(find "$bin" -name '*.o' -type f)
    [[ ${#objs[@]} -gt 0 ]] || { echo "OFFLINE nessun oggetto in $bin" >&2; exit 2; }
    syms=$(nm --undefined-only --format=just-symbols "${objs[@]}")
  elif [[ ! -f "$bin" ]]; then
    echo "OFFLINE binario assente: $bin" >&2; exit 2
  elif [[ "$bin" == *.a || "$bin" == *.o ]]; then
    syms=$(nm --undefined-only --format=just-symbols "$bin")
  else
    syms=$(nm -D --undefined-only --format=just-symbols "$bin")
    needed=$(readelf -d "$bin" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
    if bad=$(grep -E "$forbidden_needed" <<<"$needed"); then
      echo "OFFLINE NEEDED vietato in $bin: $bad" >&2; status=1
    fi
  fi
  if bad=$(grep -E "$forbidden_symbols" <<<"$syms"); then
    echo "OFFLINE simbolo vietato in $bin: $(tr '\n' ' ' <<<"$bad")" >&2; status=1
  fi
done
exit $status
