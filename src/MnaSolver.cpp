#include "MnaSolver.h"
#include <stdexcept>
#include <cmath>

using Complex = std::complex<double>;
static constexpr double PI = 3.14159265358979323846;

MnaSolver::MnaSolver(const Netlist& netlist)
    : netlist_(netlist),
      numNodes_(netlist.numNodes()),
      numVSources_(netlist.numVoltageSources()) {}

// Adds admittance Y between nodes a and b (either may be -1 for ground) to A.
static void stampAdmittance(Eigen::MatrixXcd& A, int a, int b, Complex Y) {
    if (a >= 0) A(a, a) += Y;
    if (b >= 0) A(b, b) += Y;
    if (a >= 0 && b >= 0) {
        A(a, b) -= Y;
        A(b, a) -= Y;
    }
}

void MnaSolver::buildSystem(double omega, Eigen::MatrixXcd& A, Eigen::VectorXcd& z) const {
    const int n = numNodes_;
    const int m = numVSources_;
    A.setZero(n + m, n + m);
    z.setZero(n + m);

    for (const auto& c : netlist_.components()) {
        // const_cast is safe here: nodeIndex() only assigns new indices for
        // names not seen during parsing, and parsing already registered every
        // node that appears in a component line.
        int p = const_cast<Netlist&>(netlist_).nodeIndex(c.nodeP);
        int nn = const_cast<Netlist&>(netlist_).nodeIndex(c.nodeN);

        switch (c.type) {
            case ComponentType::Resistor: {
                if (c.value <= 0.0) throw std::runtime_error("Resistor '" + c.name + "' must have positive resistance");
                stampAdmittance(A, p, nn, Complex(1.0 / c.value, 0.0));
                break;
            }
            case ComponentType::Capacitor: {
                Complex Y(0.0, omega * c.value); // jωC
                stampAdmittance(A, p, nn, Y);
                break;
            }
            case ComponentType::Inductor: {
                // Admittance of an ideal inductor: 1 / (jωL). At DC (omega=0)
                // this is singular, which is physically correct (short
                // circuit) -- callers should avoid omega=0 for pure AC sweeps.
                if (omega == 0.0) {
                    throw std::runtime_error("Inductor '" + c.name + "' admittance undefined at DC (omega=0)");
                }
                Complex Y = Complex(1.0, 0.0) / Complex(0.0, omega * c.value);
                stampAdmittance(A, p, nn, Y);
                break;
            }
            case ComponentType::CurrentSource: {
                Complex I = c.isAcSource
                    ? std::polar(c.value, c.acPhaseDeg * PI / 180.0)
                    : Complex(0.0, 0.0);
                // Current flows out of nodeP, into nodeN (source convention).
                if (p >= 0)  z(p)  -= I;
                if (nn >= 0) z(nn) += I;
                break;
            }
            case ComponentType::VoltageSource: {
                int br = n + c.branchIndex;
                if (p >= 0) { A(p, br) += 1.0; A(br, p) += 1.0; }
                if (nn >= 0) { A(nn, br) -= 1.0; A(br, nn) -= 1.0; }
                Complex V = c.isAcSource
                    ? std::polar(c.value, c.acPhaseDeg * PI / 180.0)
                    : Complex(0.0, 0.0);
                z(br) = V;
                break;
            }
        }
    }
}

Eigen::VectorXcd MnaSolver::solveAt(double omega) const {
    Eigen::MatrixXcd A;
    Eigen::VectorXcd z;
    buildSystem(omega, A, z);

    Eigen::PartialPivLU<Eigen::MatrixXcd> lu(A);
    // Rough conditioning check: PartialPivLU doesn't expose rank directly,
    // so we check the smallest-magnitude pivot on the diagonal of U.
    double minPivot = std::numeric_limits<double>::infinity();
    Eigen::MatrixXcd U = lu.matrixLU().triangularView<Eigen::Upper>();
    for (int i = 0; i < U.rows(); ++i) minPivot = std::min(minPivot, std::abs(U(i, i)));
    if (minPivot < 1e-14) {
        throw std::runtime_error("MNA matrix is singular or near-singular (check for floating nodes or missing ground reference)");
    }

    return lu.solve(z);
}
