// Lift executor (C-036 family, D-010 Claude module): how a bot rides an elevator link. Armada has three Elevator
// entities (two carrier lifts hangar 110 m <-> flight deck 121 m, fFloorHeight 11; island lift 117.75 <-> 122.5) and
// nine ElevatorSwitch entities (RECAST-BAKE v3, evidence/recast/movers.json). In Crysis a lift moves when a player
// USES a switch; the platform then slides at ~1 m/s (Slide fSpeed="1"). Ladder links reuse the jump executor's
// approach + a climb hold and are not handled here.
//
// States: GoToSwitch -> UseSwitch -> WaitPlatform (platform at our floor and stopped) -> Board (stand on the
// platform centre) -> Ride (platform moving / arriving) -> Exit (walk to the exit point) -> Done; Failed on timeouts.
// The caller does the engine side: moves toward `moveTo`, sends USE when `use` is true, reads the platform entity.
// Pure logic, no engine types; Tests/BotMoverLinkTests.cpp.
#pragma once
#include <cmath>

namespace WarsBots {

enum class MoverState { Idle = 0, GoToSwitch, UseSwitch, WaitPlatform, Board, Ride, Exit, Done, Failed };
enum class MoverFail { None = 0, SwitchTimeout, PlatformTimeout, BoardTimeout, RideTimeout, ExitTimeout, FellOff };

struct MV3 { float x = 0, y = 0, z = 0; };

struct MoverLink {
	MV3 switchPos;          // switch on the bot's floor (use from <= useRange)
	MV3 platformCentre;     // platform centre at the bot's floor (x, y; z = floor height there)
	float fromZ = 0.f, toZ = 0.f;  // floor heights (walk surface) at start and destination
	MV3 exitPos;            // first navmesh point after leaving the platform at the destination
};

struct MoverInput {
	float now = 0.f;
	MV3 pos;                // bot feet
	MV3 platform;           // platform entity position (its walk surface height in z)
	float platformSpeed = 0.f;  // |vz|, m/s
};

struct MoverOutput {
	MoverState state = MoverState::Idle;
	MoverFail fail = MoverFail::None;
	MV3 moveTo;             // where to walk (or stand) this tick
	bool move = false;      // false = stand still
	bool use = false;       // press USE this tick (one-shot)
};

struct MoverParams {
	float useRange = 1.6f;      // m to the switch
	float boardRadius = 1.0f;   // m from the platform centre counts as "on"
	float floorTol = 0.35f;     // m: platform surface at a floor
	float stopSpeed = 0.05f;    // m/s: platform stopped
	float switchTimeout = 15.f, platformTimeout = 25.f, boardTimeout = 6.f, rideTimeout = 30.f, exitTimeout = 6.f;
	float reUseEvery = 4.f;     // s: press again while waiting (switch may have sent the lift elsewhere)
};

class BotMoverLink {
public:
	explicit BotMoverLink(const MoverParams& p = MoverParams()) : m_p(p) {}
	void Start(const MoverLink& l, float now) { m_l = l; m_state = MoverState::GoToSwitch; m_t = now; m_lastUse = -1e9f; m_fail = MoverFail::None; }
	MoverState State() const { return m_state; }
	bool Active() const { return m_state != MoverState::Idle && m_state != MoverState::Done && m_state != MoverState::Failed; }

	MoverOutput Update(const MoverInput& in)
	{
		MoverOutput o; o.state = m_state;
		const float el = in.now - m_t;
		const bool platAtFrom = std::fabs(in.platform.z - m_l.fromZ) <= m_p.floorTol && in.platformSpeed <= m_p.stopSpeed;
		const bool platAtTo = std::fabs(in.platform.z - m_l.toZ) <= m_p.floorTol && in.platformSpeed <= m_p.stopSpeed;
		const bool onPlat = D2(in.pos, in.platform) <= m_p.boardRadius && std::fabs(in.pos.z - in.platform.z) <= 0.6f;
		switch (m_state)
		{
		case MoverState::GoToSwitch:
			if (platAtFrom) { Go(MoverState::Board, in.now); break; }          // lift already here: just board
			if (D2(in.pos, m_l.switchPos) <= m_p.useRange) { Go(MoverState::UseSwitch, in.now); break; }
			if (el > m_p.switchTimeout) { Fail(MoverFail::SwitchTimeout); break; }
			o.moveTo = m_l.switchPos; o.move = true; break;
		case MoverState::UseSwitch:
			o.use = true; m_lastUse = in.now; o.moveTo = in.pos; Go(MoverState::WaitPlatform, in.now); break;
		case MoverState::WaitPlatform:
			if (platAtFrom) { Go(MoverState::Board, in.now); break; }
			if (el > m_p.platformTimeout) { Fail(MoverFail::PlatformTimeout); break; }
			if (in.now - m_lastUse >= m_p.reUseEvery && D2(in.pos, m_l.switchPos) <= m_p.useRange) { o.use = true; m_lastUse = in.now; }
			o.moveTo = m_l.switchPos; o.move = D2(in.pos, m_l.switchPos) > m_p.useRange; break;
		case MoverState::Board:
			if (onPlat) { Go(MoverState::Ride, in.now); m_rideUsed = false; break; }
			if (!platAtFrom && el > 0.5f) { Go(MoverState::WaitPlatform, in.now); break; }   // lift left without us
			if (el > m_p.boardTimeout) { Fail(MoverFail::BoardTimeout); break; }
			o.moveTo = MV3{m_l.platformCentre.x, m_l.platformCentre.y, in.platform.z}; o.move = true; break;
		case MoverState::Ride:
			if (!onPlat && std::fabs(in.pos.z - m_l.toZ) > m_p.floorTol) { Fail(MoverFail::FellOff); break; }
			if (platAtTo) { Go(MoverState::Exit, in.now); break; }
			if (el > m_p.rideTimeout) { Fail(MoverFail::RideTimeout); break; }
			// a lift with switches only at the floors may need the platform's own use; press once if it is still idle
			if (!m_rideUsed && in.platformSpeed <= m_p.stopSpeed && el > 1.0f) { o.use = true; m_rideUsed = true; }
			o.moveTo = MV3{m_l.platformCentre.x, m_l.platformCentre.y, in.platform.z}; o.move = D2(in.pos, in.platform) > 0.4f; break;
		case MoverState::Exit:
			if (D2(in.pos, m_l.exitPos) <= 0.6f && std::fabs(in.pos.z - m_l.toZ) <= 0.6f) { Go(MoverState::Done, in.now); break; }
			if (el > m_p.exitTimeout) { Fail(MoverFail::ExitTimeout); break; }
			o.moveTo = m_l.exitPos; o.move = true; break;
		default: break;
		}
		o.state = m_state; o.fail = m_fail;
		return o;
	}

private:
	static float D2(const MV3& a, const MV3& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }
	void Go(MoverState s, float now) { m_state = s; m_t = now; }
	void Fail(MoverFail f) { m_state = MoverState::Failed; m_fail = f; }

	MoverParams m_p;
	MoverLink m_l;
	MoverState m_state = MoverState::Idle;
	MoverFail m_fail = MoverFail::None;
	float m_t = 0.f, m_lastUse = -1e9f;
	bool m_rideUsed = false;
};

} // namespace WarsBots
