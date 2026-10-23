# WarsBots roadmap: CryMP is primary

1. Phase 0: preserve latest Wars snapshot, fork/upstream/main, clean MSVC build,
   isolated default-OFF port with provenance and diagnostic commands.
2. Phase 1: resolve startup, then prove one server-owned Player's create/remove/recreate,
   client visibility, native movement/pickup/fire/damage/death/respawn and late join.
3. Phase 2: independent move/look intent and replicated strafe; combat movement;
   native Speed chase with measured energy/velocity; reproducible Strength jumps.
   Do not mark implemented mechanics as gameplay-proven without repeated trials.
4. Phase 3: tune reaction, aim, distances, reload, retreats and tactical suit selection.
5. Phase 4: test CryAI navigation data/provider; otherwise evaluate a small route graph.
6. Phase 5: two independent bot instances, then profiling and IA participant/statistics;
   Power Struggle and broader scaling follow separately.

Crysis Wars is preserved as source/history, not the primary implementation target.
