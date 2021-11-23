# vkrollercoaster ![CI status](https://img.shields.io/github/workflow/status/yodasoda1219/vkrollercoaster/build)

A roller coaster in C++17 and Vulkan.

## Dependencies

- [Vulkan](https://www.vulkan.org/)
- [CMake](https://www.cmake.org/)
- [Visual Studio 2019](https://visualstudio.microsoft.com/) (on Windows)

## Building

### Windows
```bash
cmake . -B build $(python3 scripts/cmake_options.py)
```

Launch Visual Studio with the sln in `build`.

Change the default project to `vkrollercoaster`. Edit the debug working directory for that project to be `$(ProjectDir)..\..`.

### Unix
```bash
# set this to any configuration
CONFIGURATION=Debug

# configure the project
cmake . -B build -DCMAKE_BUILD_TYPE=$CONFIGURATION $(python3 scripts/cmake_options.py)

# build
cmake --build build -j 8
```

To run, launch `build/src/vkrollercoaster` from the root directory of the project, so that it can access `assets/`.