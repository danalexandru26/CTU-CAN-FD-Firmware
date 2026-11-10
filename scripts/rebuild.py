"""
Recreate and build the Vitis workspace from what is tracked in git.

Run with the Vitis 2025.2 Python CLI (from the repo root):

    vitis -s scripts/rebuild.py                      # dual-node setup into ./_rebuild_ws
    vitis -s scripts/rebuild.py single               # single-node ILA setup
    vitis -s scripts/rebuild.py all C:/some/new_ws   # both setups, into a folder you pick

For each selected setup it creates the platform from the committed .xsa,
builds it (BSP + FSBL), creates the app, copies in the tracked sources
(including CMakeLists.txt with the shared-driver block), and builds the .elf.
The shared driver/ folder is copied into the workspace first, because the
apps include it as ../../driver.

The target workspace must not already contain these components, so it
never touches the workspace you are working in.
"""
import os
import shutil
import sys

import vitis

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

CPU = "ps7_cortexa9_0"
DOMAIN = "standalone_ps7_cortexa9_0"
TEMPLATE = "hello_world"

# setup name -> (platform, xsa file in <platform>/hw/, app)
SETUPS = {
    "dual": ("CTU_CAN_FD_Double_Node_Wrapper",
             "CAN_Double_Node_System_wrapper.xsa",
             "CTU_CAN_FD_DUAL_NODE_FIRMWARE"),
    "single": ("CTU_CAN_FD_LOOPBACK_2TX_ILA131072",
               "CAN_System_wrapper.xsa",
               "CTU_CAN_FD_ILA_Firmware"),
}
DEFAULT_SETUP = "dual"

# Files Vitis generates; everything else in src/ is ours.
SKIP = {"compile_commands.json", ".clangd", ".cache", ".compile_commands"}


def parse_args(argv):
    setups, ws = [], None
    for arg in argv:
        if arg == "all":
            setups = list(SETUPS)
        elif arg in SETUPS:
            setups.append(arg)
        elif ws is None:
            ws = arg
        else:
            sys.exit(f"Unexpected argument: {arg}\nUsage: rebuild.py [dual|single|all] [workspace_dir]")
    setups = setups or [DEFAULT_SETUP]
    ws = os.path.abspath(ws or os.path.join(REPO, "_rebuild_ws"))
    return list(dict.fromkeys(setups)), ws


def main():
    setups, ws = parse_args(sys.argv[1:])

    for setup in setups:
        platform, _, app = SETUPS[setup]
        for name in (platform, app):
            if os.path.exists(os.path.join(ws, name)):
                sys.exit(f"{ws} already has {name}; pick an empty workspace folder.")
    os.makedirs(ws, exist_ok=True)
    print(f"Workspace: {ws}")
    print(f"Setups:    {', '.join(setups)}")

    # Shared driver, referenced by each app's CMakeLists.txt as ../../driver
    shutil.copytree(os.path.join(REPO, "driver"), os.path.join(ws, "driver"), dirs_exist_ok=True)

    client = vitis.create_client()
    elfs = []
    try:
        client.set_workspace(path=ws)
        for setup in setups:
            elfs.append(build_setup(client, ws, *SETUPS[setup]))
    finally:
        vitis.dispose()

    print("\nDone.")
    for elf in elfs:
        print(f"  ELF: {elf}")


def build_setup(client, ws, platform_name, xsa_file, app_name):
    print(f"\n=== {platform_name} + {app_name} ===")

    # 1) Platform from the committed .xsa (also creates the zynq_fsbl boot domain)
    platform = client.create_platform_component(
        name=platform_name,
        hw_design=os.path.join(REPO, platform_name, "hw", xsa_file),
        os="standalone",
        cpu=CPU,
        domain_name=DOMAIN,
    )
    platform.build()
    xpfm = os.path.join(ws, platform_name, "export", platform_name, f"{platform_name}.xpfm")

    # 2) App from the template, then replace its sources with our tracked ones
    app = client.create_app_component(
        name=app_name,
        platform=xpfm,
        domain=DOMAIN,
        template=TEMPLATE,
    )
    repo_src = os.path.join(REPO, app_name, "src")
    app_src = os.path.join(ws, app_name, "src")
    ours = {e for e in os.listdir(repo_src) if e not in SKIP}

    # Drop template sources we don't ship (e.g. helloworld.c)
    for entry in os.listdir(app_src):
        if entry.endswith((".c", ".cpp")) and entry not in ours:
            os.remove(os.path.join(app_src, entry))

    for entry in ours:
        src = os.path.join(repo_src, entry)
        dst = os.path.join(app_src, entry)
        if os.path.isdir(src):
            shutil.copytree(src, dst, dirs_exist_ok=True)
        else:
            shutil.copy2(src, dst)

    app.build()
    return os.path.join(ws, app_name, "build", app_name + ".elf")


main()
