# Bundle: codesign

After bundle assembly, optionally run `codesign --deep` on the .app bundle.

- Check for signing identity in config (`darwin.codesign_identity`) or use ad-hoc signing (`-`)
- Run `codesign --deep --force --sign <identity> <app>`
- Non-critical — bundle works without signing, but required for distribution

## Status: pending

Created: 2026-06-13
