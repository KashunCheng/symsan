{
  description = "SymSan: Time and Space Efficient Concolic Execution via Dynamic Data-Flow Analysis";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";

    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
        llvmPackages = pkgs.llvmPackages_14;
        llvm = llvmPackages;
        stdenv = pkgs.callPackage ./nix-support/default.nix { inherit llvmPackages; };
        python3 = pkgs.python3.withPackages (python-pkgs: with python-pkgs; [
          lit
        ]);
        nativeBuildInputs = with pkgs; [ cmake ];
        buildInputs = with pkgs; [
          boost
          libxcrypt
          llvm.libllvm
          llvm.libcxx
          llvm.libunwind
          python3
          z3
        ];
      in
      with pkgs;
      {
        devShells = {
          default = (mkShell.override { inherit stdenv; }) {
            buildInputs = [ pkgs.bashInteractive ];
            packages = nativeBuildInputs ++ buildInputs ++ (with pkgs; [ self.packages.${system}.default zlib ]);
          };
        };
        
        packages = {
          default = stdenv.mkDerivation {
            name = "symsan";
            src = self;
            inherit buildInputs nativeBuildInputs;
            postInstall = ''
              lit tests --verbose
            '';
          };
        };

        formatter = nixpkgs-fmt;
      }
    );
}
