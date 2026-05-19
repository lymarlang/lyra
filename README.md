# Lyra Package Manager
Lyra is the official package manager for the Lymar (Limitly) programming language.

## Configuration
Lyra uses the **NOL (Notation Object Language)** format for its configuration files:
- `lymar.nol`: The project manifest.
- `lymar.lock`: The dependency lockfile.

## Security
Lyra uses OpenSSL for cryptographic operations, including SHA256 integrity verification of packages. All external commands are executed with audited path handling to prevent shell injection.
