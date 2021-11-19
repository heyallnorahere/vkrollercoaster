from os import mkdir
from requests import get
from sys import platform, argv
from subprocess import call
import os.path as path
def main():
    if platform != "win32":
        print("This script must only be run on Windows!")
        return 1
    vulkan_sdk_version = "1.2.182.0"
    installer_url = f"https://sdk.lunarg.com/sdk/download/{vulkan_sdk_version}/windows/VulkanSDK-{vulkan_sdk_version}-Installer.exe"
    response = get(installer_url, allow_redirects=True)
    mkdir("build")
    installer_path = path.join("build", "vulkan_installer.exe")
    with open(installer_path, "wb") as stream:
        stream.write(response.content)
        stream.close()
    return_code = call([ installer_path, "/S" ], shell=True)
    if return_code != 0:
        return 2
    if len(argv) > 1:
        if argv[1] == "--gh-actions":
            print(f"::set-env name=VULKAN_SDK::C:\\VulkanSDK\\{vulkan_sdk_version}")
    return 0
if __name__ == "__main__":
    exit(main())