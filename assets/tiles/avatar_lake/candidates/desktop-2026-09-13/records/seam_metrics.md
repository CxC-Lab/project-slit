# Seam metrics (avg abs channel delta 0-255, self-tile)

`crop edges` = the raw crop's own left/right + top/bottom (why an offset is needed at all).
`offset edges, pre-repair` = same pixels after the toroidal shift moved them away from the
crop's original border (these were contiguous interior pixels in the source, so they should
already be reasonably continuous on their own). `post-repair` = after the center-cross touch-up -
expected to be near-identical to pre-repair, since the repair band never reaches the outer edge;
it's reported to confirm the repair didn't accidentally disturb it.

- 01_80: crop edges h=6.75417 v=7.11667  |  offset edges pre-repair h=21.8667 v=10.3292  |  post-repair h=16.675 v=9.4625
- 02_80: crop edges h=10.5333 v=10.1  |  offset edges pre-repair h=8.63333 v=9.40833  |  post-repair h=7.7625 v=9.04167
- 01_120: crop edges h=7.35278 v=7.53889  |  offset edges pre-repair h=21.4111 v=2.91111  |  post-repair h=16.8194 v=3.01389
- 02_120: crop edges h=10.3556 v=9.39444  |  offset edges pre-repair h=7.34722 v=7.13611  |  post-repair h=6.47778 v=6.62222
