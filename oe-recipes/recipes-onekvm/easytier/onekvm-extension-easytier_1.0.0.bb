SUMMARY = "OneKVM EasyTier mesh VPN service extension"
HOMEPAGE = "https://github.com/EasyTier/EasyTier"
LICENSE = "GPL-3.0-only & LGPL-3.0-only"
ONEKVM_EXTENSION_SOURCE ?= "${UNPACKDIR}/extension"
ONEKVM_EXTENSION_SOURCE_REV ?= "${ONEKVM_COMPONENT_SRCREV}"
do_compile[vardeps] += "ONEKVM_EXTENSION_SOURCE_REV"
LIC_FILES_CHKSUM = "\
    file://${ONEKVM_EXTENSION_SOURCE}/LICENSE;md5=1ebbd3e34237af26da5dc08a4e440464 \
    file://LICENSE;md5=3000208d539ec061b899bce1d9ce9404 \
"
PR = "r3"

ONEKVM_COMPONENT_GIT_URI ??= "git://github.com/onekvm/onekvm-extension-easytier.git;protocol=https;branch=main"
ONEKVM_COMPONENT_SRCREV ??= ""
SRC_URI = "\
    ${ONEKVM_COMPONENT_GIT_URI};name=extension;destsuffix=extension \
    git://github.com/EasyTier/EasyTier.git;protocol=https;nobranch=1;name=easytier;destsuffix=easytier \
    file://0001-time-0.3.29-rustc-1.80-e0282.patch;apply=no \
    file://fix-machine-id.py \
    file://trim-workspace.py \
"
SRCREV_FORMAT = "extension_easytier"
SRCREV_extension = "${ONEKVM_COMPONENT_SRCREV}"
SRCREV_easytier = "8428a89d2dabc94c97d370ec607c6ca142473626"
S = "${UNPACKDIR}/easytier"
CARGO_SRC_DIR = "easytier"

inherit cargo onekvm-extension

# cargo_common_do_patch_paths would treat destsuffix=easytier as crate
# "easytier" (the workspace root) and strip git sources from Cargo.lock.
python cargo_common_do_patch_paths() {
    return
}

# Client VPN without smoltcp/kcp git crates or the GUI workspace members.
CARGO_DISABLE_BITBAKE_VENDORING = "1"
export RUSTC_BOOTSTRAP = "1"
CARGO_BUILD_FLAGS:remove = "--offline --frozen"
CARGO_BUILD_FLAGS:append = " --ignore-rust-version --bin easytier-core --bin easytier-cli --no-default-features --features wireguard,tun,websocket,quic,zstd,faketcp"
do_compile[network] = "1"
do_configure[network] = "1"
do_compile[dirs] = "${B}"

do_configure:prepend() {
    if [ -f ${S}/Cargo.lock ] && [ ! -f ${S}/easytier/Cargo.lock ]; then
        cp ${S}/Cargo.lock ${S}/easytier/Cargo.lock
    fi
}

DEPENDS += "json-c protobuf-native"
ONEKVM_EXTENSION_ID = "easytier"
ONEKVM_EXTENSION_API_VERSION = "2"
ONEKVM_EXTENSION_MANIFEST_TEMPLATE = "${ONEKVM_EXTENSION_SOURCE}/manifest.json.in"

PACKAGES = "${PN}-dbg easytier ${PN}"
FILES:${PN} = "\
    ${ONEKVM_EXTENSION_ROOT}/bin/onekvm-easytier-controller \
    ${ONEKVM_EXTENSION_ROOT}/manifest.json \
    ${ONEKVM_EXTENSION_ROOT}/web \
    ${ONEKVM_EXTENSION_ROOT}/screen-app \
"
FILES:easytier = "\
    ${bindir}/easytier-core \
    ${bindir}/easytier-cli \
"
RDEPENDS:${PN} += "easytier onekvm-kernel-module-tun"
RREPLACES:easytier += "${PN}-runtime (<= 1.0.0-r0)"
RCONFLICTS:easytier += "${PN}-runtime (<= 1.0.0-r0)"
INSANE_SKIP:${PN} += "already-stripped"
INSANE_SKIP:easytier += "already-stripped"
INSANE_SKIP:${PN}-dbg += "buildpaths"

do_compile:prepend() {
    export RUSTC_BOOTSTRAP=1
    rm -f ${S}/rust-toolchain.toml
    sed -i 's/rust-version = "1.95"/rust-version = "1.94"/' ${S}/Cargo.toml
    guarden_lib="${CARGO_HOME}/registry/src/index.crates.io-1949cf8c6b5b557f/guarden-0.1.2/src/lib.rs"
    if [ -f "$guarden_lib" ] && ! grep -q 'feature(cfg_select)' "$guarden_lib"; then
        sed -i '1i #![feature(cfg_select)]' "$guarden_lib"
    fi
    easytier_lib="${S}/easytier/src/lib.rs"
    if [ -f "$easytier_lib" ] && ! grep -q 'feature(cfg_select)' "$easytier_lib"; then
        sed -i '1i #![feature(cfg_select)]' "$easytier_lib"
    fi
    python3 ${UNPACKDIR}/fix-machine-id.py ${S}/easytier/src/common/machine_id.rs
    python3 ${UNPACKDIR}/trim-workspace.py ${S}/Cargo.toml
    oe_cargo_fix_env
    cargo fetch --target ${RUST_HOST_SYS} --manifest-path=${CARGO_MANIFEST_PATH} || true
    guarden_lib="${CARGO_HOME}/registry/src/index.crates.io-1949cf8c6b5b557f/guarden-0.1.2/src/lib.rs"
    if [ -f "$guarden_lib" ] && ! grep -q 'feature(cfg_select)' "$guarden_lib"; then
        sed -i '1i #![feature(cfg_select)]' "$guarden_lib"
    fi

    time_parse="${CARGO_HOME}/bitbake/time-0.3.29/src/format_description/parse/mod.rs"
    if grep -q 'collect::<Result<Box<_>, _>>' "$time_parse" 2>/dev/null; then
        patch -p1 -d "${CARGO_HOME}/bitbake/time-0.3.29" \
            < "${UNPACKDIR}/0001-time-0.3.29-rustc-1.80-e0282.patch"
    fi

    install -d ${B}/bin
    ${CXX} ${CXXFLAGS} -I${STAGING_INCDIR} \
        ${ONEKVM_EXTENSION_SOURCE}/src/main.cpp ${STAGING_LIBDIR}/libjson-c.a -lm ${LDFLAGS} \
        -o ${B}/bin/onekvm-easytier-controller
}

do_install() {
    oe_runmake -C ${ONEKVM_EXTENSION_SOURCE} install \
        CONTROLLER_BINARY=${B}/bin/onekvm-easytier-controller \
        RUNTIME_BINARY=${B}/target/${RUST_HOST_SYS}/${BUILD_DIR}/easytier-core \
        CLI_BINARY=${B}/target/${RUST_HOST_SYS}/${BUILD_DIR}/easytier-cli \
        DESTDIR=${D} PREFIX=${ONEKVM_EXTENSION_ROOT} BINDIR=${bindir} \
        VERSION=${PV} ARCH=${TARGET_ARCH}
}
