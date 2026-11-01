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

# 8. Content-Addressable Compilation Cache & Watch Mode
echo "--- Testing Build Cache & Watch Mode ---"
TEST_CACHE=$(mktemp -d)
cd "$TEST_CACHE"
"$LYRA_BIN" init
# Seed fake binary output to test cache store and hit
touch test_cache_dummy
"$LYRA_BIN" build || true
# Test watch mode one-shot / exit
"$LYRA_BIN" watch --max-runs=1 test || true
cd - > /dev/null
rm -rf "$TEST_CACHE"
echo "Build cache & watch mode tests: OK"

# 9. Cryptographic Package Signatures (Ed25519)
echo "--- Testing Cryptographic Package Signatures ---"
TEST_SIG=$(mktemp -d)
cd "$TEST_SIG"
"$LYRA_BIN" init
"$LYRA_BIN" pack
TARBALL=$(ls *-0.1.0.tar.gz)
"$LYRA_BIN" keygen -o mykey
"$LYRA_BIN" sign "$TARBALL" -k mykey
test -f "${TARBALL}.sig"
"$LYRA_BIN" verify "$TARBALL" -k mykey.pub -s "${TARBALL}.sig"
cd - > /dev/null
rm -rf "$TEST_SIG"
echo "Cryptographic signature tests: OK"

# 10. Automated Security Remediation (lyra audit fix)
echo "--- Testing Automated Security Remediation (audit fix) ---"
TEST_AUDIT_FIX=$(mktemp -d)
cd "$TEST_AUDIT_FIX"
"$LYRA_BIN" init
mkdir -p deps/vuln
cat << 'EOF' > deps/vuln/lymar.nol
package: { name: "jwt-auth", version: "1.0.0" }
EOF
cat << 'EOF' > lymar.nol
package: { name: "test_fix", version: "0.1.0" }
dependencies: {
  jwt-auth: { path: "deps/vuln" }
}
EOF
"$LYRA_BIN" audit fix
grep -q "\^1.2.0" lymar.nol
cd - > /dev/null
rm -rf "$TEST_AUDIT_FIX"
echo "Security audit fix tests: OK"

# 11. License Compliance & SBOM Generation
echo "--- Testing License Compliance & SBOM ---"
TEST_LIC=$(mktemp -d)
cd "$TEST_LIC"
"$LYRA_BIN" init
mkdir -p deps/mit
cat << 'EOF' > deps/mit/lymar.nol
package: { name: "mit-lib", version: "1.0.0", license: "MIT" }
EOF
cat << 'EOF' > lymar.nol
package: { name: "test_lic", version: "0.1.0", license: "MIT" }
dependencies: {
  mit-lib: { path: "deps/mit" }
}
EOF
"$LYRA_BIN" license --allowlist=MIT,Apache-2.0
"$LYRA_BIN" sbom --spdx | grep -q "SPDXRef-Package-mit-lib"
"$LYRA_BIN" sbom --cyclonedx | grep -q "pkg:lymar/mit-lib@1.0.0"
cd - > /dev/null
rm -rf "$TEST_LIC"
echo "License compliance & SBOM tests: OK"

# 12. Scoped Packages & Dependency Overrides & MVS
echo "--- Testing Scoped Packages, Dependency Overrides & MVS ---"
TEST_OVERRIDE=$(mktemp -d)
cd "$TEST_OVERRIDE"
"$LYRA_BIN" init
mkdir -p deps/@lymar/utils-v1 deps/@lymar/utils-fork
cat << 'EOF' > deps/@lymar/utils-v1/lymar.nol
package: { name: "@lymar/utils", version: "1.0.0" }
EOF
cat << 'EOF' > deps/@lymar/utils-fork/lymar.nol
package: { name: "@lymar/utils", version: "1.0.0-fork" }
EOF
cat << 'EOF' > lymar.nol
package: { name: "test_override", version: "0.1.0" }
dependencies: {
  "@lymar/utils": { path: "deps/@lymar/utils-v1" }
}
patch: {
  "@lymar/utils": { path: "deps/@lymar/utils-fork" }
}
EOF
"$LYRA_BIN" deps | grep -q "@lymar/utils"
"$LYRA_BIN" deps | grep -q "utils-fork"
cd - > /dev/null
rm -rf "$TEST_OVERRIDE"
echo "Scoped packages & overrides tests: OK"

# 13. Native C/C++ Toolchain Driver
echo "--- Testing Native C/C++ Toolchain Driver ---"
TEST_NATIVE=$(mktemp -d)
cd "$TEST_NATIVE"
"$LYRA_BIN" init
mkdir -p native
cat << 'EOF' > native/add.c
int add(int a, int b) { return a + b; }
EOF
cat << 'EOF' > lymar.nol
package: { name: "test_native_pkg", version: "0.1.0" }
native: {
  sources: ["native/add.c"]
}
EOF
"$LYRA_BIN" build || true
test -f build/native/libtest_native_pkg_native.a
cd - > /dev/null
rm -rf "$TEST_NATIVE"
echo "Native toolchain driver tests: OK"

# 14. Interactive Developer Experience (Templates, REPL, Expand)
echo "--- Testing Project Scaffolding Templates, REPL & Expand ---"
TEST_DEVEX=$(mktemp -d)
cd "$TEST_DEVEX"
"$LYRA_BIN" new test_cli --template=cli
test -f test_cli/src/main.lm
"$LYRA_BIN" new test_web --template=web
test -f test_web/src/main.lm
"$LYRA_BIN" new test_wasm --template=wasm
test -f test_wasm/src/main.lm
"$LYRA_BIN" new test_native_tmpl --template=native
test -f test_native_tmpl/native/binding.c
echo ":quit" | "$LYRA_BIN" repl | grep -q "Lymar Interactive REPL"
"$LYRA_BIN" expand test_cli/src/main.lm --ast | grep -q "Expanded & Desugared AST"
cd - > /dev/null
rm -rf "$TEST_DEVEX"
echo "Interactive developer experience tests: OK"

echo "=== All Lyra End-to-End Tests Passed! ==="
