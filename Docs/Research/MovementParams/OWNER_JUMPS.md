# Owner jumps -> Detour off-mesh links

Parsed owner MOVE TRACE (`kind=human side=client`). Filtered: ballistic reach (vh<=6.75 m/s, v0 normal 4.43/Strength 9.6, x1.2 margin) + teleport/revive within +/-1.0s.

## Counts

| type | kept | rejected |
| --- | --- | --- |
| strength | 30 | 7 |
| normal | 52 | 31 |
| drop | 7 | 6 |
| **total** | **89** | **44** |

## Candidate links (>= 2 independent take-offs within 2 m)

| size | centroid (x,y) |
| --- | --- |
| 10 | (1247.0,1319.7) |
| 2 | (1333.1,1336.1) |
| 2 | (1246.6,1324.8) |
| 2 | (1352.9,1301.4) |
| 2 | (1313.5,1293.6) |

## Biggest climbs (kept)

| peak m | dz m | dist m | type | takeoff | landing |
| --- | --- | --- | --- | --- | --- |
| 5.88 | 4.17 | 6.54 | strength | (1356.3,1337.0,147.1) | (1350.2,1339.2,151.3) |
| 3.81 | 2.67 | 9.03 | strength | (1288.5,1356.4,136.0) | (1295.1,1362.5,138.7) |
| 3.77 | 0.00 | 5.86 | strength | (1359.4,1335.9,147.1) | (1353.7,1336.9,147.1) |
| 3.59 | 1.96 | 8.73 | strength | (1347.1,1298.4,141.2) | (1340.3,1292.9,143.2) |
| 3.58 | 2.70 | 5.37 | strength | (1353.1,1300.6,138.5) | (1348.2,1298.4,141.2) |
| 3.58 | 0.65 | 12.93 | strength | (1358.9,1338.5,150.7) | (1346.2,1336.5,151.3) |
| 3.58 | 0.00 | 12.51 | strength | (1352.7,1302.1,138.5) | (1340.1,1302.3,138.5) |
| 3.58 | -0.11 | 12.80 | strength | (1340.1,1302.3,138.5) | (1327.9,1298.8,138.4) |
| 3.58 | 0.00 | 1.22 | strength | (1313.8,1293.5,143.2) | (1312.6,1293.6,143.2) |
| 3.58 | 2.55 | 5.74 | strength | (1220.1,1308.6,136.0) | (1225.7,1307.3,138.5) |
| 3.58 | 2.95 | 7.57 | strength | (1225.7,1307.3,138.5) | (1228.6,1300.3,141.5) |
| 3.58 | 2.14 | 10.29 | strength | (1301.2,1360.2,146.1) | (1298.4,1350.3,148.2) |
| 3.58 | 1.28 | 13.53 | strength | (1313.2,1293.6,143.2) | (1302.7,1302.2,144.5) |
| 3.58 | 3.08 | 9.60 | strength | (1209.7,1334.4,136.0) | (1200.2,1333.2,139.1) |
| 3.58 | 1.96 | 7.98 | strength | (1332.5,1336.7,150.0) | (1338.2,1331.0,151.9) |

## Rejected (first 40)

| reason | type | dist m | dz m | takeoff | landing |
| --- | --- | --- | --- | --- | --- |
| dist 18.9m > reach 14.0m (merged/sample gap) | strength | 18.9 | -1.3 | (1294.5,1294.8,143.2) | (1277.3,1302.6,142.0) |
| dist 10.5m > reach 6.1m (merged/sample gap) | normal | 10.5 | 0.0 | (1360.5,1304.1,138.5) | (1360.3,1314.7,138.5) |
| dz exceeds jump apex | strength | 4.4 | 8.6 | (1367.4,1332.5,138.6) | (1364.3,1335.6,147.1) |
| dist 9.5m > reach 6.1m (merged/sample gap) | normal | 9.5 | 0.0 | (1347.7,1336.1,151.4) | (1338.4,1338.3,151.4) |
| dist 8.2m > reach 6.1m (merged/sample gap) | normal | 8.2 | 0.0 | (1327.0,1343.2,150.0) | (1319.7,1346.9,150.0) |
| dist 14.5m > reach 10.0m (merged/sample gap) | normal | 14.5 | -4.3 | (1316.4,1351.5,150.4) | (1311.2,1365.0,146.1) |
| dist 12.1m > reach 9.8m (merged/sample gap) | normal | 12.1 | -3.9 | (1316.7,1381.8,145.5) | (1316.0,1393.9,141.6) |
| dist 6.2m > reach 5.0m (merged/sample gap) | drop | 6.2 | -2.7 | (1313.6,1382.5,141.4) | (1309.2,1378.2,138.7) |
| dz exceeds jump apex | strength | 59.2 | 8.6 | (1312.3,1355.2,138.5) | (1368.0,1335.2,147.1) |
| dist 9.5m > reach 7.8m (merged/sample gap) | normal | 9.5 | -1.4 | (1338.8,1338.5,151.4) | (1330.5,1343.0,150.0) |
| dist 12.3m > reach 10.0m (merged/sample gap) | normal | 12.3 | -4.3 | (1316.3,1351.9,150.4) | (1313.9,1364.0,146.1) |
| dist 13.0m > reach 8.3m (merged/sample gap) | normal | 13.0 | -1.9 | (1316.4,1329.4,150.4) | (1316.5,1316.3,148.4) |
| dz exceeds jump apex | normal | 1.5 | 1.5 | (1317.5,1317.0,148.2) | (1317.3,1315.5,149.8) |
| dist 12.2m > reach 7.9m (merged/sample gap) | normal | 12.2 | -1.5 | (1317.0,1315.3,149.8) | (1314.2,1303.5,148.2) |
| dist 10.4m > reach 6.1m (merged/sample gap) | normal | 10.4 | -0.0 | (1243.9,1294.8,142.0) | (1233.8,1297.3,141.9) |
| dist 14.4m > reach 10.6m (merged/sample gap) | normal | 14.4 | -5.1 | (1232.5,1349.8,141.1) | (1233.5,1364.2,136.0) |
| dist 8.2m > reach 6.1m (merged/sample gap) | normal | 8.2 | -0.0 | (1205.7,1364.8,139.0) | (1202.6,1357.2,139.0) |
| dist 11.3m > reach 9.3m (merged/sample gap) | normal | 11.3 | -3.2 | (1202.6,1357.2,139.0) | (1191.5,1359.2,135.8) |
| dz exceeds jump apex | normal | 5.5 | 1.0 | (1203.2,1367.8,136.1) | (1208.6,1366.5,137.1) |
| dist 11.1m > reach 6.1m (merged/sample gap) | normal | 11.1 | -0.0 | (1216.8,1370.7,136.0) | (1227.0,1366.6,136.0) |
| dist 7.4m > reach 6.0m (merged/sample gap) | normal | 7.4 | 0.1 | (1227.0,1366.6,136.0) | (1234.2,1364.5,136.1) |
| dist 5.5m > reach 4.5m (merged/sample gap) | normal | 5.5 | 0.8 | (1234.2,1364.5,136.1) | (1235.9,1359.2,136.8) |
| dist 12.4m > reach 6.0m (merged/sample gap) | normal | 12.4 | 0.1 | (1235.7,1324.2,136.0) | (1231.3,1312.6,136.1) |
| dist 7.5m > reach 6.2m (merged/sample gap) | normal | 7.5 | -0.1 | (1231.3,1312.6,136.1) | (1223.8,1311.7,136.0) |
| dist 9.3m > reach 6.0m (merged/sample gap) | normal | 9.3 | 0.0 | (1242.1,1328.7,136.0) | (1246.2,1320.2,136.1) |
| dist 13.5m > reach 6.4m (merged/sample gap) | normal | 13.5 | -0.2 | (1208.3,1309.9,136.2) | (1221.6,1308.1,136.0) |
| dz exceeds jump apex | strength | 75.7 | 6.0 | (1246.4,1336.7,136.0) | (1183.0,1295.2,142.0) |
| dist 16.9m > reach 10.4m (merged/sample gap) | normal | 16.9 | -4.8 | (1194.1,1297.7,140.8) | (1209.3,1305.1,136.0) |
| dz exceeds jump apex | normal | 3.5 | 1.1 | (1289.6,1353.2,136.0) | (1292.9,1351.9,137.1) |
| dist 9.5m > reach 6.1m (merged/sample gap) | normal | 9.5 | -0.0 | (1216.2,1367.9,136.0) | (1223.0,1361.3,136.0) |
| dist 100.4m > reach 11.1m (merged/sample gap) | strength | 100.4 | 2.5 | (1245.3,1324.1,136.1) | (1318.0,1393.3,138.6) |
| dist 102.1m > reach 4.9m (merged/sample gap) | drop | 102.1 | -2.6 | (1319.9,1391.6,138.6) | (1247.5,1319.8,136.0) |
| dist 11.0m > reach 5.9m (merged/sample gap) | normal | 11.0 | 0.1 | (1320.1,1389.2,138.6) | (1314.4,1379.8,138.7) |
| dist 9.0m > reach 6.1m (merged/sample gap) | normal | 9.0 | 0.0 | (1306.6,1378.7,138.7) | (1298.2,1375.2,138.7) |
| dist 75.2m > reach 5.0m (merged/sample gap) | drop | 75.2 | -2.7 | (1298.2,1375.2,138.7) | (1247.5,1319.8,136.0) |
| dist 136.1m > reach 11.1m (merged/sample gap) | strength | 136.1 | 2.6 | (1233.4,1330.2,136.0) | (1368.4,1312.9,138.6) |
| dist 4.8m > reach 3.3m (merged/sample gap) | drop | 4.8 | -1.2 | (1241.8,1307.3,143.1) | (1239.1,1303.4,141.9) |
| dist 9.4m > reach 7.6m (merged/sample gap) | normal | 9.4 | -1.2 | (1217.6,1303.8,138.2) | (1211.6,1311.1,137.0) |
| dist 15.7m > reach 7.3m (merged/sample gap) | normal | 15.7 | -0.9 | (1203.5,1312.2,137.0) | (1188.4,1316.7,136.1) |
| dz exceeds jump apex | strength | 5.4 | 7.2 | (1299.0,1373.2,138.7) | (1301.7,1368.5,145.9) |

