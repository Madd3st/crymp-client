#include "CryGame/Bots/BotJumpLink.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace WarsBots;

// Tiny kinematic body: walks toward moveTo at run/sprint speed, jumps ballistically, lands on a flat floor or a
// ledge (height `ledgeZ` for x >= ledgeX). Enough to exercise every executor state.
struct Sim {
	JumpLinkBody b; float vz = 0.f, vx = 0.f, vy = 0.f; float ledgeX = 1e9f, ledgeZ = 0.f; bool suitStrength = false;
	float suitDelay = 0.3f, suitAsked = -1.f; float g = 9.81f;
	float Floor(float x) const { return x >= ledgeX ? ledgeZ : 0.f; }
	void Step(const JumpLinkCommand& c, float dt)
	{
		if (c.suit == LinkSuit::Strength && suitAsked < 0) suitAsked = b.now;
		if (c.suit == LinkSuit::Armor) { suitStrength = false; suitAsked = -1.f; }
		if (suitAsked >= 0 && b.now - suitAsked >= suitDelay) suitStrength = true;
		b.strengthActive = suitStrength;
		const float spd = c.speed > 0.f ? c.speed : (c.sprint ? 6.75f : 4.5f);
		if (b.grounded)
		{
			vx = vy = 0.f;
			if (c.move) { float dx = c.moveTo.x - b.pos.x, dy = c.moveTo.y - b.pos.y, d = std::sqrt(dx * dx + dy * dy);
				if (d > 1e-3f) { float s = d < spd * dt ? d / dt : spd; vx = dx / d * s; vy = dy / d * s; } }
			if (c.jump == LinkJump::Normal) { vz = 4.43f; b.grounded = false; }
			if (c.jump == LinkJump::Strength && suitStrength && b.energy >= 70) { vz = 9.60f; b.energy -= 70; b.grounded = false; }
			if (c.jump != LinkJump::None && !b.grounded && c.move) { float dx = c.moveTo.x - b.pos.x, dy = c.moveTo.y - b.pos.y, d = std::sqrt(dx * dx + dy * dy);
				if (d > 1e-3f) { vx = dx / d * spd; vy = dy / d * spd; } }
		}
		const float nx = b.pos.x + vx * dt;
		if (b.pos.x < ledgeX && nx >= ledgeX && b.pos.z < ledgeZ - 0.05f) { vx = 0.f; b.pos.x = ledgeX - 0.01f; } // ledge wall
		else b.pos.x = nx;
		b.pos.y += vy * dt;
		if (!b.grounded)
		{
			if (c.move) {                              // limited air control toward moveTo (Crysis air control 0.9)
				float dx = c.moveTo.x - b.pos.x, dy = c.moveTo.y - b.pos.y, d = std::sqrt(dx * dx + dy * dy);
				float tx = d > 0.05f ? dx / d * spd : 0.f, ty = d > 0.05f ? dy / d * spd : 0.f;
				vx += (tx - vx) * 0.1f; vy += (ty - vy) * 0.1f; }
			b.pos.z += vz * dt; vz -= g * dt;
			if (vz < 0 && b.pos.z <= Floor(b.pos.x)) { b.pos.z = Floor(b.pos.x); b.grounded = true; vz = 0; vx = vy = 0; }
		}
		else b.pos.z = Floor(b.pos.x);
		b.speed = std::sqrt(vx * vx + vy * vy);
		b.now += dt;
	}
};

static LinkState Run(Sim& s, JumpLinkExecutor& e, float seconds = 10.f)
{
	for (int i = 0; i < int(seconds / 0.05f) && e.Running(); ++i) s.Step(e.Update(s.b), 0.05f);
	return e.State();
}

int main()
{
	// 1. Strength jump onto a 4 m ledge 2.5 m away: approach, wait for the suit, jump once, land within 1.5 m.
	{
		Sim s; s.ledgeX = 6.f; s.ledgeZ = 4.f; s.b.pos = {0, 0, 0};
		JumpLinkDef L; L.id = 7; L.type = LinkType::Strength; L.start = {4, 0, 0}; L.end = {6.5f, 0, 4};
		JumpLinkExecutor e; e.Begin(L, 0.f);
		assert(Run(s, e) == LinkState::Done);
		assert(std::fabs(s.b.pos.z - 4.f) < 0.01f && s.b.energy <= 130.f);
	}
	// 2. Not enough energy for Strength -> NoEnergy, no jump.
	{
		Sim s; s.b.energy = 40.f; s.ledgeX = 6.f; s.ledgeZ = 4.f;
		JumpLinkDef L; L.type = LinkType::Strength; L.start = {1, 0, 0}; L.end = {6.5f, 0, 4};
		JumpLinkExecutor e; e.Begin(L, 0.f);
		assert(Run(s, e) == LinkState::Failed && e.Failure() == LinkFail::NoEnergy);
	}
	// 3. Suit never switches -> SuitTimeout after 1.5 s at the edge.
	{
		Sim s; s.suitDelay = 99.f; s.ledgeX = 6.f; s.ledgeZ = 4.f;
		JumpLinkDef L; L.type = LinkType::Strength; L.start = {1, 0, 0}; L.end = {6.5f, 0, 4};
		JumpLinkExecutor e; e.Begin(L, 0.f);
		assert(Run(s, e) == LinkState::Failed && e.Failure() == LinkFail::SuitTimeout);
	}
	// 4. Normal jump at a 3 m high ledge is impossible -> lands back on the floor -> Fell/MissedLanding, link banned.
	{
		Sim s; s.ledgeX = 6.f; s.ledgeZ = 3.f;
		JumpLinkDef L; L.id = 9; L.type = LinkType::Normal; L.start = {5, 0, 0}; L.end = {6.5f, 0, 3};
		JumpLinkExecutor e; JumpLinkBans bans; e.Begin(L, 0.f);
		assert(Run(s, e) == LinkState::Failed);
		assert(e.Failure() == LinkFail::Fell || e.Failure() == LinkFail::MissedLanding);
		bans.Ban(e.LinkId()); assert(bans.Banned(9) && !bans.Banned(7)); bans.Clear(); assert(!bans.Banned(9));
	}
	// 5. Normal gap jump 3 m across on flat ground with sprint: Done.
	{
		Sim s;
		JumpLinkDef L; L.type = LinkType::Normal; L.start = {2, 0, 0}; L.end = {5.5f, 0, 0};
		JumpLinkExecutor e; e.Begin(L, 0.f);
		assert(Run(s, e) == LinkState::Done);
	}
	// 6. Brain lock only during prepare/launch/air; approach can be interrupted.
	{
		Sim s; s.ledgeX = 6.f; s.ledgeZ = 4.f;
		JumpLinkDef L; L.type = LinkType::Strength; L.start = {4, 0, 0}; L.end = {6.5f, 0, 4};
		JumpLinkExecutor e; e.Begin(L, 0.f);
		s.Step(e.Update(s.b), 0.05f); assert(e.State() == LinkState::Approach && !e.HoldsBrain());
		bool held = false; for (int i = 0; i < 200 && e.Running(); ++i) { s.Step(e.Update(s.b), 0.05f); held |= e.HoldsBrain(); }
		assert(held && e.State() == LinkState::Done);
	}
	// 7. Unreachable start -> ApproachTimeout.
	{
		Sim s; JumpLinkDef L; L.start = {100, 0, 0}; L.end = {103, 0, 0};
		JumpLinkExecutor e; e.Begin(L, 0.f);
		assert(Run(s, e) == LinkState::Failed && e.Failure() == LinkFail::ApproachTimeout);
	}
	std::puts("BotJumpLinkTests OK");
	return 0;
}
