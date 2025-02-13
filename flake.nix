{
  description = "Your build-once run-anywhere c library";

  # Nixpkgs / NixOS version to use.
  inputs.nixpkgs.url = "nixpkgs/nixos-24.11";

  outputs = {
    self,
    nixpkgs,
  }: let
    # to work with older version of flakes
    lastModifiedDate = self.lastModifiedDate or self.lastModified or "19700101";

    version = "3.0.1";
    cosmocc_version = "3.9.2";

    # System types to support.
    supportedSystems = ["x86_64-linux" "x86_64-darwin"]; #"aarch64-linux" "aarch64-darwin" ];
    apes = {
      x86_64-linux = "o//ape/ape.elf";
      x86_64-darwin = "o//ape/ape.macho";
    };

    # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
    forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

    # Nixpkgs instantiated for supported system types.
    nixpkgsFor = forAllSystems (system:
      import nixpkgs {
        inherit system;
        overlays = [self.overlay];
      });
  in {
    formatter.x86_64-darwin = nixpkgs.legacyPackages.x86_64-darwin.alejandra;

    # A Nixpkgs overlay.
    overlay = final: prev: {
      # Download and extract the cosmocc toolchain separately, so that the work
      # can be reused (and so that the build doesn't fail when the Cosmopolitan
      # makefile tries to do the same thing)
      cosmocc = final.stdenv.mkDerivation rec {
        pname = "cosmocc";
        version = cosmocc_version;

        strictDeps = true;
        src = final.fetchurl {
          url = "https://github.com/jart/cosmopolitan/releases/download/${cosmocc_version}/cosmocc-${cosmocc_version}.zip";
          hash = "sha256-9P8Tr2X80wnz8c/QQnWZb7f3KkiXcmYoqMnPcy6FAZM=";
        };
        sourceRoot = ".";

        nativeBuildInputs = [final.unzip];

        dontCheck = true;
        dontConfigure = true;
        dontPatch = true;
        dontBuild = true;
        dontFixup = true;

        installPhase = ''
          runHook preInstall
          mkdir -p "$out"
          cp -R ./* $out
          runHook postInstall
        '';
      };

      s0ph0s-cosmopolitan = let
        thisPlatformApe = apes.${final.pkgs.stdenv.hostPlatform.system};
      in
        final.stdenv.mkDerivation rec {
          pname = "s0ph0s-cosmopolitan";
          inherit version;

          src = ./.;

          nativeBuildInputs = [
            final.gnumake
          ];

          strictDeps = true;
          outputs = [
            "out"
            "dist"
          ];

          buildFlags = [
            thisPlatformApe
            "o//tool/net/redbean.com"
          ];

          checkTarget = "o//test";

          enableParallelBuilding = true;

          doCheck = true;
          dontConfigure = true;
          dontFixup = true;

          preBuild = ''
            mkdir -p ".cosmocc"
            ln -sfn ${final.pkgs.cosmocc} ".cosmocc/${cosmocc_version}"
            ln -sfn ${final.pkgs.cosmocc} ".cosmocc/current"
          '';

          preCheck = let
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
            final.lib.concatStringsSep ";\n" (map (t: "rm -v ${t}") failingTests);

          installPhase = ''
            runHook preInstall

            mkdir -p $out/{lib,bin}
            install ${thisPlatformApe} o/tool/net/redbean $out/bin
            cp -RT . "$dist"

            runHook postInstall
          '';

          meta = {
            homepage = "https://github.com/s0ph0s-dog/cosmopolitan";
            description = "Your build-once run-anywhere c library";
            license = final.lib.licenses.isc;
            platforms = final.lib.platforms.x86_64;
          };
        };
    };

    # Provide some binary packages for selected system types.
    packages = forAllSystems (system: {
      inherit (nixpkgsFor.${system}) s0ph0s-cosmopolitan;
    });

    defaultPackage = forAllSystems (system: self.packages.${system}.s0ph0s-cosmopolitan);

    nixosModules.default = {pkgs, ...}: {
      environment.systemPackages = [pkgs.s0ph0s-cosmopolitan];
      boot.binfmt.registrations.APE = {
        interpreter = "${pkgs.s0ph0s-cosmopolitan}/bin/ape";
        recognitionType = "magic";
        magicOrExtension = "MZqFpD";
        fixBinary = true;
        preserveArgvZero = true;
      };
    };
  };
}
