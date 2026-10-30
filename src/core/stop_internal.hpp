#pragma once

#include <exception>

#include "astro/error.hpp"

// Registrazione degli arresti condivisa dal runner dei job e dal workflow (docs/contracts/BASE-confini.md, OB-01, OB-03).
namespace astro::stopint {

// Senza allocazioni: il puntatore resta valido finche' `ep` e' vivo.
const char* what_of(const std::exception_ptr& ep) noexcept;

// Causa registrata per un arresto: lo stop_error stesso, oppure integrity job.unexpected_exception.
stop_error cause_of(const std::exception_ptr& ep);

// Rilancia `original` dopo una registrazione fallita: uno stop_error con stessa categoria, codice, messaggio e contesto
// piu' `registrazione_arresto` con l'errore secondario; ogni altra eccezione invariata.
[[noreturn]] void rethrow_with_record_failure(const std::exception_ptr& original, const char* secondary);

}  // namespace astro::stopint
