#pragma once

#include <array>
#include <string>

#include "astro/fault_injector.hpp"

// Verifica a runtime delle librerie compilate nel progetto (U12, docs/contracts/U12-third-party.md, obblighi 5 e 6).
// Una versione o una capacita' diversa da quella fissata e' un difetto della build, non un errore dell'input:
// ASTRO_ASSERT.
namespace astro::third_party {

// Valori letti dalle tre librerie collegate.
struct observed {
  int libraw_version_number = 0;
  std::string libraw_version;
  unsigned libraw_capabilities = 0;
  std::string cfitsio_version;  // fits_get_version stampato con "%.3f"
  int cfitsio_reentrant = 0;
  std::string wcslib_version;
  std::array<int, 3> wcslib_vers{};
};

// Confronta i valori osservati con le costanti attese; stampa il primo valore diverso e termina con ASTRO_ASSERT.
void check(const observed& values);

// Legge i valori dalle tre librerie, chiama `check` una volta per processo (le chiamate successive non fanno nulla).
// Il core la chiama prima di ogni primo uso di LibRaw (decoder RAW) e di CFITSIO (apertura e creazione FITS); ogni nuovo
// punto d'ingresso di WCSLIB deve chiamarla prima della prima funzione della libreria.
void verify_once();

// Vero dopo che `verify_once` ha completato la verifica.
bool verified() noexcept;

// Solo test: punti "third_party.libraw_version_number" e "third_party.cfitsio_version_milli" (versione 4.070 = 4070);
// un valore non nullo restituito dall'iniettore sostituisce quello letto dalla libreria, zero lascia il valore letto.
// Un iniettore vuoto toglie l'iniezione.
void set_fault_injector(fault_injector injector);

}  // namespace astro::third_party
