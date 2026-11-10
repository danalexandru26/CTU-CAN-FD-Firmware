# CTU CAN FD on Zynq-7020 — Vitis Workspace

Bare-metal firmware for the open-source **CTU CAN FD** controller running in the
programmable logic of a Digilent **Arty Z7-20** (Zynq-7000). This workspace holds the
Vitis platforms, the applications, and a shared, handle-based CAN / CAN FD driver.

It is the software side of a research project on adapting **CANsec** to CAN FD,
using **Ascon** instead of AES.

| | |
|---|---|
| **Toolchain** | AMD Vitis Unified 2025.2, standalone (bare-metal) on `ps7_cortexa9_0` |
| **Controller** | CTU CAN FD IP core, Zynq PS → AXI → APB bridge |
| **Fabric clock** | 100 MHz (all bit-timing presets assume this) |
| **Hardware repo** | [`CTU_CAN_Ascon`](https://github.com/danalexandru26/CTU_CAN_Ascon) (Vivado, exports the `.xsa`), local: `../../RTL/CTU_CAN_Ascon` |

---

## Status

| Test | Platform / App | Result |
|---|---|---|
| Single node, internal loopback + self-test, CAN FD 1 Mbit/s / 5 Mbit/s, 32-byte payload | `CTU_CAN_FD_LOOPBACK_2TX_ILA131072` / `CTU_CAN_FD_ILA_Firmware` | ✅ TX OK, received frame matches (ID, length, flags, data) |
| Two nodes on a fabric bus, CAN FD 1 Mbit/s / 5 Mbit/s with BRS, 32-byte payload, node 1 → node 2 | `CTU_CAN_FD_Double_Node_Wrapper` / `CTU_CAN_FD_DUAL_NODE_FIRMWARE` | ✅ TX OK on node 1, correct frame received on node 2, ACK visible on the ILA |
| Two nodes, node 2 → node 1, and arbitration | — | ⏳ Next |

---

## Workspace layout

```
Vitis_Workspaces/
├── driver/
│   └── ctu_can_fd/                       Shared CTU CAN FD driver (used by every app)
│       ├── ctu_can_fd.cmake              Exports CTU_CAN_FD_SOURCES / CTU_CAN_FD_INCLUDE_DIRS
│       ├── include/ctu_can.h
│       └── src/ctu_can.c
│
├── CTU_CAN_FD_Double_Node_Wrapper/       Platform: CAN_Double_Node_System_wrapper.xsa
├── CTU_CAN_FD_DUAL_NODE_FIRMWARE/        App: two-node TX/RX test (src/firmware.c)
│
├── CTU_CAN_FD_LOOPBACK_2TX_ILA131072/    Platform: single node + ILA (CAN_System_wrapper.xsa)
├── CTU_CAN_FD_ILA_Firmware/              App: single-node loopback self-test (src/ILA_Firmware.c)
│
├── CAN_FD_Platform/, CAN_FD_AXI_APB_TEST/  Early AXI→APB bring-up (legacy)
├── AES_SOC/, hello_world/                Unrelated experiments (not tracked in git)
└── scripts/                              rebuild.bat / rebuild.py
```

### Hardware: two-node system

`CAN_Double_Node_System` puts two CTU CAN FD instances behind their own AXI-APB
bridges on the SmartConnect and ties their TX/RX lines together with a wired-AND
bus inside the fabric, so the two nodes talk to each other without a transceiver.

| Node | Base address | Name in firmware |
|---|---|---|
| CAN0 | `0x43C0_0000` | `CTU CAN NODE 1` |
| CAN1 | `0x43C1_0000` | `CTU CAN NODE 2` |

- Interrupts are not connected; the driver works by **polling**.
- Each core is synthesized with **2 TXT buffers** (reduced from 4 to close timing at
  100 MHz). The driver only uses buffers **1 and 2**.
- ILA probes (`.ltx`) are produced by the Vivado implementation run (`impl_1`) in the
  hardware repo, not in this workspace.

---

## The `ctu_can` driver

A small, struct-based driver: each controller is a `ctu_can_t` handle carrying its
base address, so any number of nodes can be driven from the same code.

```c
#include "ctu_can.h"

ctu_can_config_t cfg = {
    .nominal = CTU_CAN_BT_1M,       /* arbitration phase */
    .data    = CTU_CAN_BT_FD_5M,    /* FD data phase     */
    .opts    = CTU_CAN_OPT_FD,
};

ctu_can_t can0, can1;
ctu_can_init(&can0, "CAN0", 0x43C00000u, &cfg);
ctu_can_init(&can1, "CAN1", 0x43C10000u, &cfg);

can_frame_t tx = { .id = 0x123, .len = 32, .flags = CAN_FLAG_FD | CAN_FLAG_BRS };
ctu_can_send_txt(&can0, 1, &tx);          /* or ctu_can_send() for round-robin */

can_frame_t rx;
if (ctu_can_recv(&can1, &rx) == 1) { /* got a frame */ }
```

### Types

| Type | Purpose |
|---|---|
| `ctu_can_t` | Device handle: `base`, `name`, `txt_count`, `opts`, `next_txt`, `stats` |
| `ctu_can_config_t` | `nominal` + `data` bit timing and `opts` flags |
| `ctu_can_bittiming_t` | `brp`, `prop`, `ph1`, `ph2`, `sjw` (packed into `BTR` / `BTR_FD`) |
| `can_frame_t` | One frame type for classic CAN and CAN FD: `id`, `len` (0–64), `flags`, `timestamp`, `data[64]` |

### Functions

| Function | Description |
|---|---|
| `ctu_can_init()` | Checks `DEVICE_ID` (`0xCAFD`), resets, programs bit timing and mode, enables the core, flushes RX |
| `ctu_can_disable()` | Clears mode bits and disables the core |
| `ctu_can_send_txt()` | Writes a frame into a specific TXT buffer and marks it ready |
| `ctu_can_send()` | Same, picking TXT buffers round-robin; returns the buffer used |
| `ctu_can_recv()` | Reads one frame from the RX buffer; returns `1` on success, `-1` if empty |
| `txt_state()` | Human-readable name for a `TX_STATUS` buffer state |
| `ctu_can_btr_pack()`, `ctu_can_len_to_dlc()`, `ctu_can_dlc_to_len()` | Helpers |

**Options** (`ctu_can_opt_t`): `CTU_CAN_OPT_FD`, `CTU_CAN_OPT_SELF_TEST` (missing ACK is
not an error), `CTU_CAN_OPT_LOOPBACK` (receive own frames), `CTU_CAN_OPT_BUS_MONITOR`.

### Bit-timing presets (100 MHz core clock)

| Nominal | Data (FD) |
|---|---|
| `CTU_CAN_BT_125K`, `_250K`, `_500K`, `_1M` | `CTU_CAN_BT_FD_1M`, `_2M`, `_4M`, `_5M`, `_10M` |

### Using the driver in an app

Vitis 2025.2 sets `USER_COMPILE_SOURCES` inside each app's `src/CMakeLists.txt`, so
the driver is pulled in with a short block placed **right after**
`set(USER_COMPILE_SOURCES ...)`:

```cmake
# --- shared CTU CAN FD driver ---
set(DRV ${CMAKE_CURRENT_SOURCE_DIR}/../../driver)
include(${DRV}/ctu_can_fd/ctu_can_fd.cmake)
list(APPEND USER_COMPILE_SOURCES ${CTU_CAN_FD_SOURCES})
list(REMOVE_DUPLICATES USER_COMPILE_SOURCES)
list(APPEND USER_INCLUDE_DIRECTORIES ${CTU_CAN_FD_INCLUDE_DIRS})
# --------------------------------
```

> If Vitis regenerates `CMakeLists.txt`, add the block again. After changing CMake
> files, rebuild once so `compile_commands.json` updates, then restart clangd.

---

## Working with the hardware

### Updating the `.xsa`

1. In Vivado, export the hardware (**File → Export → Export Hardware**, include bitstream).
2. In Vitis, open the platform settings, **Switch XSA** to the new file, then rebuild the
   platform and the app.
3. Commit the updated `.xsa` under the platform's `hw/` folder.

### Rebuilding from a clean clone

```bat
scripts\rebuild.bat                   :: two-node setup into .\_rebuild_ws (ignored by git)
scripts\rebuild.bat single            :: single-node ILA setup
scripts\rebuild.bat all D:\some_ws    :: both setups, into any empty folder
```

| Setup | Platform | App |
|---|---|---|
| `dual` (default) | `CTU_CAN_FD_Double_Node_Wrapper` | `CTU_CAN_FD_DUAL_NODE_FIRMWARE` |
| `single` | `CTU_CAN_FD_LOOPBACK_2TX_ILA131072` | `CTU_CAN_FD_ILA_Firmware` |

This runs `vitis -s scripts/rebuild.py`, which copies `driver/` into the new workspace,
creates each platform from its committed `.xsa`, creates the app, copies in its tracked
sources (including `CMakeLists.txt`), and builds the `.elf`. Set `XILINX_VITIS` if Vitis
is not installed at `C:\AMDDesignTools\2025.2.1\Vitis`.

You can also open the cloned folder directly as a Vitis 2025.2 workspace and build the
platform, then the app.

---

## What is in git

Tracked: app sources, `vitis-comp.json`, `CMakeLists.txt` / `UserConfig.cmake`, linker
scripts, BSP settings (`*.yaml`, `*.cmake`), each platform's `.xsa`,
`_ide/launch.json`, `driver/` and `scripts/`.

Ignored: everything Vitis regenerates (`build/`, `export/`, `logs/`, BSP
`libsrc/lib/include`, `hw/sdt/`, workspace `_ide/`). Top-level folders are
whitelisted in `.gitignore`; to track another component, add `!/<Folder>/`.

---

## Roadmap

1. Two-node: reverse direction (node 2 → node 1) and arbitration test
2. Bit-rate test suite across nominal / data rate combinations
3. Ascon-AEAD128 AXI-Lite accelerator in the PL (planned at `0x43C2_0000`)
4. Store-and-forward CANsec baseline: CAN0 → Ascon → CAN1
5. Cut-through (bit-by-bit) CANsec
