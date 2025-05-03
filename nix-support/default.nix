{ llvmPackages, bash, stdenvNoCC, overrideCC, python3, wrapCC, ... }:
let
  libstdcxxClang = llvmPackages.libstdcxxClang;
  libcxxClang = llvmPackages.libcxxClang;
  isClang = true;
  customClang =
    stdenvNoCC.mkDerivation {
      name = "clang-custom-wrapper";
      src = ./patch_clang.py;
      inherit isClang;
      dontUnpack = true;
      dontBuild = true;
      dontConfigure = true;
      enableParallelBuilding = true;

      setupHooks = libstdcxxClang.setupHooks;

      buildInputs = [
        bash
        python3
      ];

      installPhase = ''
        ${python3}/bin/python3 ${./patch_clang.py} ${bash}/bin/bash $out ${libstdcxxClang} ${libcxxClang}
      '';
    };
  wrapperClang = (wrapCC customClang).overrideAttrs (final: prev: {
    postFixup = prev.postFixup + ''
      rm -rf $out/bin
      ln -s ${customClang}/bin $out/bin
    '';
  });
in
(overrideCC llvmPackages.stdenv wrapperClang)
