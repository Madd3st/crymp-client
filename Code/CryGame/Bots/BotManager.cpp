// WarsBots project addition. See LICENSE.txt for SDK usage terms.
#include "PortIncludes.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Bots/BotCombat.h"
#include "CryGame/Bots/CryMPEngineAdapter.h"
#include "CryGame/Game.h"
#include "CryGame/GameRules.h"
#include "CryGame/Actors/Actor.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace WarsBots
{
namespace
{
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
	"wb_add", "wb_remove", "wb_remove_all", "wb_list", "wb_status", "wb_debug", "wb_select",
	"wb_probe_spawn", "wb_probe_remove", "wb_probe_status", "wb_probe_revive",
	"wb_bot_start", "wb_bot_stop", "wb_bot_status", "wb_bot_verbose",
	"wb_bot_jump", "wb_bot_gait", "wb_bot_strafe", "wb_bot_suit", "wb_bot_abilities",
};
const char* kVariables[] = { "wb_probe_local_test", "wb_bot_cloak_experimental", "wb_maxbots", "wb_skill" };
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
	console->Register("wb_maxbots", &m_maxBots, 6, 0, "Maximum simultaneous WarsBots (1..16, default 6)");
	console->Register("wb_skill", &m_skill, int(BotSkill::Normal), 0,
		"Default skill for wb_add: 0 beginner, 1 normal, 2 veteran, 3 elite");
	console->AddCommand("wb_add", AddCommand, 0, "wb_add [count] [beginner|normal|veteran|elite] [style]: add autonomous bots");
	console->AddCommand("wb_remove", RemoveCommand, 0, "wb_remove <botNumber>: remove one bot (number from wb_list, not EntityId)");
	console->AddCommand("wb_remove_all", RemoveAllCommand, 0, "Remove every WarsBot");
	console->AddCommand("wb_list", ListCommand, 0, "List bots: number, EntityId, profile, state");
	console->AddCommand("wb_status", StatusCommand, 0, "Detailed status and per-bot update cost");
	console->AddCommand("wb_debug", DebugCommand, 0, "wb_debug 0|1: verbose telemetry for all bots");
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
	CryLogAlways("[WarsBots] BotManager initialized: registry by EntityId, limit wb_maxbots=%d (hard %d); gate wb_probe_local_test=0; no automatic spawn.",
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
	gEnv->pEntitySystem->RemoveSink(this);
	m_framework->GetILevelSystem()->RemoveListener(this);
	m_framework->UnregisterListener(this);
	m_framework = nullptr;
	if (gEnv && gEnv->pLog)
		CryLogAlways("[WarsBots] BotManager shut down.");
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
	CryLogAlways("[WarsBots] Hit %s target=%u shooter=%u name=%s weapon=%u damage=%.3f health=%d botTarget=%d botShooter=%d.",
		phase, hit.targetId, hit.shooterId, shooterEntity ? shooterEntity->GetName() : "<none>", hit.weaponId,
		hit.damage, actor ? actor->GetHealth() : -1, target ? target->Number() : 0, shooter ? shooter->Number() : 0);
}

void BotManager::TraceKill(EntityId targetId, EntityId shooterId, bool killerEventEmitted)
{
	if (BotInstance* target = Find(targetId)) target->TraceKill(shooterId, killerEventEmitted);
	if (BotInstance* shooter = Find(shooterId))
		CryLogAlways("[WarsBots] #%d combat kill shooter=%u target=%u via stock GameRules::KillPlayer.",
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
		CryLogAlways("[WarsBots] Add refused: %d/%d bots (raise wb_maxbots, hard limit %d).", Count(), MaxBots(), kHardLimit);
		return nullptr;
	}
	const int number = FreeNumber();
	if (!number) return nullptr;
	if (!position && !ChooseSpawnLocation(0, 0))
	{
		CryLogAlways("[WarsBots] Add deferred: no safe IA spawn point free of other players; try again or pass x y z.");
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
		CryLogAlways("[WarsBots] #%d created but AI start refused.", raw->Number());
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
	CryLogAlways("[WarsBots] Removing all %d bots (%s).", Count(), reason);
	for (BotInstance* bot : Ordered()) bot->Remove();
	Sweep();
	// Anything left is pending engine removal; it can no longer act (brain stopped).
	if (!m_bots.empty())
		CryLogAlways("[WarsBots] %d bot slots await ENTITY_EVENT_DONE.", Count());
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
		CryLogAlways("[WarsBots] #%d removed externally (game reset?); will re-create.", bot->Number());
	}
	bot->OnEntityDone();
}

void BotManager::OnPostUpdate(float dt)
{
	// Restore outside the action-event callback, once the reset level is usable again.
	if (m_restorePending && m_levelReady && CanRunLocalIA()) { Sweep(); RestoreRoster(); }
	if (m_bots.empty()) return;
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

void BotManager::OnLevelNotFound(const char*) { m_restorePending = false; m_roster.clear(); EndLevel(); }
void BotManager::OnLoadingStart(ILevelInfo*) { m_restorePending = false; m_roster.clear(); EndLevel(); }
void BotManager::OnLoadingComplete(ILevel*) { m_levelReady = true; }
void BotManager::OnLoadingError(ILevelInfo*, const char*) { EndLevel(); }
void BotManager::OnLoadGame(ILoadGame*) { EndLevel(); }
void BotManager::OnLevelEnd(const char*) { EndLevel(); }

void BotManager::OnActionEvent(const SActionEvent& event)
{
	if (event.m_event == eAE_resetBegin)
	{
		// Native IA restart (e.g. PreGame start timer). Remember who existed, then clean up.
		m_roster.clear();
		for (BotInstance* bot : Ordered())
			m_roster.push_back({ bot->Number(), bot->Profile(), bot->AIEnabled() });
		if (!m_roster.empty())
			CryLogAlways("[WarsBots] Game reset: removing %d bots, will restore after reset.", int(m_roster.size()));
		EndLevel();
		m_restorePending = !m_roster.empty();
	}
	else if (event.m_event == eAE_disconnected)
	{
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
	CryLogAlways("[WarsBots] Game reset: restored %d/%d bots.", restored, int(m_roster.size()));
	m_roster.clear();
}

// ---------------------------------------------------------------- reporting

void BotManager::List() const
{
	CryLogAlways("[WarsBots] %d/%d bots (selected #%d):", Count(), MaxBots(), Selected() ? Selected()->Number() : 0);
	for (BotInstance* bot : Ordered())
	{
		CActor* actor = bot->GetActor();
		CryLogAlways("[WarsBots]  #%d EntityId=%u %s %s/%s ai=%d state=%s health=%d target=%u.",
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
	CryLogAlways("[WarsBots] PROFILE bots=%d avgSum=%.3fms/frame worstFrame=%.3fms rays=%u targetScans=%u (since last report).",
		Count(), total, worst, BotCombat::TakeRayCount(), BotCombat::TakeScanCount());
	for (BotInstance* bot : Ordered()) bot->ResetProfile();
}

// ---------------------------------------------------------------- commands

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
		CryLogAlways("[WarsBots] Usage: wb_add [1..%d] [beginner|normal|veteran|elite] [universal|assault|marksman|flanker|defender|hunter].", kHardLimit);
		return;
	}
	int added = 0;
	for (int i = 0; i < count; ++i)
		if (self->Add(nullptr, skill, styleGiven ? &style : nullptr, true)) ++added; else break;
	CryLogAlways("[WarsBots] wb_add: %d/%d added, now %d/%d.", added, count, self->Count(), self->MaxBots());
}

void BotManager::RemoveCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	int number = 0;
	if (!self || !args || !gEnv->bServer || args->GetArgCount() != 2 || !ParseInt(args->GetArg(1), number))
	{ CryLogAlways("[WarsBots] Usage: wb_remove <botNumber> (see wb_list)."); return; }
	BotInstance* bot = self->ByNumber(number);
	if (!bot) { CryLogAlways("[WarsBots] wb_remove: no bot #%d.", number); return; }
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
	CryLogAlways("[WarsBots] gate=%d levelReady=%d bots=%d/%d defaultVerbose=%d.",
		self->CanRunLocalIA() ? 1 : 0, self->m_levelReady ? 1 : 0, self->Count(), self->MaxBots(), self->m_defaultVerbose ? 1 : 0);
	for (BotInstance* bot : self->Ordered()) { bot->Brain().Status(); bot->Status(); }
	self->StatusReport();
}

void BotManager::DebugCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!self || !args || args->GetArgCount() != 2 || (strcmp(args->GetArg(1), "0") && strcmp(args->GetArg(1), "1")))
	{ CryLogAlways("[WarsBots] Usage: wb_debug 0|1."); return; }
	self->m_defaultVerbose = !strcmp(args->GetArg(1), "1");
	for (BotInstance* bot : self->Ordered()) bot->Brain().SetVerbose(self->m_defaultVerbose);
}

void BotManager::SelectCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	int number = 0;
	if (!self || !args || args->GetArgCount() != 2 || !ParseInt(args->GetArg(1), number) || !self->ByNumber(number))
	{ CryLogAlways("[WarsBots] Usage: wb_select <botNumber> (see wb_list)."); return; }
	self->m_selected = number;
	CryLogAlways("[WarsBots] Selected bot #%d.", number);
}

void BotManager::ProbeSpawnCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	Vec3 position(0,0,0);
	if (!self || !args || args->GetArgCount() != 4 || !ParsePosition(args, 1, position))
	{ CryLogAlways("[WarsBots] Usage: wb_probe_spawn x y z (finite world coordinates)."); return; }
	if (BotInstance* bot = self->Add(&position, BotSkill(clamp_tpl(self->m_skill, 0, 3)), nullptr, false))
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
	if (BotInstance* bot = self->Selected()) bot->Status(); else CryLogAlways("[WarsBots] No live owned bot.");
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
	if (args->GetArgCount() != 1 && !hasPosition) { CryLogAlways("[WarsBots] Usage: wb_bot_start [x y z]."); return; }
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
	else CryLogAlways("[WarsBots] No bot; wb_add or wb_bot_start creates one.");
}

void BotManager::BotVerboseCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!self || !args || args->GetArgCount() != 2 || (strcmp(args->GetArg(1), "0") && strcmp(args->GetArg(1), "1")))
	{ CryLogAlways("[WarsBots] Usage: wb_bot_verbose 0|1."); return; }
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
	{ CryLogAlways("[WarsBots] ACTION REFUSED: abilities require a running selected bot and open local IA gate."); return; }
	bot->Brain().AbilityCommand(*bot, args);
}
}
