{ pkgs ? import <nixpkgs> {} }:

let
  avrPkgs = pkgs.pkgsCross.avr.buildPackages;
in

pkgs.mkShell {
  name = "tinierOS-avr-dev";

  packages = [
    # AVR toolchain
    avrPkgs.gcc
    avrPkgs.binutils

    # Flashing tool
    pkgs.avrdude

    # Build system
    pkgs.gnumake

    # Useful for inspecting ELF output
    pkgs.python3  # for any scripting/analysis

    # Optional: GDB for AVR (pairs with scripts/tinyos-gdb.gdb)
    # avrPkgs.gdb
  ];

  shellHook = ''
    echo "-------------------------------------------"
    echo " TinierOS AVR Dev Shell"
    echo " MCU: ATmega328P @ 16MHz"
    echo "-------------------------------------------"
    echo "Toolchain:"
    avr-gcc --version | head -1
    avr-objcopy --version | head -1
    avrdude -v 2>&1 | head -1
    echo ""
    echo "Run 'make' to build, 'make flash' to upload"
    echo "-------------------------------------------"
  '';
}
