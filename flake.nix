{
  description = "X-56 HOTAS Control System";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
  };

  outputs = { self, nixpkgs }:
let
  systems = [ "x86_64-linux" "aarch64-linux" ];
  forEachSystem = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
in
  {
    packages = forEachSystem (pkgs: {
      x56d = pkgs.stdenv.mkDerivation {
        pname = "x56d";
        version = "1.0.0";
        src = self;
        buildInputs = with pkgs; [ gcc pkg-config libusb1.dev];
        buildPhase = ''
          gcc -o x56d -Wall -lm src/main.cpp src/usb.cpp $(${pkgs.pkg-config}/bin/pkg-config --cflags --libs libusb-1.0)
        '';
        installPhase = ''
          install -Dm755 x56d $out/bin/x56d
        '';
      };
        
      x56-ctrl = pkgs.stdenv.mkDerivation {
        pname = "x56-ctrl";
        version = "1.0.0";
        src = self;
        buildInputs = with pkgs; [ gcc pkg-config libusb1];
        buildPhase = ''
          gcc -o x56-ctrl -Wall -lm src/x56-ctrl.cpp $(${pkgs.pkg-config}/bin/pkg-config --cflags --libs libusb-1.0)
        '';
        installPhase = ''
          install -Dm755 x56-ctrl $out/bin/x56-ctrl
        '';
      };

      x56-udev = pkgs.stdenv.mkDerivation {
        pname = "x56-udev";
        version = "1.0.0";
        src = self;
        buildPhase = ''
        '';
        installPhase = ''
          install -Dm644 udev/99-x56.rules $out/udev/rules.d/99-x56.rules
        '';
      };
        
      x56-systemd = pkgs.stdenv.mkDerivation {
        pname = "x56-systemd";
        version = "1.0.0";
        src = self;
        buildPhase = ''
        '';
        installPhase = ''
          install -Dm644 systemd/x56d.service $out/systemd/system/x56d.service
        '';
      };

      x56 = pkgs.runCommand "x56" {} ''
        mkdir -p $out/bin
        mkdir -p $out/udev/rules.d
        mkdir -p $out/systemd/system
        cp ${self.packages.${pkgs.system}.x56d}/bin/x56d $out/bin/
        cp ${self.packages.${pkgs.system}.x56-ctrl}/bin/x56-ctrl $out/bin/
        cp ${self.packages.${pkgs.system}.x56-udev}/udev/rules.d/99-x56.rules $out/udev/rules.d
        cp ${self.packages.${pkgs.system}.x56-systemd}/systemd/system/x56d.service $out/systemd/system
      '';

      default = self.packages.${pkgs.system}.x56;
    });

    devShells = forEachSystem (pkgs: {
      default = pkgs.mkShell {
        name = "x56d";
        buildInputs = with pkgs; [ gcc pkg-config libusb1.dev];
        shell = pkgs.zsh;
        shellHook = ''
          exec ${pkgs.zsh}/bin/zsh;   # Replace bash with zsh
          source ~/.zshrc;
          export PKG_CONFIG_PATH="${pkgs.libusb1.dev}/lib/pkg-config:${pkgs.hidapi}/lib/pkg-config:$PKG_CONFIG_PATH"
          echo "x56d dev shell"
          echo "Build: gcc -o x56d -Wall src/*.cpp $(pkg-config --cflags --libs libusb-1.0)"
        '';
      };
    });
  };
}
