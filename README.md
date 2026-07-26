# M5Paper EPUB Reader

Open-source, offline EPUB 2/3 reader firmware for the **original M5Stack
M5Paper**. It reads books directly from a FAT32 microSD card, keeps memory use
bounded, supports touch navigation and physical-button font zoom, and resumes
the last stable reading position.

> This firmware is exclusively for the original ESP32 M5Paper. It does not
> support PaperS3.

## Features

- incremental ZIP/XHTML processing without loading an entire book or chapter;
- EPUB 2 NCX and EPUB 3 navigation-document table of contents;
- continuous pagination, bounded page cache and checkpoint reconstruction;
- font sizes of 16, 24, 32, 36 and 40 px with anchored reflow;
- persistent reading position, font preference and safe “restart book” action;
- English and Brazilian Portuguese interface, with English as the default;
- deliberate E-Ink refresh modes and serialized display/microSD SPI access;
- Wi-Fi and Bluetooth disabled by design.

## Install

Requirements: original M5Paper, FAT32 microSD, USB cable, Python 3 and
[PlatformIO Core](https://platformio.org/install/cli).

```powershell
# Clone or download this repository, then enter its directory:
cd M5PaperEpubReader
python -m pip install -r requirements-dev.txt
pio run -e m5stack-paper
pio run -e m5stack-paper --target upload --upload-port COM8
```

Replace `COM8` with the serial port shown by `pio device list`.

Place `.epub` files in `/Books` on the microSD card. If that directory does not
exist, the browser starts at the card root. Open the reading menu by tapping the
top band. The menu includes language selection, table of contents, restart and
library navigation.

## Supported content

The reader targets textual, reflowable EPUB 2/3 books. DRM, JavaScript, audio,
video, MathML, embedded fonts and fixed-layout EPUB are not supported. Semantic
headings, paragraphs, lists, quotes, bold and italic markers participate in the
text layout. Raster-image and basic-CSS discovery components exist, but complete
inline image rendering and the full CSS cascade are not claimed as supported.

## Development

```powershell
python -m pip install -r requirements-dev.txt
pio test -e native
pio run -e m5stack-paper
```

See [CONTRIBUTING.md](CONTRIBUTING.md), [docs/architecture.md](docs/architecture.md)
and [docs/hardware-test-plan.md](docs/hardware-test-plan.md). Hardware-dependent
tests must be performed on the original M5Paper.

## Português

Firmware open source e offline para leitura incremental de EPUB 2/3 no
**M5Stack M5Paper original**. A interface inicia em inglês; no menu de leitura,
toque em `Language: English` para mudar para português. Coloque os livros na
pasta `/Books` do cartão microSD e compile/grave usando os comandos acima.

## License

Copyright © 2026 M5PaperEpubReader contributors. Released under the
[MIT License](LICENSE). Third-party libraries retain their respective licenses;
see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
