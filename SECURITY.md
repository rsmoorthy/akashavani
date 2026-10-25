# Security policy

This repository is currently a development scaffold. Do not expose it to the
public internet with the default tokens, passwords or HTTP endpoints.

## Reporting

Report vulnerabilities privately through the repository host's security advisory
feature. Do not open a public issue with exploit details, credentials or site
information.

## Known scaffold weaknesses

These are documented, not accepted for production:

- default AP password (`akashvani-setup`) and admin password
  (`akashvani-admin`);
- development admin token `dev-admin-token` and seed device token;
- plaintext NVS for Wi-Fi and cloud credentials;
- TLS certificate verification disabled on the device
  (`WiFiClientSecure::setInsecure()`);
- OTA images are not signature-verified and the supplied SHA-256 is not checked
  while streaming;
- no local-admin CSRF/rate-limit hardening;
- RF frames are unauthenticated; only framing/plausibility can protect the
  measurement path.

## Before deployment

- use unique, strong device and admin credentials;
- pin a CA and remove `setInsecure()`;
- implement and test OTA checksum/signature verification and rollback;
- restrict the admin API by network policy;
- use HTTPS at a reverse proxy;
- back up the database and firmware artifacts;
- review the hardware power/antenna installation for the site.

## Secrets in the repository

If a secret is committed, rotate it immediately. Removing the commit is not
sufficient. Never commit real Wi-Fi credentials, device tokens, site names or
captured private data.
