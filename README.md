# sony-tracker (Linux)

A small, no-nonsense Linux port of [NicholasSlattery's sony-head-tracker](https://github.com/NicholasSlattery/sony-head-tracker) — it reads the motion sensor already sitting inside compatible Sony headphones and turns it into head-tracking data for [OpenTrack](https://github.com/opentrack/opentrack), racing sims, flight sims, or your own scripts.

No webcam, no extra hardware, no custom kernel driver. Just the gyro/orientation sensor your headphones already have, read over Bluetooth HID.

> **Unofficial project.** Not affiliated with or endorsed by Sony. This is a hobby port, use at your own risk.

## Why this exists

The original project is a great piece of work, but it's Windows-only (built around `HidD_*`/`HidP_*`, the Windows Sensor API, and a native GUI). This is a from-scratch Linux rewrite in plain C that talks to the same HID protocol directly through `hidraw`, keeping things about as small and dependency-free as the original.

## What it does

- Auto-detects any paired Sony headset that speaks the Android Head Tracker HID protocol
- Lets you pick a specific device (or several) if more than one is found
- Streams orientation as UDP doubles for OpenTrack, on a configurable port
- Can drive multiple headsets at once, each on its own port

## What it doesn't do (yet)

- No GUI — this is a CLI tool
- No gyroscope/accelerometer output. The HID descriptor advertises these fields, but on the WH-1000XM5 (the only unit this has been tested against so far) the firmware never actually populates them — every sample comes back zeroed regardless of how much you shake the headset. Orientation (rotation vector → yaw/pitch/roll) works fine and is all OpenTrack actually needs, so this hasn't been a blocker, but if your headset *does* report real gyro data, open an issue and it can be wired back in properly.

## Compatibility

Only tested against the **Sony WH-1000XM5** so far. The underlying protocol is the same one the Windows original documents, so other Sony models on their head-tracking list (WF-1000XM5/XM6, WH-1000XM6, LinkBuds family, ULT WEAR, etc.) should work too, but haven't been verified here. If you try one, a quick issue with what worked (or didn't) would be genuinely useful for anyone else who finds this.

## Build

```bash
git clone https://github.com/kdani3/SonyTrackerLinux
cd sony-tracker
make
```


## Install (optional)

```bash
sudo make install
sudo udevadm control --reload-rules
sudo udevadm trigger
```

This installs the binary to `/usr/local/bin/sony-tracker` and adds a udev rule so you don't need `sudo` every time you run it. Add yourself to the `plugdev` group if you're not already in it (It doesn't currently work in Arch or Arch based distros):

```bash
sudo usermod -aG plugdev $USER
# log out and back in for this to take effect
```

## Pairing your headphones

Standard Bluetooth pairing, nothing special:

```bash
bluetoothctl
# power on
# scan on
# pair <MAC>
# trust <MAC>
# connect <MAC>
```

## Usage

```bash
sony-tracker                              # auto-detect; picks the one device found,
                                           # or prompts if more than one shows up
sony-tracker -d /dev/hidraw3              # use a specific device directly
sony-tracker -d /dev/hidraw3 /dev/hidraw4 # use several specific devices at once
sony-tracker --all                        # stream every detected headset simultaneously
sony-tracker --port 5000                  # change the base UDP port (default 4242)
sony-tracker --verbose                    # print live yaw/pitch/roll to the terminal
sony-tracker --help                       # see all options
```


When streaming multiple devices, each one gets sequential ports starting from `--port` (e.g. `4242`, `4243`, `4244`, ...) — point separate OpenTrack instances or listeners at each.

If the headphones are paired and you get

```bash
no head-tracker device found (headset paired and connected?)
```
try with `sudo` \
I never said i was good enough.
## Connecting to OpenTrack

1. Install OpenTrack (`sudo apt install opentrack`, or grab an AppImage from their [releases page](https://github.com/opentrack/opentrack/releases)).
2. In OpenTrack, set **Input** to `UDP over network`, click the wrench icon, and set the port to match what `sony-tracker` is sending to (`4242` by default).
3. Press **Start** in OpenTrack.
4. Run `sony-tracker`.
5. Turn your head — OpenTrack's live pose display should follow along.

If nothing moves, run `sony-tracker --verbose` to confirm the tracker itself is producing sane yaw/pitch/roll values first — that isolates whether the problem is on the sensor side or the OpenTrack/UDP side.

## A word on security

UDP output is loopback-only (`127.0.0.1`) by default and has no authentication, same as the original Windows project. Don't forward this port to a network you don't trust.

## How it finds your headphones

It scans `/dev/hidraw*` for a device whose HID report descriptor declares the Sensor page / Custom (`0x20`/`0xE1`) usage, then confirms it by checking for the `#AndroidHeadTracker#` marker string in its feature report — the same marker the original Windows project checks for. If your headset shows up as `hidraw` but isn't detected, it may not (yet) implement this exact protocol; a `hid-recorder` dump of its report descriptor is the fastest way to check.

## Building blocks, if you're curious

- `hidraw.h` — thin wrapper around Linux's `hidraw` ioctls (open, read reports, read/set feature reports, scan for matching devices)
- `quat.h` — the orientation math: rotation-vector → quaternion, quaternion → yaw/pitch/roll, recentering
- `main.c` — argument parsing, device selection, the per-device read/decode/send loop (threaded when running multiple headsets), UDP output

Nothing here is exotic — it's a report descriptor, some bit unpacking, a bit of quaternion math, and a UDP socket. If you've ever wanted to see how a Bluetooth sensor actually gets from raw bytes to "the number OpenTrack wants," the source is short enough to read in one sitting.

## License

MIT, same as the original project. See [`LICENSE`](LICENSE).

## Credit

All protocol reverse-engineering and the original design are [NicholasSlattery](https://github.com/NicholasSlattery)'s work on the [Windows original](https://github.com/NicholasSlattery/sony-head-tracker) — this is just a Linux-shaped rewrite of the same idea.

> Believe in youself