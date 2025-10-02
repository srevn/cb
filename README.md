# cb

A minimal terminal utility for clipboard operations using OSC52 escape sequences.

## Usage

```bash
# Copy from stdin
echo "hello world" | cb

# Copy from file
cb filename.txt

# Paste to stdout
cb

# Paste to file
cb > filename.txt
```

## Functionality

- **Copy**: Encodes input as base64 and sends to terminal clipboard via OSC52
- **Paste**: Requests clipboard content from terminal and decodes base64 output
- **Input limit**: 10MB maximum for security
- **Terminal detection**: Automatically handles piped vs terminal output
- **Multiplexer support**: Wraps OSC52 in DCS passthrough for tmux

## Build

```bash
gcc -o cb cb.c
```

or use make to also install to `/usr/local/bin`

```bash
make install
```

## Requirements

Terminal emulator with OSC52 clipboard support (most modern terminals).