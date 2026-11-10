"""Owner playtest 11.11 (Shore-02): a bot frozen by MOAC/MOAR disappeared from the server.
Server log: hit with health=0 by the freeze gun -> ~0.11 s later "removed externally" -> immediate re-create refused
("entity name ... is occupied") -> no line for that bot until the match ended (bots #3, #9, #13).
Cause: CGameRules::ShatterEntity schedules removal of a shattered actor when !IsPlayer(); a bot has channel 0.
Checks the real sources: the shatter treats an owned bot as a player (locally, IsPlayer stays stock), and a failed
roster restore is retried (KeepRosterForRetry, BotRemovalPolicyTests) instead of being cleared.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
fails = 0


def check(ok, name):
    global fails
    print(('PASS ' if ok else 'FAIL ') + name)
    fails += 0 if ok else 1


def code(path):
    return re.sub(r'//[^\n]*', '', (ROOT / path).read_text(encoding='utf-8-sig'))


def body(src, head):
    i = src.index(head)
    j = src.index('{', i)
    depth = 0
    for k in range(j, len(src)):
        depth += src[k] == '{'
        depth -= src[k] == '}'
        if depth == 0:
            return src[j:k + 1]
    raise ValueError(head)


def main():
    rules = code('Code/CryGame/GameRules.cpp')
    shatter = body(rules, 'void CGameRules::ShatterEntity(')
    guard = 'if (!isPlayer && pActor && WarsBots::BotMatch::IsBot(*this, entityId))'
    check(guard in shatter, 'ShatterEntity treats an owned bot as a player')
    check(shatter.index('bool isPlayer = (pActor && pActor->IsPlayer());') < shatter.index(guard)
          < shatter.index('ScheduleEntityRemoval(entityId, 0.15f, false);'), 'the bot check comes before the removal')
    pre = shatter[:shatter.index(guard)]
    check(pre.rstrip().endswith('#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)'), 'the bot check is under WARSBOTS_EXPERIMENTAL_SPAWNER')
    check('bool IsPlayer() const\n\t{\n\t\treturn GetChannelId() != 0;' in code('Code/CryCommon/CryAction/IActorSystem.h').replace('\r\n', '\n'),
          'IActor::IsPlayer stays stock (no global change)')

    mgr = code('Code/CryGame/Bots/BotManager.cpp')
    restore = body(mgr, 'void BotManager::RestoreRoster()')
    check('KeepRosterForRetry(entry.firstFail, now, kRestoreRetryWindow)' in restore, 'failed restore is kept for retry')
    check('m_roster.swap(retry);' in restore and 'm_restorePending = !m_roster.empty();' in restore
          and 'm_roster.clear();' not in restore, 'roster keeps only the retries, restore stays pending')
    check('gEnv->pTimer->GetCurrTime() >= m_nextRestoreAt' in mgr and 'm_nextRestoreAt = now + kRestoreRetryStep;' in restore,
          'retries are paced')
    check('restore gave up after' in restore, 'giving up is logged')
    hdr = code('Code/CryGame/Bots/BotManager.h')
    check('struct RosterEntry { int number; BotProfile profile; bool ai; float firstFail = -1.f; };' in hdr, 'roster entry remembers the first failure')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
