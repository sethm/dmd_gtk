# GTK DMD 5620 Emulator

This is a GTK+ 3.0 implementation of an AT&T / Teletype DMD 5620 emulator.

## Status

Version: 1.3.0

This is an actively developed project.

## Dependencies

The executable has the following dependencies:

* GTK+ 3 (libgtk)
* GDK Pixbuf 2 (libgdk-pixbuf)
* Cairo 1.15+ (libcairo)
* Glib 2 (libglib)

## Usage

The terminal emulator uses the Telnet protocol to communicate with a
remote host.

```
dmd5620 -v | [-d] [-s <shell>] [-n <nvram_file>] [-- <gtk-options> ...]
```

`shell` is the shell to execute. If not specified, the user's default
login shell will be executed.

`nvram_file` is the name of a file in which to store the contents of NVRAM.
This will preserve the state of the NVRAM between runs.

## Changelog

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
