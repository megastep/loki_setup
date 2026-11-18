# Repository Guidelines

## Project Structure

Loki Setup is a C installer driven by a package’s XML description. Start in `main.c` for command-line handling, `install.c` for package options, and `copy.c` for file installation. The console, GTK, dialog, and Carbon front ends share the interface in `install_ui.h`.

Archive handlers live in `plugins/`. Use `plugins/sample.c` and `README.plugins` when adding a format. The distribution template lives in `image/`, with the launcher at `image/setup.sh` and the example manifest, Glade layouts, and splash image in `image/setup.data/`. Consult `README.xml` for manifest syntax and `po/` for translations.

## Build and Run

From the repository root, build the sibling SetupDB library first. The installer’s configure script looks for its architecture-specific `libsetupdb.a`.

```sh
(cd ../loki_setupdb && ./autogen.sh && ./configure && make)
./autogen.sh && ./configure && make
```

Run `./configure --help` to select libraries and optional front ends. Both projects default to libxml2. Use `--enable-gtk2` to require the GTK front end. Edit `configure.ac` and `Makefile.in` for build changes.

Run `./setup -n -c image` to open the example in text mode. Use `make install` to stage built binaries under `image/setup.data/bin/` for packaging.

## Coding Style

Match the surrounding C code’s indentation, including its use of tabs or four spaces. Follow existing names such as `create_install` for core functions and `*_ui.c` for front ends. Keep Makefile recipe lines tab-indented.

## Testing

Run `make smoke` in a terminal to stage media, install a compressed fixture, verify its registry entry, and run the generated uninstaller. Root-level probes follow the `test<subject>.c` pattern. Build the CD-ROM probe with `make testcd` and run `./testcd` on a host with suitable media. The `make check` target builds the installed-file verification and rescue utility.

For installer changes, exercise the affected front end with a small package in a temporary install directory. Verify the files installed by the selected option and their removal on uninstall.

## Commits and Pull Requests

Use a short subject describing the change, following examples such as `FreeBSD 64-bit support`. In a pull request, identify the affected platform or front end and record the configure flags, commands, and observed results so reviewers can repeat the check.
