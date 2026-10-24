// WarsBots project addition. See LICENSE.txt for SDK usage terms.
#pragma once

struct IGameFramework;
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
#include "CryCommon/CryAction/IGameFramework.h"
#include "CryCommon/CryAction/ILevelSystem.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryGame/Bots/BotInstance.h"
#include "CryGame/Bots/BotNav.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct IConsoleCmdArgs;
struct HitInfo;
struct IWeapon;
#endif

namespace WarsBots
{
#if defined(WARSBOTS_EXPERIMENTAL_SPAWNER)
// Registry of independent bots keyed by EntityId. Owns listeners, console commands and
// lifecycle. Each BotInstance owns its own brain/combat/locomotion/suit state.
class BotManager final : public ILevelSystemListener,
                         public IEntitySystemSink,
                         public IGameFrameworkListener
{
public:
	static constexpr int kHardLimit = 16;

	void Initialize(IGameFramework* framework);
	void Shutdown();

	// Registry queries used by scoped Player/GameRules/ShotValidator hooks.
	bool Owns(EntityId id) const;
	BotInstance* Find(EntityId id) const;
	bool AIEnabled(EntityId id) const;
	bool IsCreatingActor(const char* entityName) const;
	bool IsOwnedProjectileHit(const HitInfo& hit) const;
	void TraceHit(const char* phase, const HitInfo& hit);
    void TraceShot(EntityId weaponId, EntityId shooter, EntityId projectile);
	void TraceKill(EntityId targetId, EntityId shooterId, bool killerEventEmitted);

	bool CanRunLocalIA() const;
	IGameFramework* Framework() const { return m_framework; }
	const BotNav& Nav() const { return m_nav; }
	BotNav& Nav() { return m_nav; }
	int Count() const { return int(m_bots.size()); }
	int MaxBots() const;

	// Spawn point selection shared by initial spawn and revive: safe from other Player
	// bodies and level geometry; `avoid` is used only when nothing else passes.
	EntityId ChooseSpawnLocation(EntityId self, EntityId avoid) const;

	// Called by BotInstance::Spawn so ownership is visible during IA registration.
	void Adopt(BotInstance& instance) { m_spawning = &instance; }
	struct CreationScope
	{
		CreationScope(BotManager& m, const char* name) : manager(m) { manager.m_creatingName = name; }
		~CreationScope() { manager.m_creatingName.clear(); }
		BotManager& manager;
	};

	// ILevelSystemListener
	void OnLevelNotFound(const char*) override;
	void OnLoadingStart(ILevelInfo*) override;
	void OnLoadingComplete(ILevel*) override;
	void OnLoadingError(ILevelInfo*, const char*) override;
	void OnLoadingProgress(ILevelInfo*, int) override {}
	// IEntitySystemSink
	bool OnBeforeSpawn(SEntitySpawnParams&) override { return true; }
	void OnSpawn(IEntity*, SEntitySpawnParams&) override {}
	bool OnRemove(IEntity*) override { return true; }
	void OnEvent(IEntity* entity, SEntityEvent& event) override;
	// IGameFrameworkListener
	void OnPostUpdate(float dt) override;
	void OnSaveGame(ISaveGame*) override {}
	void OnLoadGame(ILoadGame*) override;
	void OnLevelEnd(const char*) override;
	void OnActionEvent(const SActionEvent& event) override;

private:
	BotInstance* Add(const Vec3* position, BotSkill skill, const BotStyle* style, bool startAI);
	BotInstance* AddWith(const Vec3* position, const BotProfile& profile, int number, bool startAI);
	void RestoreRoster();
	void RemoveInstance(BotInstance& instance);
	void RemoveAll(const char* reason);
	void Sweep();
	void EndLevel();
	int FreeNumber() const;
	BotInstance* ByNumber(int number) const;
	BotInstance* Selected() const;
	std::vector<BotInstance*> Ordered() const;
	void List() const;
	void StatusReport();

	static BotManager* Self();
	static void AddCommand(IConsoleCmdArgs* args);
	static void RemoveCommand(IConsoleCmdArgs* args);
	static void RemoveAllCommand(IConsoleCmdArgs* args);
	static void ListCommand(IConsoleCmdArgs* args);
	static void StatusCommand(IConsoleCmdArgs* args);
	static void DebugCommand(IConsoleCmdArgs* args);
	static void SelectCommand(IConsoleCmdArgs* args);
	static void NavCommand(IConsoleCmdArgs* args);
    static void WatchCommand(IConsoleCmdArgs* args);
    static void WatchInfoCommand(IConsoleCmdArgs* args);
    static void WatchCheckCommand(IConsoleCmdArgs* args);
	void ResetNav() { m_nav.Reset(); }
	// Legacy single-bot test commands, now addressed to the selected bot.
	static void ProbeSpawnCommand(IConsoleCmdArgs* args);
	static void ProbeRemoveCommand(IConsoleCmdArgs* args);
	static void ProbeStatusCommand(IConsoleCmdArgs* args);
	static void ProbeReviveCommand(IConsoleCmdArgs* args);
	static void BotStartCommand(IConsoleCmdArgs* args);
	static void BotStopCommand(IConsoleCmdArgs* args);
	static void BotStatusCommand(IConsoleCmdArgs* args);
	static void BotVerboseCommand(IConsoleCmdArgs* args);
	static void BotAbilityCommand(IConsoleCmdArgs* args);

	IGameFramework* m_framework = nullptr;
	bool m_initialized = false;
	bool m_levelReady = false;
	bool m_defaultVerbose = false;
	int m_localTestEnabled = 0;
	int m_cloakExperimental = 0;
	int m_maxBots = 6;
	int m_logLevel = 1;
	int m_skill = int(BotSkill::Normal);
	int m_selected = 0;
	std::string m_creatingName;
	BotInstance* m_spawning = nullptr;
	// Game-rules restart (PreGame -> InGame) resets entities: bots are re-created with the
	// same numbers/profiles after eAE_resetEnd instead of silently disappearing.
	struct RosterEntry { int number; BotProfile profile; bool ai; };
	std::vector<RosterEntry> m_roster;
	bool m_restorePending = false;
	std::unordered_map<EntityId, std::unique_ptr<BotInstance>> m_bots;
	float m_nextProfileLog = 0;
	float m_clock = 0;
	BotNav m_nav;
};
#endif
}
