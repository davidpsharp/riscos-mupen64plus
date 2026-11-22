# Requests to RoMac (rol-run)

This project never edits `~/code/rol-run`. Each request goes here first, then to the RoMac/rol-run
agent by SendMessage. Write it in ARM, Tarmac and Linux terms, with measurements and a program or
test ROM that shows the need. The RoMac agent (and David) decide.

| # | Request | Why | Status |
|---|---|---|---|
| 1 | OpenGL ES 3.0 contexts in `romac-3d` | GLideN64's GLES 3 build (phase 6) | not sent: needed from phase 6 |
| 2 | (if measured) Tarmac behaviour under new_dynarec: cache thrash or `cacheflush` storms | dynarec speed on RoMac (phase 5) | not sent: only once measured |
