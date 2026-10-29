"""DS-122 source-contract controls for the guarded upper-ladder exit proposal.

This is deliberately a *contract* test, not a runtime claim: it reads the real files at a Git revision and checks
the exact properties the DS122 proposal adds, and that no physical shortcut was added.

Usage:
  python ds122_exit_signal_contract_test.py --repo <QA/build/ds076-code> --rev abac72a9... --expect red
  python ds122_exit_signal_contract_test.py --repo <patched tree>          --expect green

Exit 0 only when the selected expectation holds. RED on abac72a9 is expected; GREEN requires the reviewed patch.
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys


def read(repo: Path, rev: str, path: str) -> str:
    if rev == 'WORKTREE':
        return (repo / path).read_text(encoding='utf-8-sig')
    out = subprocess.run(['git', '-C', str(repo), 'show', f'{rev}:{path}'], capture_output=True, check=True)
    return out.stdout.decode('utf-8-sig', errors='replace')


def block(text: str, marker: str) -> str:
    start = text.index(marker)
    left = text.index('{', start)
    depth = 0
    for i in range(left, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise ValueError('unclosed block: ' + marker)


def squash(s: str) -> str:
    return re.sub(r'\s+', ' ', s)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', type=Path, required=True)
    ap.add_argument('--rev', default='WORKTREE')
    ap.add_argument('--expect', choices=['red', 'green'], required=True)
    args = ap.parse_args()

    move = read(args.repo, args.rev, 'Code/CryGame/Actors/Player/PlayerMovement.cpp')
    player = read(args.repo, args.rev, 'Code/CryGame/Actors/Player/Player.cpp')
    loco = read(args.repo, args.rev, 'Code/CryGame/Bots/BotLocomotion.cpp')

    exit_block = block(move, 'if (m_stats.isExitingLadder)')
    execute = block(loco, 'bool BotLocomotion::ExecuteLadder(')
    leave_exit = block(player, 'if (reason == eLAT_ExitTop)')
    call = squash(execute[execute.index('m_movement.Move('):])

    reassert = re.findall(r'SetInput\("Signal",\s*"exit_ladder_top"\)', exit_block)
    pseudo_old = 'nativeAttached, false, -1.f, false, 0.5f, false' in call
    pseudo_new = 'nativeAttached, false, actor.CalculatePseudoSpeed(false), false, 0.5f, false' in call
    stock_guard = 'NativeLadderExitPhysicalReady' in exit_block
    leaves = len(re.findall(r'RequestLeaveLadder\(CPlayer::eLAT_ReachedEnd\)', exit_block))
    forbidden = [t for t in ('SetWorldPos', 'SetWorldTM', 'AddImpulse', 'PushForcedState',
                             'ForceSkeletonUpdate', 'eACF_AlwaysAnimation', 'SetPseudoSpeed',
                             'ClearMoveTarget') if t in exit_block]
    diag = all(t in exit_block for t in ('held=%d', 'sig=%d', 'pseudo=%.2f', 'upd=%d'))
    signal_log = 'LADDER EXIT SIGNAL' in leave_exit
    warsbot_scope = exit_block.index('if (warsBot)') < exit_block.index('SetInput("Signal", "exit_ladder_top")') \
        if reassert else False
    held_guard = 'exitHeld' in exit_block and 'ladderOffTop' in exit_block and '!exitHeld' in exit_block

    fails = []
    if args.expect == 'red':
        if reassert:
            fails.append('RED: abac must not re-assert the exit signal')
        if not pseudo_old:
            fails.append('RED: abac ExecuteLadder must still pass pseudo=-1.f')
        if signal_log or diag:
            fails.append('RED: abac must not contain the DS122 diagnostics')
    else:
        if len(reassert) != 1:
            fails.append('GREEN: exactly one exit-signal re-assert expected, got %d' % len(reassert))
        if not warsbot_scope:
            fails.append('GREEN: re-assert must be inside the warsBot scope')
        if not held_guard:
            fails.append('GREEN: re-assert must stop when the installed off-top state holds')
        if not pseudo_new:
            fails.append('GREEN: ExecuteLadder must pass CalculatePseudoSpeed(false) at the pseudo position')
        if not diag or not signal_log:
            fails.append('GREEN: DS122 diagnostics missing')
        if not stock_guard or leaves != 1:
            fails.append('GREEN: stock physical guard / single ReachedEnd changed')
        if forbidden:
            fails.append('GREEN: forbidden physical shortcut(s): ' + ', '.join(forbidden))

    if fails:
        for f in fails:
            print('FAIL ' + f)
        return 1
    print('DS122 source contract %s PASS' % args.expect.upper())
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
