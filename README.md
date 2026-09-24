# Lyra Package Manager
Lyra is the official package manager for the Lymar (Limitly) programming language.

## Configuration
Lyra uses the **NOL (Notation Object Language)** format for its configuration files:
- `lymar.nol`: The project manifest.
- `lymar.lock`: The dependency lockfile.

## Central Package Index
Lyra uses a **Git-Backed Central Index** (similar to Cargo, Homebrew, and CocoaPods) hosted at:
`https://github.com/lymarlang/index.git`

The local index is cloned and cached at `~/.lyra/index`. You can customize the index URL using the `LYRA_INDEX_URL` environment variable.

### Supported Dependency Types

In `lymar.nol`:
```nol
dependencies: {
  # 1. Central Index registry dependency (resolved via SemVer from lymarlang/index)
  math: "^1.0.0",

  # 2. Direct Git dependency with branch, tag, or commit SHA
  http: {
    git: "https://github.com/lymarlang/http.git",
    tag: "v0.2.0"
  },

  # 3. GitHub shortcut
  json: {
    github: "lymarlang/json",
    rev: "main"
  },

  # 4. Local path dependency
  utils: {
    path: "../utils"
  }
}
```

## Security
Lyra uses OpenSSL for cryptographic operations, including SHA256 integrity verification of packages. All external commands are executed with audited path handling to prevent shell injection.
