# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
### Changed
- lcdtp.c: update the circuit markup description.
- pcb/: newest version.
- lcdtp.h tplib.c: TPLIB_FONT12 -> LCDTP_FONT12
- */lcdtp.?: support lcdtp_sendlog*()
- lcdtp.c lcdtp.html: fix MCP3424 pin diagram.
- add photos.
- */lcdtp.?: fontid -> struct lcdtp_font_struct *font.
- x11/lcdtp.c: usleep in gettp() as hardware wait in pic32mx.
- */tplib.c: Fix return value in tplib_parts_buttongroup().
- */tplib.? */sample*: support tplib_parts_log().

## [0.0.3] - 2023-12-24
### Changed
- Rename port_circuit.html -> lcdtp.html.
- Add diagram-in-comment link to lcdtp.html.

## [0.0.2] - 2023-12-24
### Changed
- Change diagram-in-comment description.

## [0.0.1] - 2023-12-16
### Added 
- Create CHANGELOG.md
- Change comments in pic32mx/lcdtp.c to create pic32mx/port_circuit.html using paijp/diagram-in-code.
