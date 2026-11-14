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
    check('RunRestorePass(m_roster, m_restoreSchedule, now,' in restore and 'AddWith(nullptr, entry.profile, entry.number, entry.ai)' in restore,
          'RestoreRoster runs the tested pass (BotRemovalPolicyTests FakeManager) with the real AddWith')
    check('m_restorePending = !m_roster.empty();' in restore and 'm_roster.clear();' not in restore,
          'roster keeps only the retries, restore stays pending')
    post = body(mgr, 'void BotManager::OnPostUpdate(')
    check('RestoreDue(m_restoreSchedule, gEnv->pTimer->GetCurrTime())' in post and 'm_nextRestoreAt' not in mgr,
          'OnPostUpdate gate is the tested RestoreDue (rebases on clock rollback)')
    check('restore gave up after' in restore, 'giving up is logged')
    for head in ('void BotManager::OnLevelNotFound(', 'void BotManager::OnLoadingStart('):
        check('ResetRestoreSchedule(m_restoreSchedule);' in body(mgr, head), head + ' resets the schedule')
    act = body(mgr, 'void BotManager::OnActionEvent(')
    check(act.count('ResetRestoreSchedule(m_restoreSchedule);') == 2, 'resetBegin and disconnect reset the schedule')
    reset = act[act.index('eAE_resetBegin'):act.index('eAE_disconnected')]
    check('m_roster.clear()' not in reset, 'resetBegin keeps queued bots')
    hdr = code('Code/CryGame/Bots/BotManager.h')
    check('struct RosterEntry { int number; BotProfile profile; bool ai; float firstFail = -1.f; };' in hdr, 'roster entry remembers the first failure')
    check('RestoreSchedule m_restoreSchedule;' in hdr, 'manager owns a RestoreSchedule')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
