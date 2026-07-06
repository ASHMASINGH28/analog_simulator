#pragma once
#include <string>
#include <complex>

// Supported primitive component types.
enum class ComponentType {
    Resistor,
    Capacitor,
    Inductor,
    VoltageSource,
    CurrentSource
};

// A single parsed circuit element (one line of the netlist).
struct Component {
    std::string   name;      // e.g. "R1", "C2", "V1"
    ComponentType type;
    std::string   nodeP;     // positive / first terminal node name
    std::string   nodeN;     // negative / second terminal node name
    double        value = 0.0; // ohms / farads / henries / volts / amps (DC or AC magnitude)
    double        acPhaseDeg = 0.0; // phase in degrees, only meaningful for AC sources
    bool          isAcSource = false; // true if this source has an "AC <mag> [phase]" spec

    // Index of this source's branch current unknown in the MNA system.
    // Only used for VoltageSource elements (-1 = unassigned).
    int branchIndex = -1;
};
