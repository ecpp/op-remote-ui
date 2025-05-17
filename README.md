# OpenPilot UI Web Streaming

This project enables streaming the openpilot UI to a web browser, allowing for headless operation, remote debugging, and control. It captures UI screenshots at regular intervals and provides interactive touch, drag, and scroll functionality via a web interface.

## Overview

The implementation works by:
1. Taking screenshots of the UI every 2 seconds
2. Streaming these screenshots to a web browser using a Flask server
3. Capturing touch/click/scroll events from the web interface and sending them back to the UI

While not the most optimal solution, this approach is effective for debugging and remote monitoring purposes.

## Installation Steps

### 1. Install Flask

```bash
pip3 install flask
```

### 2. Create a New Branch (Recommended)

Create a new branch and remove prebuilt files to prevent your changes from being overwritten when the device reboots.

```bash
cd /data/openpilot
git checkout -b ui_streaming
# Remove prebuilt files if necessary
```

### 3. Add the Source Files

Copy the following files to your openpilot directory:
- `touch_injector.h` → `/data/openpilot/selfdrive/ui/`
- `touch_injector.cc` → `/data/openpilot/selfdrive/ui/`
- Replace the existing `main.cc` → `/data/openpilot/selfdrive/ui/`
- Add `screenshot_server.py` → `/data/`

### 4. Update SConscript

Modify the `SConscript` file in the `/data/openpilot/selfdrive/ui/` directory to include `touch_injector.cc` in the source files:

```python
# Add 'touch_injector.cc' to the qt_src list
qt_src = [
  # existing files...
  'touch_injector.cc',
  # other files...
]
```

### 5. Rebuild the UI

Compile the changes:

```bash
cd /data/openpilot
scons selfdrive/ui
```

### 6. Make the Screenshot Server Start on Boot (Optional)

Edit `/data/openpilot/launch_openpilot.sh` to launch the screenshot server on boot:

```bash
# Add this line before openpilot is launched
python3 /data/screenshot_server.py &
```

### 7. Test and Reboot

Kill the running openpilot process and test the changes:

```bash
# Enter tmux session
tmux a

# Kill openpilot (Ctrl+C)
# Exit tmux (Ctrl+B, then D)

# Launch openpilot with changes
cd /data/openpilot
./launch_openpilot.sh
```

If everything works correctly, reboot the device and access the UI by navigating to:
```
http://DEVICE_IP:8081
```

## Important Notes

- **Disk Space**: Taking screenshots every 2 seconds can quickly fill up storage. I recommend you edit the server code to remove old screenshots for example
- **Calibration Issues**: Initial calibration of OPENPILOT may not complete correctly when using a custom branch (atleast it did not for me). If needed, switch back to the release branch after debugging.
- **Model Files**: You may need to copy model files from the release branch if they're missing in your custom branch initial launch (check `/data/openpilot/selfdrive/modeld/models`).