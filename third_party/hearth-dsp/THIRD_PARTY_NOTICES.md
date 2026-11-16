# Third-party notices

hearth-dsp is GPL-3.0-or-later. It was extracted from Comp Suite 1.0.0.

## Algorithms and prior art

- **RBJ audio EQ cookbook** — the `Biquad` coefficient formulas are the
  standard Robert Bristow-Johnson cookbook equations.
- **ADAA** — first-order antiderivative anti-aliasing follows the method
  described by Parker, Zavalishin and Le Bivic (2016), "Reducing the aliasing
  of non-linear discrete-time systems by first-order antiderivative
  anti-aliasing". The implementation is original.
- **Kaiser window / half-band FIR** — the oversampler coefficients were
  generated with a Kaiser window (beta 8) and normalised to unity DC.
- **Catmull-Rom interpolation** — used by the true-peak detector.
- **NC76 reference law** — the shared `(ratio−1)/ratio` dynamics and the 76
  calibration in Comp Suite were matched against Tukan's MIT-licensed NC76
  reference. No JSFX code is copied into this library.

No third-party source code is included. If that changes, add the licence and
attribution here.
