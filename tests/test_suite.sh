#!/usr/bin/env bash
set -e

LYRA_BIN="$(pwd)/bin/lyra"

echo "=== Running End-to-End Test Suite for Lyra ==="

# 1. SemVer & Resolver Tests
echo "--- Testing SemVer & Constraint Resolver ---"
cat << 'EOF' > test_semver_e2e.cpp
#include <cassert>
#include <iostream>
#include "include/semver.hh"

int main() {
    assert(Semver::satisfies("1.2.3", "^1.2.0"));
    assert(Semver::satisfies("1.2.3", ">=1.0.0, <2.0.0"));
    assert(Semver::satisfies("1.5.0", "1.0.0 - 2.0.0"));
    assert(Semver::satisfies("1.2.9", "1.2.x"));
    assert(Semver::satisfies("1.0.0-beta.1", "1.0.0-beta.1"));
    Version v1 = Version::parse("1.0.0-alpha.1");
    Version v2 = Version::parse("1.0.0-beta.1");
    assert(v1 < v2);
    return 0;
}
EOF
g++ -std=c++17 -Iinclude test_semver_e2e.cpp -o test_semver_e2e
./test_semver_e2e
rm -f test_semver_e2e test_semver_e2e.cpp
echo "SemVer tests: OK"

# 2. In-process Tar Packing & Installing
echo "--- Testing In-Process Tarball Pack & Install ---"
TEST_TMP=$(mktemp -d)
cd "$TEST_TMP"
"$LYRA_BIN" init
"$LYRA_BIN" pack
test -f *-0.1.0.tar.gz
TARBALL=$(ls *-0.1.0.tar.gz)
"$LYRA_BIN" install "$TARBALL"
cd - > /dev/null
rm -rf "$TEST_TMP"
echo "In-process tar tests: OK"

# 3. Security Audit
echo "--- Testing Security Audit ---"
TEST_AUDIT=$(mktemp -d)
cd "$TEST_AUDIT"
"$LYRA_BIN" init
"$LYRA_BIN" audit
mkdir -p deps/vuln
cat << 'EOF' > deps/vuln/lymar.nol
package: { name: "jwt-auth", version: "1.0.0" }
EOF
cat << 'EOF' > lymar.nol
package: { name: "test_audit", version: "0.1.0" }
dependencies: {
  jwt-auth: { path: "deps/vuln" }
}
EOF
if "$LYRA_BIN" audit; then
    echo "Expected audit to fail on vulnerable package"
    exit 1
fi
cd - > /dev/null
rm -rf "$TEST_AUDIT"
echo "Security audit tests: OK"

# 4. Formatter & LSP
echo "--- Testing Formatter & LSP ---"
TEST_FMT=$(mktemp -d)
cd "$TEST_FMT"
"$LYRA_BIN" init
cat << 'EOF' > src/main.lm
fn test() {
print("bad indent");
}
EOF
if "$LYRA_BIN" fmt --check; then
    echo "Expected fmt --check to detect unformatted code"
    exit 1
fi
"$LYRA_BIN" fmt
"$LYRA_BIN" fmt --check
"$LYRA_BIN" lsp
cd - > /dev/null
rm -rf "$TEST_FMT"
echo "Formatter & LSP tests: OK"

# 5. Toolchains
echo "--- Testing Toolchain Manager ---"
"$LYRA_BIN" toolchain list
"$LYRA_BIN" toolchain install 0.9.0
"$LYRA_BIN" toolchain use 0.9.0
"$LYRA_BIN" toolchain list | grep "0.9.0 (active"
"$LYRA_BIN" toolchain uninstall 0.9.0
echo "Toolchain tests: OK"

# 6. Packaging formats (AppImage, Snap, Flatpak, Zip, DMG)
echo "--- Testing Multi-Target Packaging ---"
TEST_PKG=$(mktemp -d)
cd "$TEST_PKG"
"$LYRA_BIN" init
"$LYRA_BIN" appimage
"$LYRA_BIN" snap
"$LYRA_BIN" flatpak
"$LYRA_BIN" zip
"$LYRA_BIN" dmg
test -f *.AppImage
test -f *.snap
test -f *.flatpak
test -f *.zip
test -f *.dmg
cd - > /dev/null
rm -rf "$TEST_PKG"
echo "Multi-target packaging tests: OK"

# 7. Vendoring & Offline Mode
echo "--- Testing Vendoring & Offline Mode ---"
TEST_VENDOR=$(mktemp -d)
cd "$TEST_VENDOR"
"$LYRA_BIN" init
mkdir -p deps/foo
cat << 'EOF' > deps/foo/lymar.nol
package: { name: "foo", version: "1.0.0" }
EOF
cat << 'EOF' > lymar.nol
package: { name: "test_vendor", version: "0.1.0" }
dependencies: {
  foo: { path: "deps/foo" }
}
EOF
"$LYRA_BIN" vendor
test -d vendor/foo
cd - > /dev/null
rm -rf "$TEST_VENDOR"
echo "Vendoring & offline tests: OK"

echo "=== All Lyra End-to-End Tests Passed! ==="
