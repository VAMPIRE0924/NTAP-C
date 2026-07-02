# src/c

NTAP-C TAP terminal client.

Current implemented scope:

```text
AUTH_TAP
CONFIG_PUSH consumption before TAP setup
Linux TAP open/read/write relay path
real netns ping smoke against NTAP-A + NTAP-B
direct-only token mode against NTAP-B direct listener
direct-first fallback to AUTH_TAP relay when direct setup fails
automatic direct-first strategy consumption from CONFIG_PUSH v2
`ntap-c check-env` Linux TAP privilege preflight and Windows TAP/Wintun adapter discovery
```

Windows TAP data-plane support remains post-MVP.
