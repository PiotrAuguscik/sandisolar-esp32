# Security policy

Please report vulnerabilities using GitHub's private vulnerability reporting
for `PiotrAuguscik/sandisolar-esp32` once published. Do not place security-sensitive details in
a public issue.

Never attach APK signing material, cloud credentials, inverter Wi-Fi
credentials, customer/site data, or unredacted captures to a public report.

The project currently exposes read-only protocol operations. A finding that
allows unintended writes, bypasses target validation, or leaks device/customer
data should be treated as security-sensitive.

## Credentials and protocol constants

The public library does not need Wi-Fi passwords, GitHub tokens, cloud account
credentials or private signing keys. Keep application-specific credentials
outside this repository. Ignore rules and package exclusions cover common local
secret filenames, but do not remove secrets already committed to Git history
or detect credentials stored under other names. Review the working tree, staged
content, history and actual package before publishing; never print matched
secret values in CI logs.

`kHaiPowerAesKey` and `kHaiPowerAesIv` are fixed compatibility constants from the
analyzed local BLE protocol, not per-user account credentials. They intentionally
remain in the codec. Shared fixed AES material and CRC checks do not establish
exclusive access or authenticated ownership of an inverter. The library being
read-only does not imply that the device's wider protocol cannot change settings.
