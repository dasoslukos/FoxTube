# In pure Python environment, the script looks broken!
# Import from SCons.Script is already available, if the PlatformIO build environment is used to call it.
# from SCons.Script import Import

# To have access to the PIO build environment variables, we need to import the env module from SCons.Script
import os
import shutil
Import("env")

print("script_configure_tft_lib.py: Copying TFT config files...")

# Get the environment name
environmentname = env.subst("$PIOENV")

# Define target directory: prefer local modified lib, fall back to PIO libdeps
localLibDir = "lib/modified_TFT_eSPI/"
if os.path.exists(localLibDir):
    targetDir = localLibDir
else:
    targetDir = ".pio/libdeps/" + environmentname + "/TFT_eSPI/"

# Define target file
targetFile = targetDir + "User_Setup.h"

def copy_config(src, dst):
    """
    Prefer copy2 so normal filesystems preserve timestamps and avoid
    unnecessary TFT_eSPI rebuilds. Some Samba/CIFS/shared trees allow
    file writes but reject copystat()/utime(). In that case, fall back
    to copying file contents only.
    """
    try:
        return shutil.copy2(src, dst)
    except PermissionError as exc:
        print(
            "script_configure_tft_lib.py: copy2 metadata update not permitted "
            f"for {src}; falling back to copyfile ({exc})"
        )
        if os.path.isdir(dst):
            dst = os.path.join(dst, os.path.basename(src))
        return shutil.copyfile(src, dst)

ret = copy_config('./include/_USER_DEFINES.h', targetDir)
print("script_configure_tft_lib.py: Copied {ret}".format(**locals()))

ret2 = copy_config('./include/GLOBAL_DEFINES.h', targetFile)
print("script_configure_tft_lib.py: Copied {ret2}".format(**locals()))

print("script_configure_tft_lib.py: Done copying TFT config files!")
