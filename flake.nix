{
  description = "cgzip";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    flamegraph.url = "git+ssh://git@github.com/callumcurtis/snippets?dir=topic/profiling/flamegraph";
    flamegraph.inputs.nixpkgs.follows = "nixpkgs";
    flamegraph.inputs.flake-utils.follows = "flake-utils";
  };

  outputs = { nixpkgs, flake-utils, flamegraph, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        gzstat = pkgs.stdenv.mkDerivation {
          pname = "gzstat";
          version = "1.0";
          src = pkgs.fetchFromGitHub {
            owner = "billbird";
            repo = "gzstat";
            rev = "fd6aeaeacf26ad13e22150038600c3461af35b94";
            sha256 = "sha256-DcisxhZH3H0sn7y/BPg2k6eu3aGmJ13u3bmpHyIOSKc=";
          };
          dontBuild = true;
          installPhase = ''
            mkdir -p $out/bin
            echo '#!${pkgs.python313}/bin/python' > $out/bin/gzstat
            cat $src/gzstat.py >> $out/bin/gzstat
            chmod +x $out/bin/gzstat
          '';
        };
      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            catch2_3
            python313
            llvmPackages_20.clang-tools
            flamegraph.packages.${system}.default
            gzstat
          ];
        };
      }
    );
}
