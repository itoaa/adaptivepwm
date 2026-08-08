# Signing keys

**Policy:** Private keys must **never** be committed to this repository.

## Layout

| File | In git? | Purpose |
|------|---------|---------|
| `bootloader_public.pem` | Optional | Public key for verification / docs |
| `bootloader_public_key.c` | Yes (public material only) | Embedded public key for bootloader |
| `bootloader_private.pem` | **No** | Signing key — local or CI secret only |
| `*.pem` private material | **No** | gitignored |

## Generate a new Ed25519 key pair (example)

```bash
# Requires openssl with Ed25519 support
openssl genpkey -algorithm Ed25519 -out bootloader_private.pem
openssl pkey -in bootloader_private.pem -pubout -out bootloader_public.pem
```

Then regenerate the embedded public key C file with the project signing tool:

```bash
python tools/sign_firmware.py --export-public-c keys/bootloader_public_key.c
```

(Adjust flags to match `tools/sign_firmware.py` help.)

## If a private key was ever committed

1. **Rotate** — generate a new key pair.
2. Rebuild bootloader with the new public key.
3. Treat the old private key as compromised.
4. Purge from git history if the repo is or will be public (`git filter-repo` / BFG).

Secure boot remains **optional** (`FEATURE_SECURE_BOOT` / separate bootloader build) until recovery + key ops are mature — see `MATURITY.md`.
