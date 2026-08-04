# Security policy

## Supported version

Only the latest published release is supported. This project is experimental alpha software.

## Reporting a vulnerability

Please use GitHub's private vulnerability reporting feature instead of opening a public issue with exploit details. Include the affected version, reproduction steps, and the security impact. Do not include personal X4 saves, credentials, or unrelated process dumps.

## Release safety

Official binaries are published only in this repository's GitHub Releases and include SHA-256 checksums. They are currently unsigned. The launcher deliberately injects its bundled hook DLL into `X4.exe`, so heuristic security warnings are possible. Never disable security software to run the tool and do not use binaries from mirrors.
