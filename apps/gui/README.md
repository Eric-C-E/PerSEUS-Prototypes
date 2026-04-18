# GUI Workspace

This folder contains the host-side prototype controller:

- `src/wizard_gui`: Tkinter GUI, TCP server, registry, and device simulator
- `run_gui.py`: local entrypoint for the controller
- `run_fake_device.py`: local entrypoint for the Python fake device

Run from this folder with the shared Python path added:

```powershell
$env:PYTHONPATH = "src;..\..\shared\python"
python run_gui.py
python run_fake_device.py --device-id flower_01 --device-kind flower
```
