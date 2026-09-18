# smallest-touchpanel-ui

Sorry, under construction.

## Important changes.

- 2024/03/18 fontid -> struct lcdtp_font_struct *font
- 2023/07/22 polltask() -> lcdtp_polltask().

## usage

Type "make sample1" on directory x11/ to build.

Type "make sample1.hex" on directory pic32mx/ to build.

The rx65n/ port is built from a separate harness, which fetches it along
with the RX65N startup files: https://github.com/paijp/rx65n

Pin map and circuit diagram: https://paijp.github.io/smallest-touchpanel-ui/pic32mx/lcdtp.html

LCD parameter settings by http://www.lcdwiki.com/res/Program/Common_SPI/2.8inch/SPI_ILI9341_MSP2807_V1.1/2.8inch_SPI_Module_ILI9341_MSP2807_V1.1.zip

## photo

![proto1](https://paijp.github.io/smallest-touchpanel-ui/image/proto1.jpeg)

![proto2](https://paijp.github.io/smallest-touchpanel-ui/image/proto2.jpeg)

## licence

Apache 2.0 (see LICENSE), except rx65n/envision_hw.c and
rx65n/envision_hw.h, which are MIT and carry their own notice. See NOTICE.
