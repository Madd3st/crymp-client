// Decision commitment gate (DS-034). Engine-free draft for the T-1 "one owner per decision" refactor.
// The bot today writes its state from several places in one 10 Hz tick (UpdateRoam -> SEARCHING_ENEMY, then
// UpdateUpgrade -> MOVING_TO_WEAPON), and ATTACKING/CHASING flips at the engage/hold range boundary. This gate
// accepts at most one state change per decision tick, enforces a minimum dwell for non-urgent changes and makes
// conflicts observable instead of silent.
//
// Integration sketch (not applied to game code):
//   m_commit.BeginTick(m_decisionSerial, m_now);
//   Change(state): if (!m_commit.Offer(owner, state, m_now, urgent)) return; ... existing body ...
//   at the end: if (m_commit.ConflictsThisTick()) WB_LOG(... "STATE CONFLICT first=%s second=%s");
//
// Pure C++17, no engine types; Tests in decision_commit_test.cpp.
#pragma once
#include <cstdint>

namespace WarsBots {

enum class DecisionOwner : uint8_t { Arbiter = 0, Upgrade, Roam, Combat, Look, Range, Finish, Count };
enum class DecisionState : uint8_t { Spawning = 0, SeekWeapon, MoveToWeapon, Search, Chase, Attack, Dead, Respawn, Count };

struct CommitParams {
    float minDwell = 0.30f;      // s: a non-urgent state must live this long
    float attackLeave = 0.50f;   // s: ATTACKING may not fall back to CHASE sooner
    float conflictLogGap = 1.0f; // s: rate-limit conflict logs
};

class DecisionCommit {
public:
    explicit DecisionCommit(const CommitParams& p = CommitParams()) : m_p(p) {}

    void Reset()
    {
        m_state = DecisionState::Spawning; m_owner = DecisionOwner::Arbiter;
        m_stateAt = -1e9f; m_tick = 0; m_tickHadChange = false; m_conflicts = 0; m_lastConflictLog = -1e9f;
        m_firstOwner = m_secondOwner = DecisionOwner::Arbiter; m_firstState = m_secondState = DecisionState::Spawning;
    }

    // One decision tick (10 Hz). The first Offer after BeginTick may change state; later ones are conflicts.
    void BeginTick(uint32_t tick, float now) { m_tick = tick; m_tickHadChange = false; m_tickConflicts = 0; m_now = now; }

    bool Offer(DecisionOwner owner, DecisionState state, float now, bool urgent)
    {
        m_now = now;
        if (state == m_state)
        {
            m_owner = owner; // same state, a different writer is harmless
            return true;
        }
        if (m_tickHadChange)
        {
            ++m_conflicts; ++m_tickConflicts;
            if (m_tickConflicts == 1) { m_secondOwner = owner; m_secondState = state; }
            return false; // one state per tick: the first writer wins
        }
        if (!urgent && CanLeave(now) == false) return false;
        if (!urgent && m_state == DecisionState::Attack && now - m_stateAt < m_p.attackLeave) return false;
        m_firstOwner = m_owner; m_firstState = m_state;
        m_state = state; m_owner = owner; m_stateAt = now; m_tickHadChange = true;
        return true;
    }

    // True when leaving the current state is allowed (dwell elapsed). Urgent callers ignore this.
    bool CanLeave(float now) const { return (now - m_stateAt) >= m_p.minDwell; }

    DecisionState State() const { return m_state; }
    DecisionOwner Owner() const { return m_owner; }
    float StateAge(float now) const { return now - m_stateAt; }
    int Conflicts() const { return m_conflicts; }
    int TickConflicts() const { return m_tickConflicts; }
    bool ChangedThisTick() const { return m_tickHadChange; }
    DecisionOwner FirstOwner() const { return m_firstOwner; }
    DecisionOwner SecondOwner() const { return m_secondOwner; }
    DecisionState FirstState() const { return m_firstState; }
    DecisionState SecondState() const { return m_secondState; }

    // Rate-limited report: returns true at most once per conflictLogGap.
    bool ShouldLogConflict(float now)
    {
        if (!m_tickConflicts) return false;
        if (now - m_lastConflictLog < m_p.conflictLogGap) return false;
        m_lastConflictLog = now; return true;
    }

private:
    CommitParams m_p;
    DecisionState m_state = DecisionState::Spawning;
    DecisionOwner m_owner = DecisionOwner::Arbiter;
    float m_stateAt = 0.f, m_now = 0.f, m_lastConflictLog = -1e9f;
    uint32_t m_tick = 0;
    bool m_tickHadChange = false;
    int m_tickConflicts = 0, m_conflicts = 0;
    DecisionOwner m_firstOwner = DecisionOwner::Arbiter, m_secondOwner = DecisionOwner::Arbiter;
    DecisionState m_firstState = DecisionState::Spawning, m_secondState = DecisionState::Spawning;
};

// Movement-direction hysteresis: hold the previous destination until it is > minMove away or maxAge elapsed.
// Prevents two writers (roam point vs weapon) from alternating the destination every tick.
struct MoveCommit {
    float minMove = 0.75f;
    float maxAge = 2.5f;
    bool Valid(float x, float y, float z, float now) const
    {
        if (!m_valid) return false;
        const float dx = x - m_x, dy = y - m_y, dz = z - m_z;
        return (dx * dx + dy * dy + dz * dz) < minMove * minMove && (now - m_at) < maxAge;
    }
    void Set(float x, float y, float z, float now) { m_x = x; m_y = y; m_z = z; m_at = now; m_valid = true; }
    void Reset() { m_valid = false; }
private:
    float m_x = 0, m_y = 0, m_z = 0, m_at = 0;
    bool m_valid = false;
};

} // namespace WarsBots

