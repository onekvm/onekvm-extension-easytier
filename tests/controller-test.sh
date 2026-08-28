#!/bin/sh
set -eu

binary=$1
test_root=${TMPDIR:-/tmp}/onekvm-easytier-controller-$$
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
mkdir -p "$test_root/config" "$test_root/data" "$test_root/sockets"
printf '%s\n' '{"service_enabled":true,"network_name":"","network_secret":"","peers":"tcp://public.easytier.cn:11010","dhcp":true,"ipv4":"","hostname":"onekvm","listen_port":11010}' >"$test_root/config/config.json"

run_controller() {
    ONEKVM_EXTENSION_CONFIG="$test_root/config" \
    ONEKVM_EXTENSION_DATA="$test_root/data" \
    ONEKVM_EXTENSION_SOCKET="$test_root/sockets" \
        "$binary" --controller
}

enable_output=$(printf '%s' '{"event":"enable","extension_id":"easytier","enabled":true}' | run_controller)
printf '%s' "$enable_output" | grep -q '"type":"remove_all"'

printf '%s\n' '{"service_enabled":true,"network_name":"onekvm-lab","network_secret":"secret","peers":"tcp://public.easytier.cn:11010","dhcp":true,"ipv4":"","hostname":"onekvm","listen_port":11010}' >"$test_root/config/config.json"
enable_output=$(printf '%s' '{"event":"enable","extension_id":"easytier","enabled":true}' | run_controller)
printf '%s' "$enable_output" | grep -q '"type":"ensure"'
printf '%s' "$enable_output" | grep -q '"name":"easytier-core"'

change_output=$(printf '%s' '{"event":"config_changed","extension_id":"easytier","enabled":true}' | run_controller)
printf '%s' "$change_output" | grep -q '"type":"remove_all"'
printf '%s' "$change_output" | grep -q '"type":"ensure"'

disable_output=$(printf '%s' '{"event":"disable","extension_id":"easytier","enabled":false}' | run_controller)
printf '%s' "$disable_output" | grep -q '"type":"remove_all"'

status_output=$(printf '%s' '{"event":"invoke","extension_id":"easytier","enabled":true,"method":"status","request":null}' | run_controller)
printf '%s' "$status_output" | grep -q '"ready":true'
printf '%s' "$status_output" | grep -q '"networkName":"onekvm-lab"'

ONEKVM_EXTENSION_CONFIG="$test_root/config" "$binary" --health

mkdir -p "$test_root/bin"
cp "$binary" "$test_root/bin/onekvm-easytier-controller"
cat >"$test_root/bin/easytier-core" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" >"$ONEKVM_EXTENSION_DATA/easytier-arguments"
EOF
chmod 0755 "$test_root/bin/easytier-core"
ONEKVM_EXTENSION_CONFIG="$test_root/config" \
ONEKVM_EXTENSION_DATA="$test_root/data" \
ONEKVM_EXTENSION_SOCKET="$test_root/sockets" \
    "$test_root/bin/onekvm-easytier-controller" --run
grep -Fxq -- "-c" "$test_root/data/easytier-arguments"
grep -q 'network_name = "onekvm-lab"' "$test_root/data/easytier.toml"
grep -q 'network_secret = "secret"' "$test_root/data/easytier.toml"
grep -q 'uri = "tcp://public.easytier.cn:11010"' "$test_root/data/easytier.toml"
[ "$(stat -c '%a' "$test_root/data/easytier.toml")" = 600 ]
