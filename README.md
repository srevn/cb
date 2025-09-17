# copy

A minimal terminal utility for clipboard operations using OSC52 escape sequences.

## Usage

```bash
# Copy from stdin
echo "hello world" | copy

# Copy from file
copy filename.txt

# Paste to stdout
copy

# Paste to file
copy > filename.txt
```

## Functionality

- **Copy**: Encodes input as base64 and sends to terminal clipboard via OSC52
- **Paste**: Requests clipboard content from terminal and decodes base64 output
- **Input limit**: 10MB maximum for security
- **Terminal detection**: Automatically handles piped vs terminal output

## Build

```bash
gcc -o copy copy.c
```

or use make to also install to `/usr/local/bin`

```bash
make install
```

## Requirements

Terminal emulator with OSC52 clipboard support (most modern terminals).