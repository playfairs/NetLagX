# NetLagX

>[!IMPORTANT]
> NetLagX currently may not work as it is not in a stable state and needs improvisons. If you would like to contribute, make a PR, or DM **@playfairs** on Discord, and I can probably add you to the Repository.

A powerful CLI/GUI application for network traffic manipulation and lag simulation. NetLagX allows you to intercept, delay, drop, or throttle network packets for testing, debugging, or educational purposes.

## Features

- **Multiple Operation Modes**: OFF, LAG, DROP, THROTTLE
- **Packet Interception**: Raw socket-based packet capture and manipulation
- **Configurable Parameters**: Delay, drop rate, throttle rate
- **Target Filtering**: Filter by IP address and/or port
- **Dual Interface**: Both CLI and GUI versions available
- **Real-time Control**: Start/stop and adjust parameters on the fly
- **Traffic Statistics**: Monitor packet rates and throughput
- **Cross-platform**: Works on macOS, Linux, and other Unix-like systems

## Requirements

- GCC compiler
- GTK+3 (for GUI version, optional)
- Root/administrator privileges (for packet interception)

### Installation on macOS

```bash
xcode-select --install

brew install gtk+3
```

### Installation on Linux

```bash
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev

sudo yum groupinstall "Development Tools"
sudo yum install gtk3-devel
```

## Building

### Traditional Build (Make)

```bash
make all

make cli

make gui

make clean

make help
```

### Nix Development Environment

#### Using Flakes (Recommended)

```bash
nix develop

nix develop .#cli

nix build .#cli
nix build .#gui
nix build .

nix run .#cli
nix run .#gui
nix run .


nix flake check
```

#### Using direnv (Optional)

```bash
echo 'eval "$(direnv hook bash)"' >> ~/.bashrc
echo 'eval "$(direnv hook zsh)"' >> ~/.zshrc

direnv allow
```

#### Nix Shell (Legacy)

```bash
nix-shell

nix-shell --attr cli
```

### Nix Environment Features

The Nix development environment includes:

- **Build Tools**: GCC, pkg-config, make
- **GUI Dependencies**: GTK+3, glib, pango, cairo
- **Development Tools**: gdb, valgrind, strace, clang-tools
- **Static Analysis**: cppcheck, clang-tidy, splint
- **Runtime Utilities**: iproute2, nettools

All dependencies are reproducibly built and isolated from your system.

## Usage

### CLI Mode

```bash
sudo ./bin/netlagx-cli

sudo make install
sudo netlagx

sudo netlagx -i 192.168.1.100 -p 80 -d 200 -m lag
```

#### CLI Commands

The interactive CLI provides the following options:

1. **Start lag switch** - Begin packet interception
2. **Stop lag switch** - Stop packet interception
3. **Show configuration** - Display current settings
4. **Change mode** - Switch between OFF/LAG/DROP/THROTTLE
5. **Change delay** - Set lag delay in milliseconds (0-5000ms)
6. **Change drop rate** - Set packet drop probability (0.0-1.0)
7. **Change throttle rate** - Set throttle probability (0.0-1.0)
8. **Set target IP** - Filter packets by destination IP
9. **Set target port** - Filter packets by destination port
0. **Exit** - Quit the application

### GUI Mode

```bash
sudo ./bin/netlagx-gui

sudo make install
sudo netlagx-gui
```

The GUI provides intuitive controls for:
- Mode selection (OFF/LAG/DROP/THROTTLE)
- Delay adjustment (spin button)
- Drop rate configuration
- Throttle rate configuration
- Target IP and port filtering
- Real-time status display
- Activity logging

### Command Line Options

```
Usage: netlagx [options]

Options:
  -g, --gui          Run GUI interface (default: CLI)
  -i, --ip <IP>      Target IP address
  -p, --port <PORT>  Target port
  -d, --delay <MS>   Delay in milliseconds (default: 100)
  -m, --mode <MODE>  Mode: off, lag, drop, throttle (default: lag)
  -r, --drop-rate <RATE>  Packet drop rate 0.0-1.0 (default: 0.1)
  -t, --throttle-rate <RATE>  Throttle rate 0.0-1.0 (default: 0.5)
  -h, --help         Show help message
```

## Operation Modes

### OFF Mode
- No packet manipulation
- Packets pass through normally

### LAG Mode
- All packets are delayed by the specified amount
- Useful for simulating high-latency connections

### DROP Mode
- Randomly drops packets based on the drop rate
- Simulates packet loss scenarios

### THROTTLE Mode
- Randomly throttles (delays) packets based on throttle rate
- Simulates bandwidth limitations

## Examples

### Basic Lag Simulation
```bash
sudo netlagx -d 200 -m lag
```

### Targeted Packet Dropping
```bash
sudo netlagx -i 192.168.1.100 -p 80 -r 0.3 -m drop
```

### Mixed Configuration
```bash
sudo netlagx -d 100 -t 0.5 -m throttle
```

## Security and Permissions

NetLagX requires root privileges because it:
- Creates raw sockets for packet interception
- Manipulates network traffic at the packet level
- Needs access to low-level network operations

Always review the code and understand what the application does before running with elevated privileges.

## Troubleshooting

### Permission Denied
```bash
sudo netlagx
```

### GTK+3 Not Found
```bash
# Install GTK+3 development libraries
# macOS: brew install gtk+3
# Ubuntu: sudo apt-get install libgtk-3-dev
# CentOS: sudo yum install gtk3-devel
```

### Build Errors
```bash
make clean
make all
```

## Disclaimer

This tool is designed for legitimate testing, debugging, and educational purposes only. Users are responsible for ensuring compliance with local laws and network policies. The authors are not responsible for misuse of this software.