#pragma once

namespace dspugen::terrain {

// Bit-exact reimplementations of the sinf / cosf that UnityPlayer.dll calls.
//
// Unity's Quaternion.AngleAxis is engine native code and calls the Microsoft
// UCRT's sinf and cosf, which are polynomial approximations rather than
// correctly rounded functions: they land 1 ULP away from the true result often
// enough to move a birth planet's whole vein layout.  std::sin / std::sinf do
// not reproduce them, so the UCRT routines are reimplemented here from the
// disassembly of UnityPlayer.dll (see docs/vein-amount-parity-evidence.md for
// the addresses and constants).
//
// These are only for reproducing the engine's own arithmetic.  Do not use them
// as general-purpose trigonometry.
float ucrtSinf(float value);
float ucrtCosf(float value);

}
