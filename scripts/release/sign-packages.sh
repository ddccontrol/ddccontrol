#!/usr/bin/env bash
# Source this file only in the signing job; build jobs never receive the key.
set -euo pipefail

: "${PACKAGE_SIGNING_KEY:?Set the PACKAGE_SIGNING_KEY Actions secret}"
: "${PACKAGE_SIGNING_FINGERPRINT:?Set the PACKAGE_SIGNING_FINGERPRINT Actions variable}"
[[ $PACKAGE_SIGNING_FINGERPRINT =~ ^[A-Fa-f0-9]{40}$ ]]
export GNUPGHOME
GNUPGHOME=$(mktemp -d)
chmod 700 "$GNUPGHOME"
trap 'gpgconf --kill gpg-agent || true; rm -rf "$GNUPGHOME"' EXIT
printf '%s' "$PACKAGE_SIGNING_KEY" | gpg --batch --import
printf '%s' "${PACKAGE_SIGNING_PASSPHRASE:-}" > "$GNUPGHOME/passphrase"
chmod 600 "$GNUPGHOME/passphrase"
unset PACKAGE_SIGNING_KEY PACKAGE_SIGNING_PASSPHRASE

sign_file() {
    gpg --batch --yes --pinentry-mode loopback \
        --passphrase-file "$GNUPGHOME/passphrase" \
        --local-user "$PACKAGE_SIGNING_FINGERPRINT" --digest-algo SHA256 "$@"
}

sign_rpm() {
    rpmsign --define "_gpg_name $PACKAGE_SIGNING_FINGERPRINT" \
        --define '__gpg /usr/bin/gpg' \
        --define "_gpg_path $GNUPGHOME" \
        --define "_gpg_sign_cmd_extra_args --batch --pinentry-mode loopback --passphrase-file $GNUPGHOME/passphrase" \
        --addsign "$1"
}

export_public_key() {
    gpg --batch --armor --export "$PACKAGE_SIGNING_FINGERPRINT" > "$1"
    test -s "$1"
}
