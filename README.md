# Linear Analog Circuit Simulator (C++ / MNA)

A command-line EDA tool that parses SPICE-style netlists and computes the
**AC frequency response** (Bode magnitude + phase) of linear analog circuits
using **Modified Nodal Analysis (MNA)**. Built with Eigen for complex linear
algebra and validated against closed-form filter theory.

## Features

- Parses a custom SPICE-like netlist format: `R`, `C`, `L`, `V` (voltage
  source), `I` (current source), engineering-notation values (`1k`, `159.2n`,
  `10m`, `1meg`, ...).
- Builds the MNA admittance matrix programmatically: resistors stamp `1/R`,
  capacitors stamp `jωC`, inductors stamp `1/(jωL)`, independent sources get
  their own branch-current unknown (voltage sources) or inject current
  directly (current sources).
- Solves the complex linear system at each frequency point with Eigen's
  `PartialPivLU` decomposition, with a pivot-magnitude check that flags
  singular systems (floating nodes, missing ground reference, etc.)
  instead of silently returning garbage.
- Sweeps frequency (`.ac dec|lin|oct <points> <fstart> <fstop>`) and reports
  magnitude (dB) and phase (deg) at every point, both to stdout and to CSV.
- Node names are arbitrary strings (not just integers) and are resolved
  through an `unordered_map`, giving O(1) average-case lookup independent of
  how sparse or dense the node numbering is.
- A Python helper (`scripts/plot_bode.py`) turns the CSV into an actual Bode
  plot (magnitude + phase, log-frequency axis) with matplotlib.

## Building

Requires a C++17 compiler, CMake, and Eigen3.

```bash
sudo apt-get install libeigen3-dev cmake   # if not already installed
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

This produces the `mna_sim` executable in `build/`.

## Running

```bash
./mna_sim <netlist.net> [-o output.csv]
```

Example:

```bash
./mna_sim ../examples/rc_lowpass.net -o rc_out.csv
python3 ../scripts/plot_bode.py rc_out.csv rc_bode.png "RC Low-pass Filter"
```

## Netlist format

```
* Comments start with * or ;
V1 1 0 AC 1 0        ; AC source, node1 -> node0(gnd), 1V magnitude, 0 deg phase
R1 1 2 1k             ; 1 kOhm resistor between node 1 and node 2
C1 2 0 159.2n         ; 159.2 nF capacitor to ground
L1 2 3 10m            ; 10 mH inductor
I1 2 0 AC 1m 90       ; 1 mA current source, 90 deg phase

.ac dec 20 1 1meg     ; sweep 1 Hz to 1 MHz, 20 points/decade (dec|lin|oct)
.plot ac v(2)         ; report the output node whose voltage to track
```

Supported engineering suffixes (case-insensitive): `T G MEG K M U N P F`.

## Validation

Two examples are included and check out against theory:

| Circuit | Analytic result | Simulated result |
|---|---|---|
| RC low-pass, fc = 1/(2πRC) ≈ 1000 Hz | -3.00 dB, -45.0° at fc | **-3.01 dB, -45.0°** at 1000 Hz |
| Series RLC low-pass, f0 = 1/(2π√LC) ≈ 5033 Hz, Q ≈ 3.16 | peak gain = 20·log10(Q) ≈ 10.00 dB | **10.03 dB** at 5012 Hz |

(See `examples/rc_lowpass.net`, `examples/rlc_lowpass.net`.)

## Architecture

```
include/
  Component.h    - component data model (R, C, L, V, I)
  Netlist.h      - parser interface, node-name -> index map
  MnaSolver.h    - MNA matrix builder + complex LU solver
src/
  Netlist.cpp    - netlist parsing, engineering-notation values
  MnaSolver.cpp  - admittance stamping, linear solve
  main.cpp       - CLI: frequency sweep loop, CSV/stdout reporting
scripts/
  plot_bode.py   - CSV -> Bode plot (magnitude + phase) with matplotlib
examples/
  rc_lowpass.net
  rlc_lowpass.net
```

## Known limitations / possible extensions

- Only linear elements are supported (no diodes, transistors, or op-amps) —
  by design, this is a *linear* AC analysis tool, not a general SPICE clone.
- No DC operating-point or transient analysis; AC small-signal only.
- Inductor stamping is undefined at exactly ω = 0 (DC), which is physically
  correct (an ideal inductor is a short at DC) but means `.ac` sweeps should
  not start at 0 Hz.
- Natural extensions: dependent sources (VCVS/VCCS/CCVS/CCCS) for op-amp
  modeling, mutual inductance (transformers), Monte Carlo tolerance sweeps,
  and a proper `.subckt` mechanism for hierarchical netlists.
