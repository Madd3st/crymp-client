// Client-side smoothing of a replicated WarsBot pose (O-025 causes 1 and 3, default OFF: wb_bot_client_smooth).
// The server sends direction/aim only when they change (~10 Hz brain), so a client replica turns in steps: every
// packet the move direction and the look point jump and the view/body snap. This filters both on the client with a
// frame-rate independent exponential approach (time constant `tau`), so a remote bot turns in curves instead of
// steps. Large jumps (revive, teleport, > snapAngle) snap at once so the replica never lags a real discontinuity.
// Pure math, no engine types; Player.cpp converts Vec3 <-> RVec.
#pragma once
#include <cmath>

namespace WarsBots {

struct RVec { float x = 0.f, y = 0.f, z = 0.f; };

struct ReplicaSmoothParams {
	float dirTau = 0.08f;      // s: move direction (≈ one 10 Hz interval, keeps the bot on its path)
	float aimTau = 0.05f;      // s: look/aim point; short so tracers stay close to the shown view
	float snapAngle = 120.f;   // deg: bigger jumps are taken immediately
	float snapDist = 8.f;      // m: an aim point that moved farther than this (new target) re-bases its distance
};

class BotReplicaSmooth {
public:
	explicit BotReplicaSmooth(const ReplicaSmoothParams& p = ReplicaSmoothParams()) : m_p(p) {}
	void Reset() { m_init = false; }
	bool Initialized() const { return m_init; }

	// dir: replicated horizontal move direction; aim: replicated world aim point; eye: replica eye position.
	void Update(float dt, const RVec& dir, const RVec& aim, const RVec& eye)
	{
		const float targetYaw = std::atan2(dir.x, dir.y);
		RVec rel{aim.x - eye.x, aim.y - eye.y, aim.z - eye.z};
		const float dist = Len(rel);
		const float aYaw = std::atan2(rel.x, rel.y);
		const float aPitch = std::atan2(rel.z, std::sqrt(rel.x * rel.x + rel.y * rel.y) + 1e-4f);
		if (!m_init || dt <= 0.f)
		{
			m_dirYaw = targetYaw; m_aimYaw = aYaw; m_aimPitch = aPitch; m_aimDist = dist; m_init = true;
			return;
		}
		m_dirYaw = Approach(m_dirYaw, targetYaw, dt, m_p.dirTau);
		m_aimYaw = Approach(m_aimYaw, aYaw, dt, m_p.aimTau);
		const float kp = Alpha(dt, m_p.aimTau);
		m_aimPitch += (aPitch - m_aimPitch) * kp;
		m_aimDist = std::fabs(dist - m_aimDist) > m_p.snapDist ? dist : m_aimDist + (dist - m_aimDist) * kp;
	}

	RVec Dir() const { return RVec{std::sin(m_dirYaw), std::cos(m_dirYaw), 0.f}; }
	RVec Aim(const RVec& eye) const
	{
		const float c = std::cos(m_aimPitch);
		return RVec{eye.x + std::sin(m_aimYaw) * c * m_aimDist, eye.y + std::cos(m_aimYaw) * c * m_aimDist,
			eye.z + std::sin(m_aimPitch) * m_aimDist};
	}
	float DirYawDeg() const { return m_dirYaw * 57.2957795f; }
	float AimYawDeg() const { return m_aimYaw * 57.2957795f; }

	static float Wrap(float a) { const float pi = 3.14159265f; a = std::fmod(a + pi, 2.f * pi); if (a < 0.f) a += 2.f * pi; return a - pi; }

private:
	static float Len(const RVec& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
	static float Alpha(float dt, float tau) { return tau <= 0.f ? 1.f : 1.f - std::exp(-dt / tau); }
	float Approach(float cur, float target, float dt, float tau) const
	{
		const float d = Wrap(target - cur);
		if (std::fabs(d) * 57.2957795f > m_p.snapAngle) return target;
		return Wrap(cur + d * Alpha(dt, tau));
	}

	ReplicaSmoothParams m_p;
	bool m_init = false;
	float m_dirYaw = 0.f, m_aimYaw = 0.f, m_aimPitch = 0.f, m_aimDist = 10.f;
};

} // namespace WarsBots
