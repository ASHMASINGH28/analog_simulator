#pragma once
#include <Eigen/Dense>
#include "Netlist.h"

// Builds and solves the Modified Nodal Analysis system for a given netlist
// at one or more angular frequencies, using complex admittance stamps.
//
// System layout: unknowns = [node voltages (0..numNodes-1), branch currents
// through voltage sources (0..numVSources-1)]. Ground is not an unknown.
class MnaSolver {
public:
    explicit MnaSolver(const Netlist& netlist);

    // Solves the linear system A*x = z at angular frequency omega (rad/s).
    // Returns the full unknown vector (node voltages followed by source
    // branch currents). Throws std::runtime_error if the matrix is singular.
    Eigen::VectorXcd solveAt(double omega) const;

    int systemSize() const { return numNodes_ + numVSources_; }
    int numNodes() const { return numNodes_; }

private:
    const Netlist& netlist_;
    int numNodes_;
    int numVSources_;

    // Fills matrix A and RHS vector z with the stamps for every component.
    void buildSystem(double omega, Eigen::MatrixXcd& A, Eigen::VectorXcd& z) const;
};
