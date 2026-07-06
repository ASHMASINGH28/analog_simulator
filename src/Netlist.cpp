#include "Netlist.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstdlib>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool Netlist::isGround(const std::string& n) {
    std::string l = toLower(n);
    return l == "0" || l == "gnd" || l == "ground";
}

// Parses SPICE-style engineering suffixes: T G MEG K M U N P F (case-insensitive).
// "MEG" must be checked before a bare "M" since both start with 'm'.
double Netlist::parseEngValue(const std::string& tok) {
    char* end = nullptr;
    double mantissa = std::strtod(tok.c_str(), &end);
    if (end == tok.c_str()) {
        throw std::runtime_error("Cannot parse numeric value: '" + tok + "'");
    }
    std::string suffix = toLower(std::string(end));
    // strip any trailing unit letters (ohm, f, h, v, a...) after the multiplier itself
    if (suffix.rfind("meg", 0) == 0) return mantissa * 1e6;
    if (!suffix.empty()) {
        switch (suffix[0]) {
            case 't': return mantissa * 1e12;
            case 'g': return mantissa * 1e9;
            case 'k': return mantissa * 1e3;
            case 'm': return mantissa * 1e-3;
            case 'u': return mantissa * 1e-6;
            case 'n': return mantissa * 1e-9;
            case 'p': return mantissa * 1e-12;
            case 'f': return mantissa * 1e-15;
            default:  return mantissa; // unrecognized suffix (e.g. plain "ohm") -> treat as x1
        }
    }
    return mantissa;
}

int Netlist::nodeIndex(const std::string& name) {
    if (isGround(name)) return -1;
    auto it = nodeIndexMap_.find(name);
    if (it != nodeIndexMap_.end()) return it->second;
    int idx = static_cast<int>(nodeIndexMap_.size());
    nodeIndexMap_[name] = idx;
    indexToName_.push_back(name);
    return idx;
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> toks;
    std::istringstream iss(line);
    std::string t;
    while (iss >> t) toks.push_back(t);
    return toks;
}

void Netlist::parseComponentLine(const std::vector<std::string>& tok) {
    const std::string& name = tok[0];
    char id = std::toupper(name[0]);

    if (tok.size() < 4) {
        throw std::runtime_error("Malformed component line for '" + name + "'");
    }

    Component c;
    c.name  = name;
    c.nodeP = tok[1];
    c.nodeN = tok[2];

    // Register nodes so they appear in the unknown map even if only referenced here.
    nodeIndex(c.nodeP);
    nodeIndex(c.nodeN);

    switch (id) {
        case 'R': c.type = ComponentType::Resistor;  c.value = parseEngValue(tok[3]); break;
        case 'C': c.type = ComponentType::Capacitor;  c.value = parseEngValue(tok[3]); break;
        case 'L': c.type = ComponentType::Inductor;   c.value = parseEngValue(tok[3]); break;
        case 'V':
        case 'I': {
            c.type = (id == 'V') ? ComponentType::VoltageSource : ComponentType::CurrentSource;
            // Forms supported:
            //   V1 1 0 5              (DC only)
            //   V1 1 0 DC 5
            //   V1 1 0 AC 1           (AC magnitude, 0 deg phase)
            //   V1 1 0 AC 1 90        (AC magnitude + phase in degrees)
            size_t i = 3;
            if (i < tok.size() && toLower(tok[i]) == "dc") i++;
            if (i < tok.size() && toLower(tok[i]) == "ac") {
                c.isAcSource = true;
                i++;
                if (i < tok.size()) c.value = parseEngValue(tok[i++]);
                if (i < tok.size()) c.acPhaseDeg = parseEngValue(tok[i++]);
            } else if (i < tok.size()) {
                // Plain DC value with no AC stimulus -> contributes 0 to AC analysis.
                c.value = parseEngValue(tok[i++]);
                c.isAcSource = false;
            }
            if (c.type == ComponentType::VoltageSource) {
                c.branchIndex = numVSources_++;
            }
            break;
        }
        default:
            throw std::runtime_error("Unsupported component prefix '" + std::string(1, id) + "' in '" + name + "'");
    }
    components_.push_back(c);
}

void Netlist::parseDirectiveLine(const std::vector<std::string>& tok) {
    std::string d = toLower(tok[0]);
    if (d == ".ac") {
        // .ac dec <points> <fstart> <fstop>
        // .ac lin <points> <fstart> <fstop>
        if (tok.size() < 5) throw std::runtime_error(".ac directive needs: .ac <dec|lin|oct> <points> <fstart> <fstop>");
        std::string sweepStr = toLower(tok[1]);
        ac_.present = true;
        if (sweepStr == "dec") ac_.sweep = SweepType::Decade;
        else if (sweepStr == "lin") ac_.sweep = SweepType::Linear;
        else if (sweepStr == "oct") ac_.sweep = SweepType::Octave;
        else throw std::runtime_error("Unknown sweep type '" + tok[1] + "'");
        ac_.points = static_cast<int>(parseEngValue(tok[2]));
        ac_.fStart = parseEngValue(tok[3]);
        ac_.fStop  = parseEngValue(tok[4]);
    } else if (d == ".plot" || d == ".print") {
        // .plot ac v(2)   -> capture output node "2"
        for (const auto& t : tok) {
            std::string lt = toLower(t);
            if (lt.rfind("v(", 0) == 0) {
                outputNodeName_ = t.substr(lt.find('(') + 1);
                if (!outputNodeName_.empty() && outputNodeName_.back() == ')')
                    outputNodeName_.pop_back();
            }
        }
    }
    // Other directives (.end, .title, etc.) are silently ignored.
}

void Netlist::parseFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open netlist file: " + path);

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        // Strip inline comments starting with '*' or ';'
        auto commentPos = line.find_first_of("*;");
        if (commentPos != std::string::npos) line = line.substr(0, commentPos);

        auto tok = tokenize(line);
        if (tok.empty()) continue;

        try {
            if (tok[0][0] == '.') {
                parseDirectiveLine(tok);
            } else {
                parseComponentLine(tok);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Line " + std::to_string(lineNo) + ": " + e.what());
        }
    }

    if (outputNodeName_.empty()) {
        throw std::runtime_error("No output node specified. Add a line like: .plot ac v(<node>)");
    }
}
