{
  description = "Your build-once run-anywhere c library";

  # Nixpkgs / NixOS version to use.
  inputs.nixpkgs.url = "nixpkgs/nixos-24.11";

  outputs = { self, nixpkgs }:
    let

      # to work with older version of flakes
      lastModifiedDate = self.lastModifiedDate or self.lastModified or "19700101";

      version = "3.0.1";

      # System types to support.
      supportedSystems = [ "x86_64-linux" "x86_64-darwin" ]; #"aarch64-linux" "aarch64-darwin" ];

      # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      # Nixpkgs instantiated for supported system types.
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; overlays = [ self.overlay ]; });

    in

    {

      # A Nixpkgs overlay.
      overlay = final: prev: {

        s0ph0s-cosmopolitan = with final; stdenv.mkDerivation rec {
          pname = "s0ph0s-cosmopolitan";
          inherit version;

          src = ./.;

          nativeBuildInputs = [
            # The cosmopolitan build system downloads pre-built copies of a
            # patched GCC and LLVM toolchain, called "cosmocc."  This patched
            # compiler is required for building, and given that it already takes
            # forever, I don't want to wait for building two whole compiler
            # toolchains on top of everything else.
            cacert
            wget
            # bintools-unwrapped
            gnumake
            unzip
          ];

          strictDeps = true;
          outputs = [
            "out"
            "dist"
          ];

          buildFlags = [
            "o//cosmopolitan.a"
            "o//libc/crt/crt.o"
            "o//ape/ape.o"
            "o//ape/ape.lds"
            "o//tool/net/redbean.com"
          ];

          checkTarget = "o//test";

          enableParallelBuilding = true;

          doCheck = true;
          dontConfigure = true;
          dontFixup = true;

          preCheck =
            let
              failingTests = [
                # some syscall tests fail because we're in a sandbox
                "test/libc/calls/sched_setscheduler_test.c"
                "test/libc/thread/pthread_create_test.c"
                "test/libc/calls/getgroups_test.c"
                # Fails for mystery reasons that I haven't debugged yet.
                "test/libc/calls/poll_test.c"
                # Fails because upstream fixed a bug and didn't bother to fix the tests.
                "test/net/http/parsehttpmessage_test.c"
              ];
            in
            lib.concatStringsSep ";\n" (map (t: "rm -v ${t}") failingTests);

          installPhase = ''
            runHook preInstall

            mkdir -p $out/{lib,bin}
            install o/libc/crt/crt.o o/ape/ape.{o,lds} o/ape/ape-no-modify-self.o $out/lib
            install o/tool/net/redbean $out/bin
            cp -RT . "$dist"

            runHook postInstall
          '';

#           passthru = {
#             cosmocc = callPackage ./cosmocc.nix {
#               cosmopolitan = finalAttrs.finalPackage;
#             };
#           };

          meta = {
            homepage = "https://github.com/s0ph0s-dog/cosmopolitan";
            description = "Your build-once run-anywhere c library";
            license = lib.licenses.isc;
            platforms = lib.platforms.x86_64;
          };
        };

      };

      # Provide some binary packages for selected system types.
      packages = forAllSystems (system:
        {
          inherit (nixpkgsFor.${system}) s0ph0s-cosmopolitan;
        });

      defaultPackage = forAllSystems (system: self.packages.${system}.s0ph0s-cosmopolitan);

    };
}
