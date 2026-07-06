#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "Component.h"

enum class SweepType { Decade, Linear, Octave };

// Parsed ".ac" analysis directive.
struct AcDirective {
    bool       present = false;
    SweepType  sweep   = SweepType::Decade;
    int        points  = 10;   // points per decade/octave, or total points for lin
    double     fStart  = 1.0;
    double     fStop   = 1e6;
};

// Parses a SPICE-like netlist and builds a fast node-name -> index lookup
// (unordered_map gives O(1) average lookup regardless of node density/naming).
class Netlist {
public:
    // Parses the file at `path`. Throws std::runtime_error on malformed input.
    void parseFile(const std::string& path);

    const std::vector<Component>& components() const { return components_; }
    const AcDirective& acDirective() const { return ac_; }
    const std::string& outputNode() const { return outputNodeName_; }

    // Ground is always node 0 and is excluded from the unknown-index map.
    // Returns the 0-based unknown index for a node name (creates it if unseen,
    // unless it's the ground node "0" / "gnd", which always maps to -1).
    int nodeIndex(const std::string& name);

    int numNodes() const { return static_cast<int>(nodeIndexMap_.size()); }
    int numVoltageSources() const { return numVSources_; }

    // Human readable name for reporting, e.g. index 2 -> "3" if that's how the
    // user named it in the netlist.
    const std::string& nodeName(int idx) const { return indexToName_.at(idx); }

private:
    std::vector<Component> components_;
    AcDirective ac_;
    std::string outputNodeName_;

    std::unordered_map<std::string, int> nodeIndexMap_; // name -> 0-based index (ground excluded)
    std::vector<std::string> indexToName_;               // index -> name
    int numVSources_ = 0;

    static bool isGround(const std::string& n);
    static double parseEngValue(const std::string& tok); // handles k, m, u, n, p, meg, etc.
    void parseComponentLine(const std::vector<std::string>& tok);
    void parseDirectiveLine(const std::vector<std::string>& tok);
};
