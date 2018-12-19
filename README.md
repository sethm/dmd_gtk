# GTK DMD 5620 Emulator

This is a GTK+ 3.0 implementation of an AT&T / Teletype DMD 5620 emulator.

## Status

Version: 1.0.0

This is an actively developed project.

## Usage

The terminal uses the Telnet protocol to communicate with a remote
host.

```
dmd5620 -h <host> [-p <port>] [-n <nvram_file>] [-v] [-- <gtk-options> ...]
```

If not specified, `<port>` defaults to 23.

`nvram_file` is the name of a file in which to store the contents of NVRAM.
This will preserve the state of the NVRAM between runs.

## To Do

- Terminal bell is not yet implemented.
- Local serial line support is not yet implemented.

## See Also

* [dmd_core](https://github.com/sethm/dmd_core): DMD 5620 core
  implementation library, used by this project.

## Acknowledgements

* [libtelnet](https://github.com/seanmiddleditch/libtelnet): The
  telnet library used by this project. It is in the public domain,
  but I would like to thank the authors for making it available.

## License

MIT license. See the file [LICENSE.md](LICENSE.md)

Copyright (c) 2018, Seth Morabito &lt;web@loomcom.com&gt;
