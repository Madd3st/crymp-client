# CryMPResearch tools

Offline, read-only, Python 3 standard library only. They never start the game, never touch build
folders and never copy game content: they read installed/extracted files in place and print facts.

| Tool | Purpose |
|---|---|
| `lua_channel_audit.py` | Resolves the effective Lua set exactly as CryMP loads it (`Pak/CryMP/Scripts` > `Pak/Scripts` > given PAK roots), parses functions, builds a call graph from Power Struggle entry points and lists every channel-dependent sink (client RMI, channel tables/APIs, `IsPlayer()` gates, player enumeration). Code text of PAK lines is redacted unless `--show-pak-code` (local use only). |
| `level_inventory.py` | Opens a level `.pak`, reports CryAI navigation export sizes and counts/positions of objective entities (SpawnGroup, Factory, HQ, AlienEnergyPoint, turrets, buy zones). |

Game PAKs must be extracted by the user locally (e.g. with any zip tool) for the Lua audit;
`level_inventory.py` reads `level.pak` directly. Do not commit extracted files.
