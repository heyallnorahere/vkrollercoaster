# vkrollercoaster ![CI status](https://img.shields.io/github/workflow/status/yodasoda1219/vkrollercoaster/build)

A roller coaster in C++17 and Vulkan.

## Dependencies

- [Python 3](https://www.python.org/)
- [Vulkan](https://www.vulkan.org/)
- [CMake](https://www.cmake.org/)
- [Visual Studio 2019](https://visualstudio.microsoft.com/) (on Windows)

All other dependencies can be synced with the command `git submodule update --init --recursive`.

## Building

### Windows
```bash
# the "$(...)" expressions only work in bash
cmake . -B build -G "Visual Studio 16 2019" $(python3 scripts/cmake_options.py)
```

Launch Visual Studio with the sln in `build`. Change the default project to `vkrollercoaster` and click the "Local Windows Debugger" button at the top of the window to build and launch.

### Unix
```bash
# set this to any configuration
CONFIGURATION=Debug

# configure the project
cmake . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$CONFIGURATION $(python3 scripts/cmake_options.py)

# build
cmake --build build -j 8
```

To run, launch `build/src/vkrollercoaster` from the root directory of the project, so that it can access `assets/`.

## Contributing

If you have a contribution, feel free to submit a pull request. However, please follow the code style shown in the source code and described in [`.clang-format`](.clang-format).