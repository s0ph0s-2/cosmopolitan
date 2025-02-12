{ runCommand, s0ph0s-cosmopolitan }:

let
  cosmocc =
    runCommand "cosmocc-${s0ph0s-cosmopolitan.version}"
      {
        pname = "cosmocc";
        inherit (s0ph0s-cosmopolitan) version;

        passthru.tests = {
          cc = runCommand "c-test" { } ''
            ${cosmocc}/bin/cosmocc ${./hello.c}
            ./a.out > $out
          '';
        };

        meta = s0ph0s-cosmopolitan.meta // {
          description = "compilers for Cosmopolitan C/C++ programs";
        };
      }
      ''
        mkdir -p $out/bin
        install ${s0ph0s-cosmopolitan.dist}/tool/scripts/{cosmocc,cosmoc++} $out/bin
        sed 's|/opt/cosmo\([ /]\)|${s0ph0s-cosmopolitan.dist}\1|g' -i $out/bin/*
      '';
in
cosmocc
