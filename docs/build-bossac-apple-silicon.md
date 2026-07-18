# Build `bossac` for Apple Silicon macOS

This document builds a native `bossac` uploader for M-series Macs and adds it
to the moddo Arduino Boards Manager index.

Use the Arduino host identifier:

```text
arm64-apple-darwin
```

Do **not** run `arduino/make_package.sh` unchanged. Its macOS settings target
Intel (`i386` and `x86_64`), so it cannot produce the native Apple Silicon
archive required here.

## 1. Prepare the source

On the Apple Silicon Mac, install Apple's command-line developer tools once:

```bash
xcode-select --install
```

Download or copy `BOSSA-1.7.0-mattairtech-3.zip` to the Mac, then run:

```bash
cd ~/Downloads
unzip BOSSA-1.7.0-mattairtech-3.zip
cd BOSSA-1.7.0-mattairtech-3
```

Use a fresh extracted copy of the source so these build-only edits do not
affect any other checkout.

## 2. Build native `bossac`

Replace the old Intel architecture flags in the source Makefile:

```bash
sed -i '' \
  's/-arch i386 -arch x86_64 -mmacosx-version-min=10\.5/-arch arm64 -mmacosx-version-min=11.0/g' \
  Makefile
```

Build only the command-line uploader:

```bash
make clean

# The release already contains this generated applet source. Updating its
# timestamp prevents make from attempting to regenerate it with legacy tools.
touch src/WordCopyArm.cpp src/WordCopyArm.h

make -j"$(sysctl -n hw.ncpu)" CXX=clang++ bin/bossac
strip -x bin/bossac

file bin/bossac
./bin/bossac --help
```

The `file` output must identify `bin/bossac` as an `arm64` Mach-O executable.
The help command should print BOSSA usage text without needing a board
connected.

## 3. Create the Boards Manager archive

Run the following from the `BOSSA-1.7.0-mattairtech-3` source directory:

```bash
VERSION=1.7.0-mattairtech-3
HOST=arm64-apple-darwin
DIR="bossac-$VERSION-$HOST"
ARCHIVE="arduino/$DIR.tar.gz"

rm -rf "arduino/$DIR"
mkdir -p "arduino/$DIR"
install -m 755 bin/bossac "arduino/$DIR/bossac"

(
  cd arduino
  COPYFILE_DISABLE=1 tar -czf "$DIR.tar.gz" "$DIR"
)

tar -tzf "$ARCHIVE"
shasum -a 256 "$ARCHIVE"
stat -f '%z' "$ARCHIVE"
```

The archive must contain exactly one top-level folder and the executable:

```text
bossac-1.7.0-mattairtech-3-arm64-apple-darwin/
bossac-1.7.0-mattairtech-3-arm64-apple-darwin/bossac
```

Record:

- The first column of the `shasum -a 256` output: the 64-character SHA-256
  digest.
- The output of `stat -f '%z'`: the archive size in bytes.

## 4. Publish the archive

Upload this file to the existing GitHub release tagged `tools`:

```text
arduino/bossac-1.7.0-mattairtech-3-arm64-apple-darwin.tar.gz
```

Its release-asset URL must be:

```text
https://github.com/moddoio/moddoBoards-SAMD/releases/download/tools/bossac-1.7.0-mattairtech-3-arm64-apple-darwin.tar.gz
```

## 5. Update `package_moddo_index.json`

In the `bossac` tool's `systems` array, add this object after the Linux entry.
Replace the two placeholders with the values from step 3. There must be no
space after `SHA-256:`.

```json
{
  "host": "arm64-apple-darwin",
  "url": "https://github.com/moddoio/moddoBoards-SAMD/releases/download/tools/bossac-1.7.0-mattairtech-3-arm64-apple-darwin.tar.gz",
  "archiveFileName": "bossac-1.7.0-mattairtech-3-arm64-apple-darwin.tar.gz",
  "checksum": "SHA-256:PASTE_SHA256_DIGEST_HERE",
  "size": "PASTE_ARCHIVE_SIZE_HERE"
}
```

The same index also needs an Apple Silicon entry for `CMSIS-Atmel`; otherwise
Boards Manager will fail on that dependency after downloading `bossac`. The
CMSIS archive itself is platform-independent, so reuse the existing asset and
metadata:

```json
{
  "host": "arm64-apple-darwin",
  "url": "https://github.com/moddoio/moddoBoards-SAMD/releases/download/tools/CMSIS-Atmel-3.49.1.tar.gz",
  "archiveFileName": "CMSIS-Atmel-3.49.1.tar.gz",
  "checksum": "SHA-256:390438009c9ee5988ee49e05cb305fb506187ee6d0070bef01f56b9a31d367a4",
  "size": "799218"
}
```

Commit and push the updated index after the release assets are published.

## 6. Test on the Mac

Update the Arduino IDE package index, then install **moddo SAMD Boards**. A
successful Apple Silicon installation downloads both `bossac` and
`CMSIS-Atmel`, and uses the executable named `bossac` (without `.exe`).

If Intel Macs will also be supported, build and publish a separate
`x86_64-apple-darwin` archive. Do not label an ARM64 archive as x86_64.
