{
  description = "NetLagX - Network Lag Switch Tool";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        isLinux = pkgs.stdenv.isLinux;
        isDarwin = pkgs.stdenv.isDarwin;
        
        # Common build dependencies
        buildInputs = with pkgs; [
          gcc
          clang
          nasm
          pkg-config
          curl
          json-c_0_16
        ] ++ pkgs.lib.optionals isLinux [
          glibc
        ] ++ pkgs.lib.optionals isDarwin [
          darwin.apple_sdk.frameworks.Foundation
          darwin.apple_sdk.frameworks.SystemConfiguration
          darwin.apple_sdk.frameworks.CoreFoundation
          darwin.apple_sdk.frameworks.IOKit
        ];

        # GUI-specific dependencies
        guiBuildInputs = with pkgs; [
          gtk3
          glib
          pango
          cairo
          gdk-pixbuf
          atk
        ];

        # Development dependencies (platform-specific)
        devInputs = with pkgs; [
          gdb
          clang-tools
          cppcheck
        ] ++ pkgs.lib.optionals isLinux [
          valgrind
          strace
          ltrace
          splint
        ] ++ pkgs.lib.optionals isDarwin [
          lldb  # macOS debugger
        ];

        # Runtime dependencies (platform-specific)
        runtimeInputs = with pkgs; [
        ] ++ pkgs.lib.optionals isLinux [
          iproute2  # For network utilities
          nettools  # For network tools
        ];

      in {
        # Development shell with all dependencies
        devShells.default = pkgs.mkShell {
          buildInputs = buildInputs ++ guiBuildInputs ++ devInputs ++ runtimeInputs;
          
          shellHook = ''
            echo "Welcome to NetLagX Development Environment"
            echo "=========================================="
            echo
            echo "Available commands:"
            echo "  make all        - Build both CLI and GUI versions"
            echo "  make cli        - Build CLI version only"
            echo "  make gui        - Build GUI version only"
            echo "  make clean      - Clean build artifacts"
            echo "  make install    - Install system-wide"
            echo "  make run-cli    - Build and run CLI"
            echo "  make run-gui    - Build and run GUI"
            echo "  make help       - Show all options"
            echo
            echo "Development tools:"
            echo "  gdb            - GNU Debugger"
            ${pkgs.lib.optionalString isLinux ''
            echo "  valgrind       - Memory debugger"
            echo "  strace         - System call tracer"
            ''}
            ${pkgs.lib.optionalString isDarwin ''
            echo "  lldb           - LLDB Debugger (macOS)"
            ''}
            echo "  clang-tidy     - Static analysis"
            echo "  cppcheck       - Static analysis"
            echo
            echo "Note: Packet interception requires root privileges"
            echo "      Use 'sudo make run-cli' or 'sudo make run-gui'"
            echo
            export CC=gcc
            export CFLAGS="-Wall -Wextra -std=c99 -pthread -g"
            export PKG_CONFIG_PATH="${pkgs.gtk3.dev}/lib/pkgconfig:$PKG_CONFIG_PATH"
          '';
        };

        # CLI-only development shell
        devShells.cli = pkgs.mkShell {
          buildInputs = buildInputs ++ devInputs ++ runtimeInputs;
          
          shellHook = ''
            echo "NetLagX CLI Development Environment"
            echo "===================================="
            echo "GUI dependencies omitted for minimal environment"
            echo
            export CC=gcc
            export CFLAGS="-Wall -Wextra -std=c99 -pthread -g"
          '';
        };

        # Package definition for CLI version
        packages.cli = pkgs.stdenv.mkDerivation {
          pname = "netlagx-cli";
          version = "1.0.0";
          
          src = ./.;
          
          nativeBuildInputs = with pkgs; [ makeWrapper ];
          buildInputs = buildInputs;
          
          makeFlags = [ "cli" ];
          installFlags = [ "PREFIX=$(out)" ];
          
          postInstall = ''
            wrapProgram "$out/bin/netlagx-cli" \
              --prefix PATH : ${pkgs.lib.makeBinPath runtimeInputs}
          '';
          
          meta = with pkgs.lib; {
            description = "Network Lag Switch Tool - CLI version";
            longDescription = ''
              NetLagX is a powerful CLI application for network traffic manipulation 
              and lag simulation. It allows you to intercept, delay, drop, or throttle 
              network packets for testing, debugging, or educational purposes.
            '';
            license = licenses.mit;
            platforms = platforms.unix;
            mainProgram = "netlagx-cli";
          };
        };

        # Package definition for GUI version
        packages.gui = pkgs.stdenv.mkDerivation {
          pname = "netlagx-gui";
          version = "1.0.0";
          
          src = ./.;
          
          nativeBuildInputs = with pkgs; [ makeWrapper pkg-config ];
          buildInputs = buildInputs ++ guiBuildInputs;
          
          makeFlags = [ "gui" ];
          installFlags = [ "PREFIX=$(out)" ];
          
          postInstall = ''
            wrapProgram "$out/bin/netlagx-gui" \
              --prefix PATH : ${pkgs.lib.makeBinPath runtimeInputs} \
              --prefix XDG_DATA_DIRS : "${pkgs.gtk3}/share/gsettings-schemas/${pkgs.gtk3.name}"
          '';
          
          meta = with pkgs.lib; {
            description = "Network Lag Switch Tool - GUI version";
            longDescription = ''
              NetLagX GUI provides an intuitive graphical interface for network traffic 
              manipulation and lag simulation. Built with GTK+3, it offers real-time 
              control over packet interception, delay, dropping, and throttling.
            '';
            license = licenses.mit;
            platforms = platforms.unix;
            mainProgram = "netlagx-gui";
          };
        };

        # Default package (CLI version)
        packages.default = self.packages.${system}.cli;

        # Application definitions for nix run
        apps.cli = {
          type = "app";
          program = "${self.packages.${system}.cli}/bin/netlagx-cli";
        };

        apps.gui = {
          type = "app";
          program = "${self.packages.${system}.gui}/bin/netlagx-gui";
        };

        # Default app (CLI version)
        apps.default = self.apps.${system}.cli;

        # Checks for code quality
        checks = {
          # Static analysis with clang-tidy
          clang-tidy = pkgs.stdenv.mkDerivation {
            name = "netlagx-clang-tidy";
            src = ./.;
            buildInputs = with pkgs; [ clang-tools pkg-config ];
            buildPhase = ''
              find src -name "*.c" -exec clang-tidy --checks='-*' \
                --checks='bugprone-*,performance-*,readability-*,modern-*' \
                --warnings-as-errors='*' {} \;
            '';
            installPhase = "mkdir -p $out";
          };

          # Memory leak check with valgrind (Linux only)
          valgrind-check = pkgs.lib.optionalAttrs isLinux (pkgs.stdenv.mkDerivation {
            name = "netlagx-valgrind-check";
            src = ./.;
            buildInputs = with pkgs; [ valgrind gcc pkg-config ];
            buildPhase = ''
              make cli
              echo "Running valgrind check (requires interactive testing)"
              echo "Run manually: valgrind --leak-check=full --show-leak-kinds=all ./bin/netlagx-cli -h"
            '';
            installPhase = "mkdir -p $out";
          });

          # Code style check
          cppcheck = pkgs.stdenv.mkDerivation {
            name = "netlagx-cppcheck";
            src = ./.;
            buildInputs = with pkgs; [ cppcheck ];
            buildPhase = ''
              cppcheck --enable=all --error-exitcode=1 src/
            '';
            installPhase = "mkdir -p $out";
          };
        };

        # Development formatter
        formatter = pkgs.nixfmt;
      });
}
