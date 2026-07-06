#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <complex>
#include "Netlist.h"
#include "MnaSolver.h"

static constexpr double PI = 3.14159265358979323846;

// Generates the sweep of test frequencies (Hz) requested by the .ac directive.
static std::vector<double> generateFrequencies(const AcDirective& ac) {
    std::vector<double> freqs;
    if (ac.sweep == SweepType::Linear) {
        int n = std::max(ac.points, 2);
        for (int i = 0; i < n; ++i) {
            double f = ac.fStart + (ac.fStop - ac.fStart) * (double(i) / (n - 1));
            freqs.push_back(f);
        }
    } else {
        // Decade or octave: logarithmically spaced, `points` steps per decade/octave.
        double base = (ac.sweep == SweepType::Octave) ? 2.0 : 10.0;
        double logStart = std::log(ac.fStart) / std::log(base);
        double logStop  = std::log(ac.fStop)  / std::log(base);
        int steps = static_cast<int>(std::round((logStop - logStart) * ac.points));
        for (int i = 0; i <= steps; ++i) {
            double f = std::pow(base, logStart + double(i) / ac.points);
            freqs.push_back(f);
        }
        if (freqs.back() < ac.fStop) freqs.push_back(ac.fStop);
    }
    return freqs;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <netlist.net> [-o output.csv]\n";
        return 1;
    }

    std::string netlistPath = argv[1];
    std::string outCsvPath  = "bode_output.csv";
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "-o" && i + 1 < argc) outCsvPath = argv[++i];
    }

    try {
        Netlist netlist;
        netlist.parseFile(netlistPath);

        if (!netlist.acDirective().present) {
            std::cerr << "Error: netlist has no .ac directive (e.g. '.ac dec 20 1 1meg')\n";
            return 1;
        }

        int outNodeIdx = netlist.nodeIndex(netlist.outputNode());
        if (outNodeIdx < 0) {
            std::cerr << "Error: output node '" << netlist.outputNode() << "' is ground (0 V by definition)\n";
            return 1;
        }

        MnaSolver solver(netlist);
        auto freqs = generateFrequencies(netlist.acDirective());

        std::ofstream csv(outCsvPath);
        csv << "frequency_hz,magnitude_db,phase_deg,real,imag\n";

        std::cout << std::left
                   << std::setw(14) << "Freq (Hz)"
                   << std::setw(16) << "Mag (dB)"
                   << std::setw(14) << "Phase (deg)" << "\n";
        std::cout << std::string(44, '-') << "\n";

        for (double f : freqs) {
            double omega = 2.0 * PI * f;
            Eigen::VectorXcd x = solver.solveAt(omega);
            std::complex<double> vOut = x(outNodeIdx);

            double mag = std::abs(vOut);
            double magDb = 20.0 * std::log10(mag > 0 ? mag : 1e-300);
            double phaseDeg = std::arg(vOut) * 180.0 / PI;

            csv << f << "," << magDb << "," << phaseDeg << ","
                << vOut.real() << "," << vOut.imag() << "\n";

            std::cout << std::left << std::setw(14) << f
                       << std::setw(16) << magDb
                       << std::setw(14) << phaseDeg << "\n";
        }

        std::cout << "\nWrote " << freqs.size() << " points to " << outCsvPath << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
