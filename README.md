# GTK DMD 5620 Emulator

This is a GTK+ 3.0 implementation of an AT&T / Teletype DMD 5620 emulator.

## Status

Version: 1.4.1

This is an actively developed project.

## Dependencies

The executable has the following dependencies:

* Rust toolchain version 1.50 or later (see: https://rustup.rs)
* GTK+ 3 (libgtk)
* GDK Pixbuf 2 (libgdk-pixbuf)
* Cairo 1.15+ (libcairo)
* Glib 2 (libglib)

## Building

- Clone with `git clone --recursive` to make sure submodules are up to date,
  *or* run `git submodule init; git submodule update`.
- Ensure that the Rust toolchain is installed. This is most easily done using
  the `rustup` installation script. For more information, see
  https://rustlang.org/ and https://rustup.rs/
- Type `make`

This new build process is still somewhat experimental. The Makefile
attempts to ensure that the Git submodule `dmd_core` is updated and built,
but this has not yet been widely tested.

## Usage

### Running the Terminal

```
Usage: dmd5620 [-h] [-v] [-V] [-D] [-d DEV|-s SHELL]\
               [-t FILE] [-n FILE] [-- <gtk_options> ...]
AT&T DMD 5620 Terminal emulator.

-h, --help                display help and exit
-v, --version             display version and exit
-V, --verbose             display verbose output
-D, --delete              backspace sends ^? (DEL) instead of ^H
-t, --trace FILE          trace to FILE
-d, --device DEV          serial port name
-s, --shell SHELL         execute SHELL instead of default user shell
-n, --nvram FILE          store nvram state in FILE
```

- `--help` displays the help shown above, and exits.
- `--version` displays the executable version number, and exits.
- `--nvram FILE` causes terminal parameters stored in non-volatile memory
   to be persisted to `FILE`.
- `--shell SHELL` will execute the specified shell (e.g. "/bin/sh")
- `--device DEV` will attach the terminal to the specified physical or 
   virtual serial device (e.g. "/dev/ttyS0")
- `--delete` will cause the terminal to send the DELETE character (`^?`)
   instead of BACKSPACE (`^H`) when the backspace key is pressed.
- `--trace FILE` allows optional and *extremely verbose* trace logging
   to the supplied file. Tracing is turned on and off by pressing `F10`.
- `--verbose` causes each character received or transmitted to be
   printed to stdout. Useful for debugging.

Example usage:

```
$ dmd5620 --nvram ~/.dmd5620_nvram --shell /bin/sh
$ dmd5620 -D --nvram ~/.dmd5620_nvram --device /dev/ttyS0
```

### Configuration

All configuration of the terminal is done by pressing the `F9` key, which shows
a series of menu buttons at the bottom of the screen. These buttons reveal
menus that allow you to set the baud rate, reverse the video colors, turn on
and off the bell, and so forth.

Full documentation is available here: [https://archives.loomcom.com/3b2/documents/DMD_Terminal/](https://archives.loomcom.com/3b2/documents/DMD_Terminal/)


## Key Map

Certain keys are mapped to special DMD5620 function keys.

* F1-F8 are mapped directly to terminal F1-F8
* F9 is mapped to the DMD5620's SETUP key.
* Shift+F9 is mapped to the terminal's RESET functionality.

## Changelog

### Version 1.4.1

* Did away with multi-threaded execution and significantly improved timing.
* Added support for attaching to physical and virtual serial ports.
* Upgraded to `dmd_core` 0.6.4.

### Version 1.3.0

* Removed telnet support.
* Added local shell execution.
* Added man page.

### Version 1.2.0

* Upgraded to `libdmd_core` 0.6.3
* Improved timing further (loading with 32ld works now)
* Fixed build in CentOS. Should build cleanly on Fedora,
  RedHat, Ubuntu, and Debian now.

### Version 1.1.0

* Upgraded to `libdmd_core` 0.6.2
* Removed the need for Rust to compile!
* Improved timing.
* Fixed a Telnet bug that prevented BINARY mode negotiation.
* Reduced CPU usage by not redrawing every frame unless
  video RAM has changed.
* Added missing TAB and arrow key support.

## To Do

- Local serial line support is not yet implemented.

## See Also

* [dmd_core](https://github.com/sethm/dmd_core): DMD 5620 core
  implementation library, used by this project.

## License

MIT license. See the file [LICENSE.md](LICENSE.md)

Copyright (c) 2018, Seth Morabito &lt;web@loomcom.com&gt;
