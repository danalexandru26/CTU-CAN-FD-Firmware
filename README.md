# CTU CAN FD ILA – Vitis workspace

Vitis Unified **2025.2** workspace for the Digilent Zynq-7020 board.

| Folder | Vitis component | What it is |
|---|---|---|
| `CTU_CAN_FD_ILA/` | Platform | Built from `hw/CAN_System_wrapper_ILA.xsa`; domains `zynq_fsbl` and `standalone_ps7_cortexa9_0` |
| `CTU_CAN_FD_ILA_Firmware/` | Application | Bare-metal firmware on `ps7_cortexa9_0` (sources in `src/`) |

The hardware design (Vivado project that exports the `.xsa`) is a separate repo:
`../../RTL/CTU_CAN_Ascon` → https://github.com/danalexandru26/CTU_CAN_Ascon

## What is in git

Sources, `vitis-comp.json`, `UserConfig.cmake`, linker scripts, BSP settings
(`bsp.yaml`, `lib_list.yaml`, …), the platform's `.xsa`, and `_ide/launch.json`.
Everything Vitis regenerates (`build/`, `export/`, `logs/`, BSP `libsrc/lib/include`,
`hw/sdt/`, workspace `_ide/`) is ignored – see `.gitignore`.

Other components in this folder (AES_SOC, hello_world, …) are ignored on purpose.
To track one, add `!/<Folder>/` to the whitelist at the top of `.gitignore`.

## Updating the hardware

1. In Vivado, re-export the hardware (`File > Export > Export Hardware`, include bitstream).
2. In Vitis, open the platform's settings and point it at the new `.xsa` ("Switch XSA"),
   then rebuild the platform and the app.
3. Commit the updated `CTU_CAN_FD_ILA/hw/CAN_System_wrapper_ILA.xsa`.

## Rebuilding from a clean clone

```bat
scripts\rebuild.bat            :: builds into .\_rebuild_ws (ignored by git)
scripts\rebuild.bat D:\some_ws :: or any empty folder
```

This runs `vitis -s scripts/rebuild.py`, which recreates the platform from the
committed `.xsa`, builds it, creates the app, copies in `CTU_CAN_FD_ILA_Firmware/src`,
and builds the `.elf`. Set `XILINX_VITIS` if Vitis is not at
`C:\AMDDesignTools\2025.2.1\Vitis`.

Alternatively, open the cloned folder directly as a workspace in Vitis 2025.2 and
build the platform, then the app: the committed `vitis-comp.json` files describe both
components.
