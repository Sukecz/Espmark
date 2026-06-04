# Contributing to espmark

For early development, contributions should keep tests small, deterministic and
capability-aware. A board that cannot run a lane should skip it explicitly
rather than reporting a fake zero.

Result submissions should include:

- board name, vendor, module and revision
- SoC target
- Arduino IDE version and installed `esp32` board package version
- CPU frequency
- flash size and mode when known
- raw UART log or JSON bundle
