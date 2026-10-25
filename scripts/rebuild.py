"""
Recreate and build the Vitis workspace from what is tracked in git.

Run with the Vitis 2025.2 Python CLI (from the repo root):

    vitis -s scripts/rebuild.py                 # builds into ./_rebuild_ws
    vitis -s scripts/rebuild.py C:/some/new_ws  # builds into a folder you pick

It creates the platform from the committed .xsa, builds it (BSP + FSBL),
creates the app from the empty_application template, copies in the
tracked sources from CTU_CAN_FD_ILA_Firmware/src, and builds the .elf.

The target workspace must not already contain these components, so it
never touches the workspace you are working in.
"""
import os
import re
import shutil
import sys

import vitis

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

PLATFORM = "CTU_CAN_FD_ILA"
APP = "CTU_CAN_FD_ILA_Firmware"
XSA = os.path.join(REPO, PLATFORM, "hw", "CAN_System_wrapper_ILA.xsa")
CPU = "ps7_cortexa9_0"
DOMAIN = "standalone_ps7_cortexa9_0"
APP_SRC = os.path.join(REPO, APP, "src")

# Files Vitis generates for the template; everything else in src/ is ours.
SKIP = {"compile_commands.json", ".clangd", ".cache", ".compile_commands"}
SOURCE_EXT = (".c", ".cpp", ".cc", ".S", ".s")


def main():
    ws = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "_rebuild_ws"))
    for name in (PLATFORM, APP):
        if os.path.exists(os.path.join(ws, name)):
            sys.exit(f"{ws} already has {name}; pick an empty workspace folder.")
    os.makedirs(ws, exist_ok=True)
    print(f"Workspace: {ws}")

    client = vitis.create_client()
    try:
        client.set_workspace(path=ws)

        # 1) Platform from the committed .xsa (also creates the zynq_fsbl boot domain)
        platform = client.create_platform_component(
            name=PLATFORM,
            hw_design=XSA,
            os="standalone",
            cpu=CPU,
            domain_name=DOMAIN,
        )
        platform.build()
        xpfm = os.path.join(ws, PLATFORM, "export", PLATFORM, f"{PLATFORM}.xpfm")

        # 2) App from an empty template, then overlay our tracked sources
        app = client.create_app_component(
            name=APP,
            platform=xpfm,
            domain=DOMAIN,
            template="empty_application",
        )
        app_src = os.path.join(ws, APP, "src")
        for entry in os.listdir(APP_SRC):
            if entry in SKIP:
                continue
            src = os.path.join(APP_SRC, entry)
            dst = os.path.join(app_src, entry)
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copy2(src, dst)

        sync_source_list(app_src)
        app.build()
        print(f"\nDone. ELF: {os.path.join(ws, APP, 'build', APP + '.elf')}")
    finally:
        vitis.dispose()


def sync_source_list(app_src):
    """Point USER_COMPILE_SOURCES at the source files actually present,
    so a renamed .c file (e.g. helloworld.c -> ILA_Firmware.c) still builds."""
    cfg = os.path.join(app_src, "UserConfig.cmake")
    if not os.path.exists(cfg):
        return
    files = sorted(f for f in os.listdir(app_src) if f.endswith(SOURCE_EXT))
    body = "\n".join(f'"{f}"' for f in files)
    with open(cfg, encoding="utf-8") as fh:
        text = fh.read()
    text = re.sub(
        r"set\(USER_COMPILE_SOURCES\s*\n.*?\n\)",
        lambda _: f"set(USER_COMPILE_SOURCES\n{body}\n)",
        text,
        count=1,
        flags=re.S,
    )
    with open(cfg, "w", encoding="utf-8") as fh:
        fh.write(text)
    print("USER_COMPILE_SOURCES:", ", ".join(files))


main()
