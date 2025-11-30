# Demo: Geodesic Heat

![](https://github.com/davreev/demo-geodesic-heat/actions/workflows/build.yml/badge.svg)

![](https://spatialslur.com/demos/geodesic-heat-400.png)

Demo and reference implementation of [the heat method](https://www.cs.cmu.edu/~kmcrane/Projects/HeatMethod) for approximating geodesic distance on triangle meshes.

Try it here: https://davreev.gitlab.io/demos/geodesic-heat/

## Build

This project can be built to run natively or in a web browser.

### Native Build

Build via `cmake`

> ⚠️ Currently only tested with Clang and GCC. MSVC is not supported.

```sh
mkdir build
cmake -S . -B ./build -G <generator>
cmake --build ./build [--config <config>]
```

### Web Build

Download the [Emscripten SDK](https://github.com/emscripten-core/emsdk) and dot source the
provided script to initialize the Emscripten toolchain

```sh
# Bash
EMSDK_DIR="absolute/path/of/emsdk/root"
. ./emsc-init.sh

# Powershell
$EMSDK_DIR="absolute/path/of/emsdk/root"
. ./emsc-init.ps1
```

Then build via `emcmake`

```sh
mkdir build 
emcmake cmake -S . -B ./build -G <generator>
cmake --build ./build [--config <config>]
```

Output can be served locally for testing e.g.

```sh
python -m http.server
```

### Dependencies

The following build-time dependencies are expected to be installed locally:

- `slangc` (>= 2025.5.0)
- `spirv-cross` ( >= 2021.01.15)
- `spirv-tools` ( >= 2022.1)

Remaining build-time dependencies are fetched during CMake's configure step (see `cmake/deps`).
