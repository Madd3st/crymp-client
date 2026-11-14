// SPEC-006a p. 4 (D-058): vertical swim control of the swim-out under the Shore submarine hull. Engine-free.
// Codex #41 re-review 1bae48b5: native auto float-up is commented out (PlayerMovement.cpp ProcessSwimming) and the
// sticky surface only acts within 1 m of the surface, so releasing the descent at 78-79 m left both real swim-outs
// deep under the open sea (timeout at z 78.7 / 78.1, surface 99.5). The native controlled ascent is ACTION_JUMP held
// under water (ProcessSwimming: up control when well below the surface after 0.5 s in water); it is released 1.5 m
// below the surface so the native swim-jump near the surface (|relativeWaterLevel| < 0.1) is never triggered.
#pragma once

namespace WarsBots
{
// -1: descend (ACTION_CROUCH) while a deck/hull is overhead; +1: ascend (ACTION_JUMP) with open sky while deeper than
// `surfaceGap` under `surfaceZ`; 0: neither (near the surface the native sticky surface holds the swimmer).
inline int SubSwimVertical(bool solidAbove, float surfaceZ, float feetZ, float surfaceGap = 1.5f)
{
	if (solidAbove) return -1;
	return surfaceZ - feetZ > surfaceGap ? 1 : 0;
}
}
