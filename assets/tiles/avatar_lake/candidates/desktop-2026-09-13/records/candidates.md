# E0/E1/E2 candidates - crop coordinates and stats

Source: assets/tiles/avatar_lake/source/rock_raw_01.png (1254x1254), palette: same 14-color
k-means over the whole image for all three candidates (matches E's color treatment).

## E0_current
- crop: (0,0) size 1254x1254 -> reduced to 80x80
- self-tile edge diff: h=6.75417 v=7.11667
- largest connected same-color fraction: 0.00953125
- big blob count (>5% area): 0
- luminance mean=123.666 sd=27.2379

## E1_medium
- crop: (125,0) size 941x941 -> reduced to 80x80
- self-tile edge diff: h=21.0458 v=22.225
- largest connected same-color fraction: 0.0165625
- big blob count (>5% area): 0
- luminance mean=122.817 sd=27.0063

## E2_loose
- crop: (627,627) size 627x627 -> reduced to 80x80
- self-tile edge diff: h=30.9375 v=21.3042
- largest connected same-color fraction: 0.0392187
- big blob count (>5% area): 0
- luminance mean=122.315 sd=27.725

