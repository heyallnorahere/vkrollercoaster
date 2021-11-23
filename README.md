# vkrollercoaster

I'm just playing with Vulkan.

# To build install cmake and vulkan SDK

## Windows
```
cmake . -B build $(python scripts/cmake_options.py)
```

Launch Visual Studio with the sln in `build`.

Change the default project to `vkrollercoaster`. Edit the debug working directory for that project to be `$(ProjectDir)..\..`.
