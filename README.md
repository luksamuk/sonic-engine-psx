# sonic-engine-psx

Disclaimer: I am not the original owner of any songs here, nor of the Psy-Q SDK, nor of Sonic The Hedgehog.

This was built with educational purposes only.

## Building

Build the application's `ps-exe`:

```bash
# Build in debug mode
make BUILD=Debug

# Build in release mode
make
```

Make sure you have `LICENSEA.DAT` or equivalent on `LCNSFILE` directory (this file is not provided here!!!).

Now build the ISO using `buildiso.sh`, or run the `buildshell.sh` and run the command:

```bash
mkpsxiso -y CDLAYOUT.xml
```

## Cooking audio files

To cook audio files, you need to:

- Convert them to .WAV files (with a tool such as `ffmpeg`);
- Convert them to individual .XA files (with `psxavenc`);
- Interleave them in a single `.XA` file containing up to 4 or 8 different songs (known as channels)
  - You MUST use either 4 or 8 files, and set them to `null` if not used. The number of channels determine the CD-ROM read speed (1x or 2x respectively).

You'll have to execute the `buildshell.sh` script to start a `bash` within a Docker container, with tools to build the app.

Should you need extra info, see [this link](https://github.com/ABelliqueux/nolibgs_hello_worlds/wiki/XA).

### Converting files to WAV

```bash
ffmpeg -i file.ogg -acodec pcm_s16le -ac 2 -ar 44100 file.wav
```

Tweak `ac` to change mono/stereo and `-ar` to reduce or increase audio quality (here 44.1kHZ is assumed).

### Converting WAV to individual XA

```bash
# Run this within the build shell
psxavenc -f 37800 -t xa -b 4 -c 2 -F 1 -C 0 file.wav file.xa
```

Here's how I'd convert a directory full of .WAV files:

```bash
for f in *.WAV; do psxavenc -f 37800 -t xa -b 4 -c 2 -F 1 -C 0 "$f" "${f%%.WAV}.XA"; done
```

### Interleaving individual .XA files

First, create a file containing the interleave mapping (e.g. `xainterleave.txt`). The file layout should look somewhat like this for an individual song:

```
1 xa file.xa 1 0
1 null
1 null
1 null
```
In this case, we're building a .XA file with 4 channels, where 3 channels remain unused. Furthermore, using a 1x CD read speed (when playing the song) should suffice.

...or, if you were using two songs:

```
1 xa file1.xa 1 0
1 xa file2.xa 1 1
1 null
1 null
```

Notice how we change the channel number for each.

Now interleave the XA:

```bash
# Run this within the build shell
xainterleave 1 xainterleave.txt FINAL.XA
```

That's it. All that remains is adding `FINAL.XA` to `CDLAYOUT.XML` so it packs nicely within the ISO.

```xml
<file name="FINAL.XA" type="xa" source="FINAL.XA" />
```

There is a proper tutorial on using .XA [here](https://psx.arthus.net/sdk/Psy-Q/DOCS/XATUT.pdf).