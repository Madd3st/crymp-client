"""SPEC-009 (owner 10.11/11.11), wb_bot_ladder_ban: a ladder failure bans the link for that bot only.
Checks the real sources: with the flag OFF CancelLadder bans on the first failure exactly as before; with 1 the shared
LadderBanBook (BotLadderBanTests) decides bot/level scope, the bot's path query raises the cost of its avoided link,
level bans are lifted (NavMeshCore::UnbanLink) and the book is cleared at level end.
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
    return re.sub(r'//[^\n]*', '', (ROOT / path).read_text(encoding='utf-8-sig')).replace('\r\n', '\n')


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
    brain = code('Code/CryGame/Bots/BotBrain.cpp')
    cancel = body(brain, 'void BotBrain::CancelLadder(')
    check('if (BotConfig::Get().ladderBan!=1) mesh->BanLink(m_ladderNow.ref);' in cancel, 'flag OFF: ban on the first failure as before')
    check('LadderBans().Fail(m_ladderNow.ref,m_number,gEnv->pTimer->GetCurrTime())' in cancel, 'flag ON: the shared book decides')
    check('if (d.scope==LadderBanScope::Level) mesh->BanLink(m_ladderNow.ref);' in cancel
          and 'm_ladderAvoidRef=m_ladderNow.ref; m_ladderAvoidUntil=d.until;' in cancel, 'level -> mesh ban, bot -> personal avoidance')
    check('LADDER ban link=%llu scope=%s' in cancel, 'the ban is logged')
    check(cancel.index('LadderBans().Fail(') < cancel.index('m_ladderNow=NavPathLink();'), 'the link is read before the ladder state resets')
    fail_site = brain[brain.index('if (o.phase==LadderPhase::Failed)'):]
    check('CancelLadder(actor,why[int(o.fail)],true);' in fail_site[:900], 'a ladder failure still cancels with ban=true')

    check('const bool avoid = LadderAvoidActive();' in brain
          and 'if (avoid) { cost.avoidLink = m_ladderAvoidRef; cost.avoidMult = kLadderAvoidCost; }' in brain
          and 'if (layered || avoid) mesh->SetCostContext(&cost);' in brain
          and 'if (layered || avoid) mesh->SetCostContext(nullptr);' in brain, 'the path query carries the avoided link')
    check('else if (avoid) { cost.climb = false; cost.noise = 0.f; }' in brain, 'without wb_bot_unstick only the avoided link costs more')
    active = body(brain, 'bool BotBrain::LadderAvoidActive() const')
    check('BotConfig::Get().ladderBan==1' in active and 'gEnv->pTimer->GetCurrTime()<m_ladderAvoidUntil' in active, 'avoidance is flag-gated and times out')
    release = body(brain, 'void BotBrain::ReleaseLadderBans()')
    check('BotConfig::Get().ladderBan!=1' in release and 'mesh->UnbanLink(ref);' in release and 'TakeExpired(' in release, 'expired level bans are lifted')
    upd = brain[brain.index('if (m_ladderCancelPending) CancelLadder(actor,"reset",false);'):]
    check('ReleaseLadderBans();' in upd[:200], 'lifting runs every update')

    mgr = code('Code/CryGame/Bots/BotManager.cpp')
    check('BotBrain::ClearLadderBans();' in body(mgr, 'void BotManager::EndLevel()'), 'the book is cleared at level end')

    core = code('Code/CryGame/Bots/BotNavMeshCore.cpp')
    check('m_bannedFlags.emplace(ref, old);' in body(core, 'void NavMeshCore::BanLink(') and
          'm_mesh->setPolyFlags(dtPolyRef(ref), it->second);' in body(core, 'void NavMeshCore::UnbanLink('), 'unban restores the original flags')
    check('m_bannedFlags.clear();' in body(core, 'void NavMeshCore::Reset()'), 'mesh reset forgets bans')
    seg = body(core, 'float NavMeshCore::SegmentCostMultiplier(')
    check('if (c->avoidLink && cur == c->avoidLink) m *= c->avoidMult;' in seg
          and seg.index('if (c->avoidLink') > seg.index('if (!offMesh)'), 'the avoided link costs more also off-mesh')

    hdr = code('Code/CryGame/Bots/BotConfig.h')
    cfg = code('Code/CryGame/Bots/BotConfig.cpp')
    check('int ladderBan=0;' in hdr and '"wb_bot_ladder_ban",&BotConfig::ladderBan' in cfg, 'cvar registered, default OFF')
    check('wb_bot_ladder_ban' in (ROOT / 'Docs/FLAGS.md').read_text(encoding='utf-8-sig'), 'FLAGS.md lists the flag')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
