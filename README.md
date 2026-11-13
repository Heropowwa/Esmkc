# Esmkc: Extremely simple and minimal keyboard controller for the Lenovo Legion 5 gen 11 on Linux

Sets the color and brightness of a **single-zone RGB** Legion 5 keyboard without Lenovo Vantage.
Written in C, one file, no dependencies.

## Build

```bash
gcc -o esmkc esmkc.c
```

## Usage

```bash
sudo ./esmkc color 88 5 255     # the default violet, in slot 2
sudo ./esmkc color 255 0 0      # red
sudo ./esmkc brightness 5       # 1-9
sudo ./esmkc status             # shows the brightness, e.g. "Brightness: 3/9 (33%)"
```

It needs root because it writes to `/dev/hidraw*`.

`color` always writes **slot 2** (the "fixed color" preset) and selects it. It never touches the
other presets. Brightness is on a scale of 1 to 9.

This matches my own setup: in Vantage I use slot 2 as my custom solid color preset. Yours may be arranged
differently. Making the slot selectable from the command
line would be easy, but I wanted something really simple and straightforward.

## How it was made

I captured the USB traffic of the keyboard's HID controller with Wireshark (USBPcap on Windows)
while changing colors and brightness in Vantage, and reverse-engineered the packets I needed from that.

## FAQ

**Why C?**
Mostly because why not. Also, I can't be bothered to fight the Rust compiler and crates for
something this small.

**Why wait 500 ms between the two packets?**
Small tests showed that at about 100 ms the color was often lost. From 150 ms up it worked, but
not always reliably. 500 ms leaves a wide margin and isn't that noticeable when you run the command by hand.

**Does it work on other Legion models?**
Unknown. Other models may use a different controller or packet layout. Check `lsusb | grep 048d`
first: if you don't see `048d:c615`, this tool won't find your keyboard. Support for other models
could probably be added without much effort, but I doubt anyone besides me will use this.

**Why not OpenRGB or an existing tool?**
OpenRGB didn't recognize this controller, and none of the Legion tools and similar I tried work with this
particular model, so I made my own.
