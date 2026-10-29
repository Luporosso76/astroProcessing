# Da includere con `source`: il PC è condiviso, build e test usano al massimo il 70% delle CPU.
# Esporta CMAKE_BUILD_PARALLEL_LEVEL (default: il tetto) e rifiuta con uscita 2 un valore non valido
# di CMAKE_BUILD_PARALLEL_LEVEL o CTEST_PARALLEL_LEVEL. ctest senza CTEST_PARALLEL_LEVEL resta seriale.
cap=$(( $(nproc) * 7 / 10 ))
cap=$(( cap > 0 ? cap : 1 ))
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$cap}"
for var in CMAKE_BUILD_PARALLEL_LEVEL CTEST_PARALLEL_LEVEL; do
  [ -z "${!var+set}" ] && continue
  # Al massimo 4 cifre: un numero oltre gli interi di Bash farebbe fallire il confronto numerico in silenzio.
  if ! [[ "${!var}" =~ ^[1-9][0-9]{0,3}$ ]] || [ "${!var}" -gt "$cap" ]; then
    echo "$var=${!var} non valido: intero fra 1 e $cap (70% di $(nproc) CPU)" >&2
    exit 2
  fi
done
