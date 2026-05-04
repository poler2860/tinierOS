{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {

    # Ensure we have a clean enviroment
	pure = true;

	buildInputs = [
	   # Get the necessary utilities
		pkgs.coreutils
		pkgs.findutils
       	pkgs.gnused
       	pkgs.gnugrep
       	pkgs.gawk
       	pkgs.bash
	pkgs.unixtools.whereis
       	pkgs.gzip
	pkgs.bison
	pkgs.flex
	pkgs.gmp
	pkgs.libmpc
	pkgs.mpfr
	pkgs.texinfo
	pkgs.isl
	pkgs.perl
	   pkgs.git
	   pkgs.gnumake
	   pkgs.valgrind
	   pkgs.doxygen
	];

}
