# PRS - Production Rule Set Library

This library provides a comprehensive framework for the modeling, manipulation, and simulation of asynchronous digital circuits using Production Rule Sets (PRS). It bridges the gap between abstract logical descriptions and physical transistor-level implementations, making it a powerful tool for asynchronous circuit design, analysis, and verification.

## Dependencies

The PRS library depends on the following components:

- **sch**: Schematic representation for circuit elements
- **phy**: Physical layout and technology parameters
- **boolean**: Boolean algebra and expression handling
- **interpret_boolean**: Boolean expression interpreter
- **parse_expression**: Expression parser for logical formulas
- **parse_ucs**: Parser for UCS (Unified Circuit Specification)
- **parse**: General parsing utilities
- **common**: Common data structures and utility functions

For testing, additional dependencies include:
- **interpret_prs**: PRS interpreter 
- **parse_prs**: PRS format parser
- **parse_spice**: SPICE netlist parser
- **parse_dot**: DOT graph format parser
- **ucs**: Unified Circuit Specification library

## Build and Installation

### Requirements

- C++17 compatible compiler (g++, clang++)
- Make build system
- All dependency libraries must be available in the parent directory (`../`)

### Building the Library

To build the library, simply run:

```bash
make
```

This will compile the library and generate `libprs.a` in the current directory.

### Building and Running Tests

To build and run the test suite (requires Google Test framework):

```bash
make tests
./test
```

The test binary will verify all library functionality.

### Cleaning the Build

To clean up build artifacts:

```bash
make clean       # Clean all build artifacts
make cleantest   # Clean only test-related artifacts
```

### Linking with Your Project

To use the PRS library in your project:

1. Include the header files in your source code:
   ```cpp
   #include <prs/production_rule.h>
   #include <prs/simulator.h>
   // ... other headers as needed
   ```

2. Add the library to your compiler/linker flags:
   ```
   -I/path/to/prs -L/path/to/prs -lprs
   ```

3. Make sure all dependencies are also linked:
   ```
   -lsch -lphy -lboolean -linterpret_boolean -lparse_expression -lparse_ucs -lparse -lcommon
   ```

## Core Components

### Production Rule Set (`production_rule_set`)

The foundational data structure representing an asynchronous circuit as a collection of:

- **Nets**: Signal wires with properties including names, regions, and connection information
- **Devices**: Transistors connecting nets (source, gate, drain) with properties like threshold and driver values
- **Attributes**: Device properties such as strength (weak/strong), delay characteristics, and sizing information

Key operations include:
- Building circuits by adding devices and connecting nets
- Manipulating circuit topology (connecting, replacing, inverting nets)
- Adding standard structures (inverters, buffers, keepers)
- Circuit verification and validation
- Device sizing and optimization

### Circuit Simulator (`simulator`)

Event-driven simulator for PRS circuits featuring:

- **State Tracking**: Maintains both instantaneous and target circuit states
- **Event Scheduling**: Uses calendar queue for efficient time-ordered event processing
- **Signal Resolution**: Handles conflicts based on signal strengths (power, normal, weak, floating)
- **Signal Propagation**: Accurate modeling of transitions through combinational logic

The simulator supports:
- Setting input values and observing output responses
- Fine-grained control over simulation timing
- Reset and initialization procedures
- Handling of signal interference and stability

### Bubble Reshuffling (`bubble`)

Implementation of the bubble reshuffling algorithm for signal polarity optimization:

- Constructs a graph representation of signal dependencies
- Identifies cycles and isochronic forks in asynchronous circuits
- Optimizes signal polarities to push inversions off isochronic forks
- Makes circuits CMOS-implementable by strategic placement of inverters

### Synthesis and Translation (`synthesize`)

Tools to convert between logical descriptions and physical implementations:

- `build_netlist`: Converts PRS to SPICE netlists with appropriate transistor sizing
- `extract_rules`: Derives production rules from transistor-level netlists for analysis and verification

### Event Scheduling (`calendar_queue`)

Efficient priority queue implementation optimized for discrete event simulation:

- Hierarchical bucket structure for O(1) average case operations
- Adaptive bucket sizing based on event distribution
- Supports event rescheduling and cancellation

## Usage Examples

### Building a Circuit
```cpp
// Create a new production rule set
prs::production_rule_set circuit;

// Define nets
int a = circuit.netIndex("a", 0, true);
int b = circuit.netIndex("b", 0, true);
int c = circuit.netIndex("c", 0, true);

// Define power nets
int vdd = circuit.netIndex("Vdd", 0, true);
int gnd = circuit.netIndex("GND", 0, true);
circuit.set_power(vdd, gnd);

// Create an inverter (a→b)
circuit.add_inverter_between(a, b);

// Create a NAND gate (b&c→d)
boolean::cube guard;
guard.set_var(b, 1);
guard.set_var(c, 1);
int d = circuit.netIndex("d", 0, true);
circuit.add(guard, d, 0);  // b&c → d-
```

### Simulating a Circuit
```cpp
// Create simulator with the circuit
prs::simulator sim(&circuit);

// Reset to initial state
sim.reset();

// Set input and run simulation
sim.set(circuit.netIndex("a"), 1);  // Set 'a' high
while(!sim.enabled.empty()) {
    sim.fire();  // Process next event
}

// Check output value
int output_net = circuit.netIndex("d");
int value = sim.encoding.get_val(output_net);
```

### Circuit Transformations
```cpp
// Perform bubble reshuffling to optimize signal polarities
prs::bubble optimizer;
optimizer.load_prs(circuit);
optimizer.reshuffle();
optimizer.save_prs(&circuit);

// Add keeper circuits to maintain state
circuit.add_keepers();

// Size devices based on stack length
circuit.size_devices();
```

## Known Limitations

The PRS library has several known limitations that users should be aware of:

1. **Dividing Signals**: The bubble reshuffling algorithm cannot handle dividing signals (same signal driving multiple outputs with conflicting polarities). These create isochronic cycles with bubbles that cannot be resolved.

2. **Gating Signals**: Signals used in contradictory ways within the same gate (both active-high and active-low) cannot be properly handled by the bubble reshuffling algorithm.

3. **Non-CMOS-Implementable Circuits**: Some circuits with complex feedback structures may not be CMOS-implementable regardless of bubble reshuffling.

### Validation Controls

The library offers several validation settings (disabled by default) that can be enabled to detect potential issues:

```cpp
// Enable to require all nodes to have defined drivers
circuit.require_driven = true;  

// Enable to detect glitches/hazards
circuit.require_stable = true;  

// Enable to prohibit Vdd to GND shorts
circuit.require_noninterfering = true;  

// Enable to detect non-adiabatic transitions
circuit.require_adiabatic = true;  
```

### Behavior Configuration

The library also provides behavior settings that can affect circuit analysis:

```cpp
// Enable to prevent NMOS from driving weak 1 and PMOS from driving weak 0
circuit.assume_nobackflow = true;  

// Enable to hold value at all named nodes (staticizers)
circuit.assume_static = true;  
```

## License

Licensed by Broccoli, LLC under GNU GPL v3.

Written by Ned Bingham.
Copyright © 2020 Broccoli, LLC.

Haystack is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Haystack is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License may be found in COPYRIGHT.
Otherwise, see <https://www.gnu.org/licenses/>.
