// WarsBots project addition. See LICENSE.txt for SDK usage terms.
#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotManager.h"
#include "version.h"
#include "CryGame/Bots/BotCombat.h"
#include "CryGame/Bots/BotMatch.h"
#include "CryGame/Bots/BotSpectator.h"
#include "CryGame/Bots/CryMPEngineAdapter.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Actor.h"
#include "CryGame/Actors/Player/Player.h"
#include "CryGame/Actors/Player/NanoSuit.h"
#include "CryCommon/CryAction/IWeapon.h"
#include "CryGame/Items/Weapons/WeaponSystem.h"
#include "CryGame/Items/Weapons/Projectile.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace WarsBots
{
namespace
{
int g_progressExperiment=0;
int g_trapRecovery=0;
int g_physicsDiagnostics=0;
int g_reachableGoals=0;
int g_sinkEscape=0;
int g_hotspotAllNodes=0;
int g_fireDiagnostics=0, g_fireBudget=20000;
int g_qaSocomOnly=0;
bool FireTrace() {
    if (!g_fireDiagnostics || !gEnv->bServer || g_fireBudget<=0) return false;
    if (--g_fireBudget==0) WB_LOG(1,"[WarsBots] FIRE TRACE capped; remaining episode context incomplete.");
    return true;
}
bool ParseCoordinate(const char* text, float& value)
{
	if (!text || !*text)
		return false;
	char* end = nullptr;
	errno = 0;
	value = std::strtof(text, &end);
	return end != text && *end == '\0' && errno != ERANGE &&
	       std::isfinite(value) && std::fabs(value) <= 1000000.0f;
}
bool ParseInt(const char* text, int& value)
{
	if (!text || !*text) return false;
	char* end = nullptr;
	errno = 0;
	const long parsed = std::strtol(text, &end, 10);
	if (end == text || *end || errno == ERANGE || parsed < -1000000 || parsed > 1000000) return false;
	value = int(parsed);
	return true;
}
bool ParsePosition(IConsoleCmdArgs* args, int first, Vec3& position)
{
	return args->GetArgCount() >= first + 3 && ParseCoordinate(args->GetArg(first), position.x) &&
		ParseCoordinate(args->GetArg(first + 1), position.y) && ParseCoordinate(args->GetArg(first + 2), position.z);
}
const char* kCommands[] = {
	"wb_revive_at", "wb_teleport", "wb_watch", "wb_watch_info", "wb_watch_check", "wb_nav_export", "wb_nav_explain",
	"wb_add", "wb_remove", "wb_remove_all", "wb_list", "wb_status", "wb_debug", "wb_select", "wb_nav",
	"wb_probe_spawn", "wb_probe_remove", "wb_probe_status", "wb_probe_revive",
	"wb_bot_start", "wb_bot_stop", "wb_bot_status", "wb_bot_verbose",
	"wb_bot_jump", "wb_bot_gait", "wb_bot_strafe", "wb_bot_suit", "wb_bot_abilities",
};
const char* kVariables[] = { "wb_sink_escape", "wb_reachable_goals", "wb_physics_diagnostics", "wb_hotspot_all_nodes", "wb_trap_recovery", "wb_probe_local_test", "wb_bot_cloak_experimental", "wb_maxbots", "wb_skill", "wb_log", "wb_progress_experiment", "wb_fire_diagnostics", "wb_qa_socom_only" };
}

BotManager* BotManager::Self()
{
	return g_pGame ? &g_pGame->GetBotManager() : nullptr;
}

void BotManager::Initialize(IGameFramework* framework)
{
	if (m_initialized || !framework)
		return;
	m_initialized = true;
	m_framework = framework;
	m_levelReady = framework->GetILevelSystem()->IsLevelLoaded();
	framework->GetILevelSystem()->AddListener(this);
	framework->RegisterListener(this, "WarsBots", FRAMEWORKLISTENERPRIORITY_GAME);
	gEnv->pEntitySystem->AddSink(this);

	IConsole* console = gEnv->pConsole;
	console->Register("wb_probe_local_test", &m_localTestEnabled, 0, 0,
		"Opt in to WarsBots; requires sv_bind=127.0.0.1, sv_lanonly=1 and InstantAction.");
	console->Register("wb_bot_cloak_experimental", &m_cloakExperimental, 0, 0, "Experimental bot cloak, defaults OFF");
    console->Register("wb_hotspot_all_nodes", &g_hotspotAllNodes, 0, 0, "Experimental complete hotspot scan: 0 legacy stride2 default, 1 all nodes; QA until 10min soak.");
    console->Register("wb_sink_escape", &g_sinkEscape, 0, 0, "Opt-in physical escape at8s in closed SCCs; requires trap recovery1; default OFF.");
    console->Register("wb_reachable_goals", &g_reachableGoals, 0, 0, "Opt-in graph reachability filter for weapon/hotspot goals; unknown projection kept, default OFF.");
    console->Register("wb_physics_diagnostics", &g_physicsDiagnostics, 0, 0, "Opt-in read-only body snapshots after create/revive, +1s, +5s; local IA only, default OFF.");
    console->Register("wb_trap_recovery", &g_trapRecovery, 0, 0, "Experimental local IA displacement watchdog: 0 OFF, 1 staged recovery incl native self-kill after 45s; QA only until 10min soak.");
	console->Register("wb_progress_experiment", &g_progressExperiment, 0, 0,
		"Experimental stationary-combat recovery: 0 OFF (default), 1 ON; local IA bots only.");
    console->Register("wb_qa_socom_only", &g_qaSocomOnly, 0, 0, "Local IA test only: suppress weapon upgrades/fallback, retain native SOCOM/ammo; default OFF.");
    console->Register("wb_fire_diagnostics", &g_fireDiagnostics, 0, 0, "Opt-in shot/hit diagnostics; at most 20000 records per level; default OFF.");
	console->Register("wb_log", &m_logLevel, 1, 0, "WarsBots console log: 0 essential, 1 +kills/revives (default), 2 +telemetry. Telemetry always goes to the log file.");
	console->Register("wb_maxbots", &m_maxBots, 6, 0, "Maximum simultaneous WarsBots (1..16, default 6)");
	console->Register("wb_skill", &m_skill, int(BotSkill::Normal), 0,
		"Default skill for wb_add: 0 beginner, 1 normal, 2 veteran, 3 elite");
    console->AddCommand("wb_watch", WatchCommand, 0, "Spectators only: wb_watch list|<botNumber>|next|prev|status|off; native F1 toggles first person");
    console->AddCommand("wb_watch_info", WatchInfoCommand, 0, "Local IA server only: decisions of bots followed by real spectators");
    console->AddCommand("wb_watch_check", WatchCheckCommand, 0, "Local IA server/no clients: assert native Lua spectator selection without changing any actor");
	console->AddCommand("wb_add", AddCommand, 0, "wb_add [count] [beginner|normal|veteran|elite] [style]: add autonomous bots");
	console->AddCommand("wb_remove", RemoveCommand, 0, "wb_remove <botNumber>: remove one bot (number from wb_list, not EntityId)");
	console->AddCommand("wb_remove_all", RemoveAllCommand, 0, "Remove every WarsBot");
	console->AddCommand("wb_list", ListCommand, 0, "List bots: number, EntityId, profile, state");
	console->AddCommand("wb_status", StatusCommand, 0, "Detailed status and per-bot update cost");
	console->AddCommand("wb_debug", DebugCommand, 0, "wb_debug 0|1: verbose telemetry for all bots");
    console->AddCommand("wb_revive_at",ReviveAtCommand,0,"wb_revive_at <spawnEntityId> [botNumber|me]: manual native kill/revive; selected bot by default; local IA only");
    console->AddCommand("wb_teleport",TeleportCommand,0,"wb_teleport <botNumber> x y z: manual owned live bot relocation; local IA only");
	console->AddCommand("wb_nav_explain", NavExplainCommand, 0, "wb_nav_explain <fromNode> <toNode> or after <seconds> <from> <to>: local IA bidirectional ray diagnostics");
	console->AddCommand("wb_nav_export", NavExportCommand, 0, "wb_nav_export [path.json] or after <1..30 seconds>: local IA graph snapshot, no rays/memory changes");
	console->AddCommand("wb_nav", NavCommand, 0, "Navigation graph status for this level");
	console->AddCommand("wb_select", SelectCommand, 0, "wb_select <botNumber>: target of the legacy wb_bot_*/wb_probe_* commands");
	console->AddCommand("wb_probe_spawn", ProbeSpawnCommand, 0, "wb_probe_spawn x y z: add one bot without AI");
	console->AddCommand("wb_probe_remove", ProbeRemoveCommand, 0, "Remove the selected bot");
	console->AddCommand("wb_probe_status", ProbeStatusCommand, 0, "Status of the selected bot");
	console->AddCommand("wb_probe_revive", ProbeReviveCommand, 0, "Revive the selected dead bot");
	console->AddCommand("wb_bot_start", BotStartCommand, 0, "wb_bot_start [x y z]: start AI on the selected bot (adds one if none)");
	console->AddCommand("wb_bot_stop", BotStopCommand, 0, "Stop AI of the selected bot");
	console->AddCommand("wb_bot_status", BotStatusCommand, 0, "AI status of the selected bot");
	console->AddCommand("wb_bot_verbose", BotVerboseCommand, 0, "wb_bot_verbose 0|1 for the selected bot and new bots");
	console->AddCommand("wb_bot_jump", BotAbilityCommand, 0, "wb_bot_jump [normal|strength] [delay]: selected bot");
	console->AddCommand("wb_bot_strafe", BotAbilityCommand, 0, "wb_bot_strafe left|right|auto: selected bot");
	console->AddCommand("wb_bot_gait", BotAbilityCommand, 0, "wb_bot_gait walk|run|sprint|auto: selected bot");
	console->AddCommand("wb_bot_suit", BotAbilityCommand, 0, "wb_bot_suit defense|speed|strength|cloak|auto: selected bot");
	console->AddCommand("wb_bot_abilities", BotAbilityCommand, 0, "Ability and physical state of the selected bot");
	WB_LOG(0, "[WarsBots] BotManager initialized (build " CRYMP_VERSION_STRING "): registry by EntityId, limit wb_maxbots=%d (hard %d); gate wb_probe_local_test=0; no automatic spawn.",
		m_maxBots, kHardLimit);
}

void BotManager::Shutdown()
{
	if (!m_initialized)
		return;
	m_initialized = false;
	for (const char* name : kCommands) gEnv->pConsole->RemoveCommand(name);
	for (const char* name : kVariables) gEnv->pConsole->UnregisterVariable(name, true);
	EndLevel();
	m_nav.Reset();
	gEnv->pEntitySystem->RemoveSink(this);
	m_framework->GetILevelSystem()->RemoveListener(this);
	m_framework->UnregisterListener(this);
	m_framework = nullptr;
	if (gEnv && gEnv->pLog)
		WB_LOG(0, "[WarsBots] BotManager shut down.");
}

// ---------------------------------------------------------------- registry queries

BotInstance* BotManager::Find(EntityId id) const
{
	if (!id) return nullptr;
	if (m_spawning && m_spawning->Owns(id)) return m_spawning;
	auto it = m_bots.find(id);
	return it != m_bots.end() && it->second->Owns(id) ? it->second.get() : nullptr;
}

bool BotManager::Owns(EntityId id) const { return Find(id) != nullptr; }

bool BotManager::AIEnabled(EntityId id) const
{
	const BotInstance* bot = Find(id);
	return bot && bot->AIEnabled();
}

bool BotManager::IsCreatingActor(const char* entityName) const
{
	return !m_creatingName.empty() && entityName && m_creatingName == entityName;
}

bool BotManager::IsOwnedProjectileHit(const HitInfo& hit) const
{
	const BotInstance* bot = Find(hit.shooterId);
	return bot && bot->IsOwnedProjectileHit(hit);
}

void BotManager::TraceHit(const char* phase, const HitInfo& hit)
{
	if (!m_framework || !gEnv->bServer) return;
	BotInstance* target = Find(hit.targetId);
	BotInstance* shooter = Find(hit.shooterId);
	if (!target && !shooter) return; // Human-vs-human hits are none of our business.
	if (target) target->TraceHit(phase, hit);
	IActor* actor = m_framework->GetIActorSystem()->GetActor(hit.targetId);
	IEntity* shooterEntity = gEnv->pEntitySystem->GetEntity(hit.shooterId);
	WB_LOG(2, "[WarsBots] Hit %s target=%u shooter=%u name=%s weapon=%u damage=%.3f health=%d botTarget=%d botShooter=%d.",
		phase, hit.targetId, hit.shooterId, shooterEntity ? shooterEntity->GetName() : "<none>", hit.weaponId,
		hit.damage, actor ? actor->GetHealth() : -1, target ? target->Number() : 0, shooter ? shooter->Number() : 0);
    if (actor && FireTrace()) {
        CNanoSuit* suit=CPlayer::GetNanoSuit(static_cast<CActor*>(actor));
        CGameRules* rules=g_pGame->GetGameRules();
        ISurfaceType* surface=rules?rules->GetHitMaterial(hit.material):nullptr;
        const char* material=surface?surface->GetName():nullptr;
        IItem* item=m_framework->GetIItemSystem()->GetItem(hit.weaponId);
        const float distance=shooterEntity?(shooterEntity->GetWorldPos()-actor->GetEntity()->GetWorldPos()).GetLength():-1;
        const bool human=actor->GetChannelId()>0 && m_framework->GetNetChannel(actor->GetChannelId());
        WB_LOG(2,"[WarsBots] FIRE HIT phase=%s target=%u shooter=%u projectile=%u weapon=%u class=%s actor=1 human=%d health=%d suit=%d energy=%.2f invulnerable=%d distance=%.2f material=%d head=%d part=%d damage=%.3f.",
            phase,hit.targetId,hit.shooterId,hit.projectileId,hit.weaponId,item?item->GetEntity()->GetClass()->GetName():"unknown",
            human?1:0,actor->GetHealth(),suit?int(suit->GetMode()):-1,suit?suit->GetSuitEnergy():-1,
            suit && suit->IsInvulnerable()?1:0,distance,hit.material,material && std::strstr(material,"head")?1:0,hit.partId,hit.damage);
    }
}

void BotManager::TraceShot(EntityId weaponId, EntityId shooter, EntityId projectile)
{
    BotInstance* bot=Find(shooter);
    if (!bot || !FireTrace()) return;
    IItem* item=m_framework->GetIItemSystem()->GetItem(weaponId);
    IWeapon* weapon=item?item->GetIWeapon():nullptr;
    IFireMode* mode=weapon?weapon->GetFireMode(weapon->GetCurrentFireMode()):nullptr;
    CProjectile* bullet=g_pGame->GetWeaponSystem()->GetProjectile(projectile);
    WB_LOG(2,"[WarsBots] FIRE SHOT shooter=%u target=%u projectile=%u weapon=%u class=%s mode=%s rpm=%.2f clip=%d remote=%d.",
        shooter,bot->Brain().Target(),projectile,item?item->GetEntityId():0,
        item?item->GetEntity()->GetClass()->GetName():"unknown",mode?mode->GetType():"unknown",mode?mode->GetFireRate():0,mode?mode->GetAmmoCount():-1,bullet?int(bullet->IsRemote()):-1);
}

void BotManager::TraceKill(EntityId targetId, EntityId shooterId, bool killerEventEmitted)
{
	// Map memory learns from every kill on the server, human ones included.
	if (IEntity* victim = gEnv->pEntitySystem->GetEntity(targetId)) m_nav.ReportDeath(victim->GetWorldPos());
	if (shooterId && shooterId != targetId)
		if (IEntity* killer = gEnv->pEntitySystem->GetEntity(shooterId)) m_nav.ReportKill(killer->GetWorldPos());
	if (BotInstance* target = Find(targetId)) target->TraceKill(shooterId, killerEventEmitted);
	if (BotInstance* shooter = Find(shooterId))
		WB_LOG(1, "[WarsBots] #%d combat kill shooter=%u target=%u via stock GameRules::KillPlayer.",
			shooter->Number(), shooterId, targetId);
}

bool BotManager::CanRunLocalIA() const
{
	return CryMPEngineAdapter::LocalIAGate(m_framework, m_levelReady, m_localTestEnabled, false);
}

int BotManager::MaxBots() const
{
	return clamp_tpl(m_maxBots, 1, kHardLimit);
}

EntityId BotManager::ChooseSpawnLocation(EntityId self, EntityId avoid) const
{
	CGameRules* rules = g_pGame->GetGameRules();
	const int count = rules ? rules->GetSpawnLocationCount() : 0;
	if (!count) return 0;
	// Per-bot start offset so simultaneous requests fan out instead of racing for one point.
	const unsigned salt = unsigned(self) * 2654435761u + unsigned(gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64());
	const int start = int(salt % unsigned(count));
	for (int pass = 0; pass < 2; ++pass)
		for (int i = 0; i < count; ++i)
		{
			const EntityId candidate = rules->GetSpawnLocation((start + i) % count);
			// Native checks: swept body volume and proximity to every other Player entity (bots included).
			if ((!pass && candidate == avoid) ||
				!rules->IsSpawnLocationSafe(self, candidate, 6.0f, 0.0f) ||
				!rules->TestSpawnLocationWithEnvironment(candidate, self, 0.2f, 1.8f)) continue;
			return candidate;
		}
	return 0;
}

// ---------------------------------------------------------------- lifecycle

int BotManager::FreeNumber() const
{
	for (int n = 1; n <= kHardLimit; ++n)
		if (!ByNumber(n)) return n;
	return 0;
}

BotInstance* BotManager::ByNumber(int number) const
{
	for (const auto& entry : m_bots)
		if (entry.second->Number() == number) return entry.second.get();
	return nullptr;
}

std::vector<BotInstance*> BotManager::Ordered() const
{
	std::vector<BotInstance*> bots;
	bots.reserve(m_bots.size());
	for (const auto& entry : m_bots) bots.push_back(entry.second.get());
	std::sort(bots.begin(), bots.end(), [](BotInstance* a, BotInstance* b) { return a->Number() < b->Number(); });
	return bots;
}

BotInstance* BotManager::Selected() const
{
	if (BotInstance* bot = ByNumber(m_selected)) return bot;
	std::vector<BotInstance*> bots = Ordered();
	return bots.empty() ? nullptr : bots.front();
}

BotInstance* BotManager::Add(const Vec3* position, BotSkill skill, const BotStyle* style, bool startAI)
{
	if (!CryMPEngineAdapter::LocalIAGate(m_framework, m_levelReady, m_localTestEnabled, true))
		return nullptr;
	if (Count() >= MaxBots())
	{
		WB_LOG(0, "[WarsBots] Add refused: %d/%d bots (raise wb_maxbots, hard limit %d).", Count(), MaxBots(), kHardLimit);
		return nullptr;
	}
	const int number = FreeNumber();
	if (!number) return nullptr;
	if (!position && !ChooseSpawnLocation(0, 0))
	{
		WB_LOG(0, "[WarsBots] Add deferred: no safe IA spawn point free of other players; try again or pass x y z.");
		return nullptr;
	}
	// Default rotation through the six styles so a full roster never moves in lockstep.
	const BotStyle chosenStyle = style ? *style : BotStyle((number - 1) % int(BotStyle::Count));
	return AddWith(position, BotProfile::Make(chosenStyle, skill), number, startAI);
}

BotInstance* BotManager::AddWith(const Vec3* position, const BotProfile& profile, int number, bool startAI)
{
	if (!number || ByNumber(number) || Count() >= MaxBots()) return nullptr;
	Vec3 where(0,0,0);
	if (position) where = *position;
	else
	{
		const EntityId spawnId = ChooseSpawnLocation(0, 0);
		IEntity* spawn = spawnId ? gEnv->pEntitySystem->GetEntity(spawnId) : nullptr;
		if (!spawn) return nullptr;
		where = spawn->GetWorldPos() + Vec3(0,0,0.2f);
	}
	std::unique_ptr<BotInstance> bot(new BotInstance(*this, number, profile));
	const bool spawned = bot->Spawn(where);
	m_spawning = nullptr;
	if (!spawned || bot->Expired())
		return nullptr;
	BotInstance* raw = bot.get();
	raw->Brain().SetVerbose(m_defaultVerbose);
	m_bots[raw->GetEntityId()] = std::move(bot);
	if (startAI && !raw->Brain().Start(*raw))
		WB_LOG(0, "[WarsBots] #%d created but AI start refused.", raw->Number());
	return raw;
}

void BotManager::RemoveInstance(BotInstance& instance)
{
	instance.Remove();
	// If the engine deferred removal the slot stays (brain stopped) until ENTITY_EVENT_DONE + Sweep.
	if (instance.Expired())
		for (auto it = m_bots.begin(); it != m_bots.end(); ++it)
			if (it->second.get() == &instance) { m_bots.erase(it); break; }
}

void BotManager::RemoveAll(const char* reason)
{
	if (m_bots.empty()) return;
	WB_LOG(0, "[WarsBots] Removing all %d bots (%s).", Count(), reason);
	for (BotInstance* bot : Ordered()) bot->Remove();
	Sweep();
	// Anything left is pending engine removal; it can no longer act (brain stopped).
	if (!m_bots.empty())
		WB_LOG(0, "[WarsBots] %d bot slots await ENTITY_EVENT_DONE.", Count());
}

void BotManager::Sweep()
{
	for (auto it = m_bots.begin(); it != m_bots.end();)
		if (it->second->Expired()) it = m_bots.erase(it); else ++it;
}

void BotManager::EndLevel()
{
	m_levelReady = false;
	for (BotInstance* bot : Ordered()) bot->Remove();
	// After a level ends no entity can still be ours: drop every slot unconditionally.
	m_bots.clear();
	m_spawning = nullptr;
}

void BotManager::OnEvent(IEntity* entity, SEntityEvent& event)
{
	if (event.event != ENTITY_EVENT_DONE || !entity) return;
	// Do not erase here: this can arrive from inside RemoveEntity called by our own Remove().
	BotInstance* bot = Find(entity->GetId());
	if (!bot) return;
	// Removed by someone else while the level lives: native IA RestartGame -> GameFramework::Reset
	// deletes runtime-spawned actors without eAE_resetBegin. Re-create the same bot next frame.
	if (!bot->Removing() && m_levelReady)
	{
		m_roster.push_back({ bot->Number(), bot->Profile(), bot->AIEnabled() });
		m_restorePending = true;
		WB_LOG(0, "[WarsBots] #%d removed externally (game reset?); will re-create.", bot->Number());
	}
	bot->OnEntityDone();
}

void BotManager::OnPostUpdate(float dt)
{
	// Restore outside the action-event callback, once the reset level is usable again.
	if (m_restorePending && m_levelReady && CanRunLocalIA()) { Sweep(); RestoreRoster(); }
	if (m_bots.empty()) return;
	// Navigation graph is built lazily once bots exist, a few ms per frame; geometry survives
	// IA game resets, so only level changes discard it.
	if (m_levelReady && CanRunLocalIA()) { m_nav.Begin(); m_nav.Step(2.0f); m_nav.Tick(dt); }
	m_clock += dt;
	// Stable order by display number; vector copy keeps iteration valid if a callback sweeps.
	for (BotInstance* bot : Ordered()) bot->Update(dt);
	Sweep();
	if (m_defaultVerbose && m_clock >= m_nextProfileLog)
	{
		StatusReport();
		m_nextProfileLog = m_clock + 10.0f;
	}
}

void BotManager::OnLevelNotFound(const char*) { m_restorePending = false; m_roster.clear(); EndLevel(); m_nav.Reset(); }
void BotManager::OnLoadingStart(ILevelInfo*) { m_restorePending = false; m_roster.clear(); EndLevel(); m_nav.Reset(); }
void BotManager::OnLoadingComplete(ILevel*) { m_levelReady = true; g_fireBudget=20000; }
void BotManager::OnLoadingError(ILevelInfo*, const char*) { EndLevel(); m_nav.Reset(); }
void BotManager::OnLoadGame(ILoadGame*) { EndLevel(); m_nav.Reset(); }
void BotManager::OnLevelEnd(const char*) { EndLevel(); m_nav.Reset(); }

void BotManager::OnActionEvent(const SActionEvent& event)
{
	if (event.m_event == eAE_resetBegin)
	{
		// Native IA restart (e.g. PreGame start timer). Remember who existed, then clean up.
		m_roster.clear();
		for (BotInstance* bot : Ordered())
			m_roster.push_back({ bot->Number(), bot->Profile(), bot->AIEnabled() });
		if (!m_roster.empty())
			WB_LOG(0, "[WarsBots] Game reset: removing %d bots, will restore after reset.", int(m_roster.size()));
		EndLevel();
		m_restorePending = !m_roster.empty();
	}
	else if (event.m_event == eAE_disconnected)
	{
		m_nav.Reset();
		m_roster.clear();
		m_restorePending = false;
		EndLevel();
	}
	else if (event.m_event == eAE_resetEnd && m_framework)
		m_levelReady = m_framework->GetILevelSystem()->IsLevelLoaded();
}

void BotManager::RestoreRoster()
{
	m_restorePending = false;
	int restored = 0;
	for (const RosterEntry& entry : m_roster)
		if (AddWith(nullptr, entry.profile, entry.number, entry.ai)) ++restored;
	WB_LOG(0, "[WarsBots] Game reset: restored %d/%d bots.", restored, int(m_roster.size()));
	m_roster.clear();
}

// ---------------------------------------------------------------- reporting

void BotManager::List() const
{
	WB_LOG(0, "[WarsBots] %d/%d bots (selected #%d):", Count(), MaxBots(), Selected() ? Selected()->Number() : 0);
	for (BotInstance* bot : Ordered())
	{
		CActor* actor = bot->GetActor();
		WB_LOG(0, "[WarsBots]  #%d EntityId=%u %s %s/%s ai=%d state=%s health=%d target=%u.",
			bot->Number(), bot->GetEntityId(), bot->Name(), BotProfile::StyleName(bot->Profile().style),
			BotProfile::SkillName(bot->Profile().skill), bot->AIEnabled() ? 1 : 0, bot->Brain().StateName(),
			actor ? actor->GetHealth() : -1, bot->Brain().Target());
	}
}

void BotManager::StatusReport()
{
	float total = 0, worst = 0;
	for (BotInstance* bot : Ordered())
	{
		if (bot->UpdateCount()) total += bot->TotalUpdateMs() / float(bot->UpdateCount());
		worst = max(worst, bot->MaxUpdateMs());
	}
	WB_LOG(0, "[WarsBots] PROFILE bots=%d avgSum=%.3fms/frame worstFrame=%.3fms rays=%u targetScans=%u progressExperiment=%d (since last report).",
		Count(), total, worst, BotCombat::TakeRayCount(), BotCombat::TakeScanCount(), g_progressExperiment);
	for (BotInstance* bot : Ordered()) bot->ResetProfile();
}

// ---------------------------------------------------------------- commands

void BotManager::WatchCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();
    CGameRules* rules=g_pGame ? g_pGame->GetGameRules() : nullptr;
    CPlayer* viewer=self && self->m_framework ? CPlayer::FromIActor(self->m_framework->GetClientActor()) : nullptr;
    const bool ia=rules && !std::strcmp(rules->GetEntity()->GetClass()->GetName(),"InstantAction");
    if (!BotSpectator::CanInspect(gEnv->bClient,ia,viewer && viewer->GetSpectatorMode()!=CActor::eASM_None))
    {
        WB_LOG(0,"[WarsBots] WATCH refused: enter native spectator mode on InstantAction first.");
        return;
    }
    std::vector<EntityId> ids;
    IActorIteratorPtr it=self->m_framework->GetIActorSystem()->CreateActorIterator();
    while (IActor* actor=it->Next())
    {
        CPlayer* player=CPlayer::FromIActor(actor);
        if (player && BotSpectator::Eligible(player->IsWarsBotActor(),player->GetHealth()>0,
            player->GetEntity()->IsHidden(),player->GetSpectatorMode()!=CActor::eASM_None))
            ids.push_back(player->GetEntityId());
    }
    std::sort(ids.begin(),ids.end(),[&](EntityId a,EntityId b) {
        return std::strcmp(gEnv->pEntitySystem->GetEntity(a)->GetName(),gEnv->pEntitySystem->GetEntity(b)->GetName())<0;
    });
    const char* op=args->GetArgCount()>1 ? args->GetArg(1) : "status";
    if (!std::strcmp(op,"off"))
    {
        rules->ChangeSpectatorMode(viewer,CActor::eASM_Free,0,false);
        return;
    }
    if (!std::strcmp(op,"list"))
    {
        for (EntityId id:ids) WB_LOG(0,"[WarsBots] WATCH %s EntityId=%u",gEnv->pEntitySystem->GetEntity(id)->GetName(),id);
        WB_LOG(0,"[WarsBots] WATCH live replicated bots=%d",int(ids.size()));
        return;
    }
    if (!std::strcmp(op,"status"))
    {
        CPlayer* target=CPlayer::FromIActor(self->m_framework->GetIActorSystem()->GetActor(viewer->GetSpectatorTarget()));
        if (viewer->GetSpectatorMode()!=CActor::eASM_Follow || !target || !target->IsWarsBotActor())
        { WB_LOG(0,"[WarsBots] WATCH no followed WarsBot."); return; }
        IItem* item=target->GetCurrentItem();
        CNanoSuit* suit=target->GetNanoSuit();
        WB_LOG(0,"[WarsBots] WATCH %s EntityId=%u FP=%d spectatorHealth=%d weapon=%s suit=%d energy=%.1f (native replicated state; -1 unknown)",
            target->GetEntity()->GetName(),target->GetEntityId(),viewer->IsFpSpectator()?1:0,viewer->GetSpectatorHealth(),
            item ? item->GetEntity()->GetClass()->GetName() : "none",suit ? int(suit->GetMode()) : -1,suit ? suit->GetSuitEnergy() : -1.f);
        return;
    }
    EntityId target=0;
    if (!std::strcmp(op,"next") || !std::strcmp(op,"prev"))
        target=BotSpectator::Cycle(ids,viewer->GetSpectatorTarget(),!std::strcmp(op,"prev") ? -1 : 1);
    else
    {
        int number=0;
        if (!ParseInt(op,number) || number<1 || number>kHardLimit)
        { WB_LOG(0,"[WarsBots] Usage: wb_watch list|1..16|next|prev|status|off"); return; }
        char name[32];std::snprintf(name,sizeof(name),"WarsBot_%02d",number);
        for (EntityId id:ids) if (!std::strcmp(gEnv->pEntitySystem->GetEntity(id)->GetName(),name)) target=id;
    }
    if (!target) { WB_LOG(0,"[WarsBots] WATCH no eligible live bot."); return; }
    rules->ChangeSpectatorMode(viewer,CActor::eASM_Follow,target,false); // Existing validated real-observer RMI.
    WB_LOG(0,"[WarsBots] WATCH requested %s EntityId=%u; native F1 toggles FP.",gEnv->pEntitySystem->GetEntity(target)->GetName(),target);
}

void BotManager::WatchInfoCommand(IConsoleCmdArgs*)
{
    BotManager* self=Self();
    if (!self || !gEnv->bServer || !self->CanRunLocalIA())
    { WB_LOG(0,"[WarsBots] WATCH INFO refused: local IA server only."); return; }
    CGameRules::TPlayers players;g_pGame->GetGameRules()->GetPlayers(players);
    unsigned count=0;
    for (EntityId id:players)
    {
        CPlayer* viewer=CPlayer::FromIActor(self->m_framework->GetIActorSystem()->GetActor(id));
        if (!viewer || !viewer->GetChannelId() || !self->m_framework->GetNetChannel(viewer->GetChannelId()) ||
            viewer->GetSpectatorMode()!=CActor::eASM_Follow) continue;
        if (BotInstance* bot=self->Find(viewer->GetSpectatorTarget()))
        {
            WB_LOG(0,"[WarsBots] WATCH INFO observer=%u follows #%d",id,bot->Number());
            bot->Status();bot->Brain().Status();++count;
        }
    }
    WB_LOG(0,"[WarsBots] WATCH INFO real observers following owned bots=%u (server console only)",count);
}

void BotManager::WatchCheckCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();
    CGameRules* rules=g_pGame ? g_pGame->GetGameRules() : nullptr;
    CGameRules::TPlayers humans;
    if (rules) rules->GetPlayers(humans);
    if (!self || !rules || !gEnv->bServer || !self->CanRunLocalIA() || !humans.empty())
    { WB_LOG(0,"[WarsBots] WATCH CHECK refused: local IA server without real clients required."); return; }
    if (args && args->GetArgCount()>1 && !std::strcmp(args->GetArg(1),"after"))
    {
        // Query again after deferred entity removal has completed. Callback rechecks the gate/clients.
        const char* script="Script.SetTimer(500,function() System.ExecuteCommand('wb_watch_check') end)";
        gEnv->pScriptSystem->ExecuteBuffer(script,std::strlen(script),"WarsBots deferred spectator query");
        return;
    }
    CGameRules::TPlayers players;rules->GetMatchParticipants(players);
    unsigned checks=0;bool ok=true;
    for (EntityId query:players)
    {
        std::vector<EntityId> eligible;
        for (EntityId id:players)
        {
            CActor* actor=static_cast<CActor*>(self->m_framework->GetIActorSystem()->GetActor(id));
            if (id!=query && actor && actor->GetHealth()>0 && actor->GetSpectatorMode()==CActor::eASM_None) eligible.push_back(id);
        }
        const EntityId next=eligible.empty()?0:eligible.front(), prev=eligible.empty()?0:eligible.back();
        char script[1024];
        // Query only: native Lua entity IDs are handles, not ordinary numbers.
        // Do not turn a channel-0 actor into an observer or invoke a spectator RMI.
        std::snprintf(script,sizeof(script),
            "local q=System.GetEntity(%u).id; local n=System.GetEntity(g_gameRules.game:GetNextSpectatorTarget(q,1)); "
            "local p=System.GetEntity(g_gameRules.game:GetNextSpectatorTarget(q,-1)); "
            "assert(n==System.GetEntity(%u)); assert(p==System.GetEntity(%u))",query,next,prev);
        ok=gEnv->pScriptSystem->ExecuteBuffer(script,std::strlen(script),"WarsBots native spectator QA") && ok;
        checks+=2;
    }
    if (players.empty())
    {
        const char* script="assert((g_gameRules.game:GetNextSpectatorTarget(NULL_ENTITY,1) or 0)==0)";
        ok=gEnv->pScriptSystem->ExecuteBuffer(script,std::strlen(script),"WarsBots empty spectator QA");++checks;
    }
    WB_LOG(0,"[WarsBots] WATCH CHECK %s participants=%d assertions=%u; query-only, no observer/channel created",ok?"PASS":"FAIL",int(players.size()),checks);
}

void BotManager::AddCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!self || !args || !gEnv->bServer) return;
	int count = 1;
	BotSkill skill = BotSkill(clamp_tpl(self->m_skill, 0, int(BotSkill::Count) - 1));
	BotStyle style = BotStyle::Universal;
	const bool styleGiven = args->GetArgCount() >= 4;
	if ((args->GetArgCount() >= 2 && (!ParseInt(args->GetArg(1), count) || count < 1 || count > kHardLimit)) ||
		(args->GetArgCount() >= 3 && !BotProfile::ParseSkill(args->GetArg(2), skill)) ||
		(styleGiven && !BotProfile::ParseStyle(args->GetArg(3), style)) ||
		args->GetArgCount() > 4)
	{
		WB_LOG(0, "[WarsBots] Usage: wb_add [1..%d] [beginner|normal|veteran|elite] [universal|assault|marksman|flanker|defender|hunter].", kHardLimit);
		return;
	}
	int added = 0;
	for (int i = 0; i < count; ++i)
		if (self->Add(nullptr, skill, styleGiven ? &style : nullptr, true)) ++added; else break;
	WB_LOG(0, "[WarsBots] wb_add: %d/%d added, now %d/%d.", added, count, self->Count(), self->MaxBots());
}

void BotManager::RemoveCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	int number = 0;
	if (!self || !args || !gEnv->bServer || args->GetArgCount() != 2 || !ParseInt(args->GetArg(1), number))
	{ WB_LOG(0, "[WarsBots] Usage: wb_remove <botNumber> (see wb_list)."); return; }
	BotInstance* bot = self->ByNumber(number);
	if (!bot) { WB_LOG(0, "[WarsBots] wb_remove: no bot #%d.", number); return; }
	self->RemoveInstance(*bot);
}

void BotManager::RemoveAllCommand(IConsoleCmdArgs*)
{
	if (BotManager* self = Self()) if (gEnv->bServer) self->RemoveAll("wb_remove_all");
}

void BotManager::ListCommand(IConsoleCmdArgs*) { if (BotManager* self = Self()) self->List(); }

void BotManager::StatusCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (!self) return;
	WB_LOG(0, "[WarsBots] gate=%d levelReady=%d bots=%d/%d defaultVerbose=%d.",
		self->CanRunLocalIA() ? 1 : 0, self->m_levelReady ? 1 : 0, self->Count(), self->MaxBots(), self->m_defaultVerbose ? 1 : 0);
	for (BotInstance* bot : self->Ordered()) { bot->Brain().Status(); bot->Status(); }
	self->m_nav.Status();
	self->StatusReport();
}

void BotManager::DebugCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!self || !args || args->GetArgCount() != 2 || (strcmp(args->GetArg(1), "0") && strcmp(args->GetArg(1), "1")))
	{ WB_LOG(0, "[WarsBots] Usage: wb_debug 0|1."); return; }
	self->m_defaultVerbose = !strcmp(args->GetArg(1), "1");
	for (BotInstance* bot : self->Ordered()) bot->Brain().SetVerbose(self->m_defaultVerbose);
}

void BotManager::TeleportCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self(); int number=0; Vec3 position(0,0,0);
    if(!self || !self->CanRunLocalIA()) {WB_LOG(0,"[WarsBots] DEBUG TELEPORT refused: local IA gate closed");return;}
    if(!args || args->GetArgCount()!=5 || !ParseInt(args->GetArg(1),number) || !ParsePosition(args,2,position))
    {WB_LOG(0,"[WarsBots] Usage: wb_teleport <botNumber> x y z");return;}
    BotInstance* bot=self->ByNumber(number);
    if(!bot || !bot->DebugTeleport(position))WB_LOG(0,"[WarsBots] DEBUG TELEPORT refused: owned live unfrozen bot without vehicle required");
}

void BotManager::ReviveAtCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();int spawnNumber=0,number=0;
    if(!self || !self->CanRunLocalIA()) {WB_LOG(0,"[WarsBots] DEBUG REVIVE refused: local IA gate closed");return;}
    if(!args || (args->GetArgCount()!=2 && args->GetArgCount()!=3) || !ParseInt(args->GetArg(1),spawnNumber) || spawnNumber<=0)
    {WB_LOG(0,"[WarsBots] Usage: wb_revive_at <spawnEntityId> [botNumber|me]");return;}
    CGameRules* rules=g_pGame->GetGameRules();bool registered=false;
    for(int i=0;i<rules->GetSpawnLocationCount();++i)registered |= rules->GetSpawnLocation(i)==EntityId(spawnNumber);
    IEntity* spawn=registered?gEnv->pEntitySystem->GetEntity(EntityId(spawnNumber)):nullptr;
    if(!spawn){WB_LOG(0,"[WarsBots] DEBUG REVIVE refused: spawnId=%d not registered",spawnNumber);return;}
    BotInstance* bot=nullptr;CActor* actor=nullptr;
    if(args->GetArgCount()==3 && !std::strcmp(args->GetArg(2),"me")) {
        actor=static_cast<CActor*>(self->m_framework->GetClientActor());
        if(!actor) { // Dedicated console has no caller: accept only one unambiguous real player.
            CGameRules::TPlayers players;rules->GetPlayers(players);
            for(EntityId id:players) {
                CActor* candidate=static_cast<CActor*>(self->m_framework->GetIActorSystem()->GetActor(id));
                if(candidate && candidate->GetChannelId()>0) {
                    if(actor){WB_LOG(0,"[WarsBots] DEBUG REVIVE refused: me ambiguous on dedicated server");return;}
                    actor=candidate;
                }
            }
        }
        if(actor && (self->Find(actor->GetEntityId()) || actor->GetChannelId()<=0))actor=nullptr;
    } else {
        if(args->GetArgCount()==2)bot=self->Selected();
        else if(ParseInt(args->GetArg(2),number))bot=self->ByNumber(number);
        actor=bot?bot->GetActor():nullptr;
    }
    if(!actor || !actor->GetInventory() || actor->GetLinkedVehicle() || rules->IsFrozen(actor->GetEntityId()) ||
        actor->GetSpectatorMode()!=CActor::eASM_None || (bot && !bot->Owns(actor->GetEntityId())))
    {WB_LOG(0,"[WarsBots] DEBUG REVIVE refused: non-spectator actor with inventory, no vehicle/freeze required");return;}
    const EntityId id=actor->GetEntityId();
    const Vec3 position=spawn->GetWorldPos()+Vec3(0,0,0.2f); const Ang3 angles(spawn->GetWorldRotation());
    const bool running=bot && bot->AIEnabled();
    if(running)bot->Brain().Stop(*bot);
    WB_LOG(0,"[WarsBots] DEBUG REVIVE actor=%u bot=%d spawnId=%d manual=1 nativeKill=%d",id,bot?bot->Number():0,spawnNumber,actor->GetHealth()>0?1:0);
    if(actor->GetHealth()>0)rules->KillPlayer(actor,false,true,id,0,float(actor->GetHealth()),0,0,Vec3(0,0,0));
    // Kill callbacks may remove an entity; never use a stale actor pointer.
    CActor* current=static_cast<CActor*>(self->m_framework->GetIActorSystem()->GetActor(id));
    if(current && current==actor && current->GetHealth()<=0) {
        if(bot)bot->Revive(EntityId(spawnNumber));
        else {
            rules->RevivePlayer(current,position,angles,rules->GetTeam(id),true);
            BotMatch::EquipLikePlayer(*rules,*current);
            WB_LOG(0,"[WarsBots] DEBUG REVIVE human actor=%u spawnId=%d health=%d",id,spawnNumber,current->GetHealth());
        }
    }
    if(running && bot->Owns(id))bot->Brain().Start(*bot);
}

void BotManager::NavExplainCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();
    if(!self || !gEnv->bServer || !self->CanRunLocalIA())
    {WB_LOG(0,"[WarsBots] NAV explain refused: local IA server required");return;}
    int from=-1,to=-1,delay=0;
    if(args->GetArgCount()==5 && !std::strcmp(args->GetArg(1),"after") && ParseInt(args->GetArg(2),delay) && delay>=1 && delay<=30 &&
        ParseInt(args->GetArg(3),from) && ParseInt(args->GetArg(4),to) && from>=0 && to>=0)
    {
        char script[192];std::snprintf(script,sizeof(script),"Script.SetTimer(%d,function() System.ExecuteCommand('wb_nav_explain %d %d') end)",delay*1000,from,to);
        gEnv->pScriptSystem->ExecuteBuffer(script,std::strlen(script),"WarsBots delayed edge explanation");return;
    }
    if(args->GetArgCount()!=3 || !ParseInt(args->GetArg(1),from) || !ParseInt(args->GetArg(2),to))
    {WB_LOG(0,"[WarsBots] Usage: wb_nav_explain <fromNode> <toNode>");return;}
    self->m_nav.Explain(from,to);
}

void BotManager::NavExportCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();
    if (!self || !gEnv->bServer || !self->CanRunLocalIA())
    { WB_LOG(0,"[WarsBots] NAV export refused: local IA server required");return; }
    if (args->GetArgCount()==3 && !std::strcmp(args->GetArg(1),"after"))
    {
        int seconds=0;
        if (!ParseInt(args->GetArg(2),seconds) || seconds<1 || seconds>30)
        { WB_LOG(0,"[WarsBots] NAV export delay must be 1..30 seconds");return; }
        char script[128];std::snprintf(script,sizeof(script),"Script.SetTimer(%d,function() System.ExecuteCommand('wb_nav_export') end)",seconds*1000);
        gEnv->pScriptSystem->ExecuteBuffer(script,std::strlen(script),"WarsBots delayed graph export");return;
    }
    if (args->GetArgCount()>2) { WB_LOG(0,"[WarsBots] Usage: wb_nav_export [path.json] or after <seconds>");return; }
    self->m_nav.Export(args->GetArgCount()==2?args->GetArg(1):nullptr);
}

void BotManager::NavCommand(IConsoleCmdArgs*)
{
	if (BotManager* self = Self()) self->m_nav.Status();
}

void BotManager::SelectCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	int number = 0;
	if (!self || !args || args->GetArgCount() != 2 || !ParseInt(args->GetArg(1), number) || !self->ByNumber(number))
	{ WB_LOG(0, "[WarsBots] Usage: wb_select <botNumber> (see wb_list)."); return; }
	self->m_selected = number;
	WB_LOG(0, "[WarsBots] Selected bot #%d.", number);
}

void BotManager::ProbeSpawnCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	Vec3 position(0,0,0);
	if (!self || !args || args->GetArgCount() != 4 || !ParsePosition(args, 1, position))
	{ WB_LOG(0, "[WarsBots] Usage: wb_probe_spawn x y z (finite world coordinates)."); return; }
    const BotStyle qaStyle=BotStyle::Universal;
	if (BotInstance* bot = self->Add(&position, BotSkill(clamp_tpl(self->m_skill, 0, 3)), g_qaSocomOnly==1?&qaStyle:nullptr, false))
		self->m_selected = bot->Number();
}

void BotManager::ProbeRemoveCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (self && gEnv->bServer) if (BotInstance* bot = self->Selected()) self->RemoveInstance(*bot);
}

void BotManager::ProbeStatusCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (!self) return;
	if (BotInstance* bot = self->Selected()) bot->Status(); else WB_LOG(0, "[WarsBots] No live owned bot.");
}

void BotManager::ProbeReviveCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (self) if (BotInstance* bot = self->Selected()) bot->Revive();
}

void BotManager::BotStartCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!self || !args) return;
	Vec3 position(0,0,0);
	const bool hasPosition = args->GetArgCount() == 4 && ParsePosition(args, 1, position);
	if (args->GetArgCount() != 1 && !hasPosition) { WB_LOG(0, "[WarsBots] Usage: wb_bot_start [x y z]."); return; }
	BotInstance* bot = self->Selected();
	if (!bot)
	{
		bot = self->Add(hasPosition ? &position : nullptr, BotSkill(clamp_tpl(self->m_skill, 0, 3)), nullptr, false);
		if (!bot) return;
		self->m_selected = bot->Number();
	}
	bot->Brain().Start(*bot);
}

void BotManager::BotStopCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (self && gEnv->bServer) if (BotInstance* bot = self->Selected()) bot->Brain().Stop(*bot);
}

void BotManager::BotStatusCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (!self) return;
	if (BotInstance* bot = self->Selected()) { bot->Brain().Status(); bot->Status(); }
	else WB_LOG(0, "[WarsBots] No bot; wb_add or wb_bot_start creates one.");
}

void BotManager::BotVerboseCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!self || !args || args->GetArgCount() != 2 || (strcmp(args->GetArg(1), "0") && strcmp(args->GetArg(1), "1")))
	{ WB_LOG(0, "[WarsBots] Usage: wb_bot_verbose 0|1."); return; }
	const bool verbose = !strcmp(args->GetArg(1), "1");
	self->m_defaultVerbose = verbose; // Also applies to bots created later (QA starts with this flag).
	if (BotInstance* bot = self->Selected()) bot->Brain().SetVerbose(verbose);
}

void BotManager::BotAbilityCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!self || !args) return;
	BotInstance* bot = self->Selected();
	if (!bot || !self->CanRunLocalIA() || !bot->GetActor() || !bot->AIEnabled())
	{ WB_LOG(0, "[WarsBots] ACTION REFUSED: abilities require a running selected bot and open local IA gate."); return; }
	bot->Brain().AbilityCommand(*bot, args);
}
}
