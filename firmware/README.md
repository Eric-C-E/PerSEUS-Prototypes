# Firmware Workspace

Each device lives in its own ESP-IDF project:

- `devices/anthro_sphere`
- `devices/abstract_sphere`
- `devices/flower`

Shared embedded code belongs in `components/perseus_common` first. If a dependency is reusable across two or more devices, prefer promoting it into a shared component instead of duplicating it inside a single project.

Typical usage from an individual device folder:

```powershell
idf.py set-target esp32
idf.py build
```

Each project includes `../../components` through `EXTRA_COMPONENT_DIRS`, so anything placed under `firmware/components` is available to all device builds.
