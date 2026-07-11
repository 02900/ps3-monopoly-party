# Vendored engine — attribution

`source/engine/` is the Monopoly rules engine from
**[jemeador/monopoly](https://github.com/jemeador/monopoly)** (MIT License,
Copyright (c) 2021 Jeremy Meador), reused essentially verbatim.

PS3-port changes are minimal and localized:
- `EmscriptenBindings.cpp` dropped (web-only).
- `Game.h`: removed unused `<thread>/<mutex>/<future>/<condition_variable>`
  includes (the engine is single-threaded; PSL1GHT has no std::thread runtime).
- `DisplayStrings.cpp`: parses the string table baked into `strings_data.h`
  instead of reading `share/strings.json` from disk (PS3 has no host path).
- `newlib_compat.h`: force-included shim supplying `std::to_string` and pulling
  the C stdio/stdlib conversion functions into `std::` (newlib gaps), so the
  engine and the vendored `nlohmann/json.hpp` build under ppu-gcc.

`nlohmann/json.hpp` is the single-header build of
[nlohmann/json](https://github.com/nlohmann/json) v3.11.3 (MIT).
