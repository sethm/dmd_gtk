# GTK DMD 5620 Emulator

This is a GTK+ 3.0 implementation of an AT&T / Teletype DMD 5620 emulator.

## Status

Version: 2.1.0

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

## Usage

### Running the Terminal

```
Usage: dmd5620 [-h] [-v] [-i] [-d DEV|-s SHELL] \
               [-f VER] [-n FILE] [-- <gtk_options> ...]
AT&T DMD 5620 Terminal emulator.

-h, --help              display help and exit
-v, --version           display version and exit
-i, --inherit           inherit parent environment
-f, --firmware VER      Firmware version ("8;7;3" or "8;7;5")
-d, --device DEV        serial port name
-s, --shell SHELL       execute SHELL instead of default user shell
-n, --nvram FILE        store nvram state in FILE
```

- `--help` displays the help shown above, and exits.
- `--version` displays the executable version number, and exits.
- `--inherit` inherit parent shell environment.
- `--firmware VER` selects the firmware version to use. Older DMD
   terminals used firmware version "8;7;3". Newer terminals used firmware
   version "8;7;5". The version must be specified as a string, and is
   "8;7;5" by default.
- `--device DEV` will attach the terminal to the specified physical or 
   virtual serial device (e.g. "/dev/ttyS0")
- `--shell SHELL` will execute the specified shell (e.g. "/bin/sh")
- `--nvram FILE` causes terminal parameters stored in non-volatile memory
   to be persisted to `FILE`.

Example usage:

```
$ dmd5620 --nvram ~/.dmd5620_nvram --shell /bin/sh
$ dmd5620 --firmware "8;7;3" --nvram ~/.dmd5620_nvram --device /dev/ttyS0
```

### Configuration

All configuration of the terminal is done by pressing the `F9` key, which shows
a series of menu buttons at the bottom of the screen. These buttons reveal
menus that allow you to set the baud rate, reverse the video colors, turn on
and off the bell, and so forth.

In firmware version "8;7;3", these buttons are navigated by pressing TAB or
BACKSPACE to move around, and pressing SPACE to change the values.

In firmware version "8;7;5", the function keys or the mouse select and change
the values.

Full documentation is available here: [https://archives.loomcom.com/3b2/documents/DMD_Terminal/](https://archives.loomcom.com/3b2/documents/DMD_Terminal/)


## Key Map

Certain keys are mapped to special DMD5620 function keys.

* F1-F8 are mapped directly to terminal F1-F8
* F9 is mapped to the DMD5620's SETUP key.
* Shift+F9 is mapped to the terminal's RESET functionality.

## Changelog

### Version 2.1.0

* Sub-processes now inherit the user's environment.
* Maximum execution rate is limited to prevent runaway starvation of
  resources and slow performance.
  
### Version 2.0.1

* Several major bug fixes to the UART code on the back end.
* Now supports the ability to run firmware version 8;7;3 and 8;7;5.

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
