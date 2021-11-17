#!/usr/bin/env python3
OPTIONS = {
    "SPIRV_CROSS_ENABLE_TESTS": "OFF",
    "SPIRV_CROSS_CLI": "OFF",
    "SPIRV_CROSS_ENABLE_C_API": "OFF",
    "SPIRV_CROSS_ENABLE_HLSL": "OFF",
    "SPIRV_CROSS_ENABLE_MSL": "OFF",
    "SPIRV_CROSS_ENABLE_CPP": "OFF",
    "SHADERC_SKIP_TESTS": "ON",
    "SHADERC_SKIP_INSTALL": "ON",
    "SHADERC_SKIP_EXAMPLES": "ON",
    "SHADERC_SKIP_COPYRIGHT_CHECK": "ON",
    "SHADERC_ENABLE_SHARED_CRT": "ON",
    "SKIP_SPIRV_TOOLS_INSTALL": "ON",
    "SPIRV_SKIP_EXECUTABLES": "ON",
    "SPIRV_SKIP_TESTS": "ON",
    "GLFW_BUILD_EXAMPLES": "OFF",
    "GLFW_BUILD_TESTS": "OFF",
    "GLFW_BUILD_DOCS": "OFF",
    "GLFW_INSTALL": "OFF",
    "ENABLE_CTEST": "OFF",
}
def main():
    option_string = "-Wno-dev"
    for name in OPTIONS.keys():
        value = OPTIONS[name]
        option_string += f" -D{name}={value}"
    print(option_string)
if __name__ == "__main__":
    main()