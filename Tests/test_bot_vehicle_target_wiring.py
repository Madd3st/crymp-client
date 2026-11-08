"""Guard the callers of BotVehicleTarget.h (owner playtest 07.11: no shots at the player in a vehicle).
Checks the real sources: Classify, ClearRay and AimPoint use the engine-free rules behind wb_bot_vehicle_targets, and
with the flag OFF the old Classify rule is what VehicleOutOfScope returns (BotVehicleTargetTests).
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


def main():
    tgt = code('Code/CryGame/Bots/BotTargeting.cpp')
    cmb = code('Code/CryGame/Bots/BotCombat.cpp')
    cfg = code('Code/CryGame/Bots/BotConfig.cpp')
    hdr = code('Code/CryGame/Bots/BotConfig.h')
    check('VehicleOutOfScope(other->GetEntity()->IsHidden(), other->GetLinkedVehicle() != nullptr, !ally,' in tgt
          and 'BotConfig::Get().vehicleTargets == 1' in tgt, 'Classify scope through VehicleOutOfScope and the flag')
    check('IsHidden() || other->GetLinkedVehicle()' not in tgt, 'the old unconditional passenger filter is gone')
    check(tgt.index('ally = mine != 0') < tgt.index('VehicleOutOfScope('), 'the team is known before the scope rule')
    check('VehicleRayReaches(entity->GetId(),' in cmb, 'sight/fire ray accepts the target vehicle hull')
    check('VehicleAimAtHull(true, target.GetEntity()->IsHidden(), BotConfig::Get().vehicleTargets == 1)' in cmb,
          'aim at the hull centre only for a hidden passenger')
    check('int vehicleTargets=0;' in hdr and '"wb_bot_vehicle_targets",&BotConfig::vehicleTargets' in cfg, 'cvar registered, default OFF')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
