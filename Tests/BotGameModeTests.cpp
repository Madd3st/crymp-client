// DS-050 #2: game-mode policy tests (engine-free).
#include "CryGame/Bots/BotGameMode.h"
#include <cstdio>

using namespace WarsBots;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main()
{
    const BotGameModePolicy ia = PolicyForGameRulesClass("InstantAction");
    CHECK(ia.kind == BotGameModeKind::InstantAction && ia.botRules);
    CHECK(!ia.teamBeforeSpawn && !ia.teamSpawn && !ia.waveRevive && !ia.captureGoals && !ia.reviveBuy && !ia.spawnGroups);

    const BotGameModePolicy ps = PolicyForGameRulesClass("PowerStruggle");
    CHECK(ps.kind == BotGameModeKind::PowerStruggle && ps.botRules);
    CHECK(ps.teamBeforeSpawn && ps.teamSpawn && ps.waveRevive && ps.captureGoals && ps.reviveBuy && ps.attachParticipant && ps.spawnGroups);

    const BotGameModePolicy none = PolicyForGameRulesClass(nullptr);
    CHECK(none.kind == BotGameModeKind::Unknown && !none.botRules);

    CHECK(BotGameModeFor("InstantAction").Kind() == BotGameModeKind::InstantAction);
    CHECK(BotGameModeFor("PowerStruggle").Kind() == BotGameModeKind::PowerStruggle);
    CHECK(BotGameModeFor("SomethingElse").Kind() == BotGameModeKind::Unknown);
    CHECK(BotGameModeFor(nullptr).Kind() == BotGameModeKind::Unknown);
    CHECK(BotGameModeFor("PowerStruggle").Policy().reviveBuy);

    if (failures) { std::printf("game_mode: %d FAILURES\n", failures); return 1; }
    std::printf("game_mode: all tests passed\n");
    return 0;
}
