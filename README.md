# PerSEUS Prototypes

This repository is now organized as a multi-project workspace:

- `apps/gui`: host-side GUI controller prototype and Python fake-device tooling
- `firmware/devices`: one ESP-IDF project per hardware device type
- `firmware/components`: reusable embedded components shared by device projects
- `shared/python`: shared Python modules, including protocol helpers
- `shared/protocol`: protocol-level documentation and contract notes

## Layout

```text
apps/
  gui/
    src/wizard_gui/
firmware/
  components/perseus_common/
  devices/
    anthro_sphere/
    abstract_sphere/
    flower/
shared/
  protocol/
  python/perseus_shared/
```

## Why this structure

- The GUI remains independent from firmware builds.
- Each device can evolve as a standalone ESP-IDF project with its own target, sdkconfig, and `main`.
- Reusable firmware logic has a first-class home in `firmware/components`.
- Protocol code and documentation can stay aligned across Python and embedded implementations.

## Current entrypoints

GUI:

```powershell
cd apps\gui
$env:PYTHONPATH = "src;..\..\shared\python"
python run_gui.py
```

Fake device:

```powershell
cd apps\gui
$env:PYTHONPATH = "src;..\..\shared\python"
python run_fake_device.py --device-id flower_01 --device-kind flower
```

Firmware:

```powershell
cd firmware\devices\flower
idf.py build
```
