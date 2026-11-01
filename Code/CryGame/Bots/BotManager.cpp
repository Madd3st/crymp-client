// WarsBots project addition. See LICENSE.txt for SDK usage terms.
#include "PortIncludes.h"
#include "CryGame/Bots/BotLog.h"
#include "CryGame/Bots/BotManager.h"
#include "CryGame/Bots/BotConfig.h"
#include "CryGame/Bots/BotDataPath.h"
#include "CryGame/Bots/BotProbeRunner.h"
#include "CryGame/Bots/BotPSCaptureRuntime.h"
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include "CryGame/Bots/BotGeometryExport.h"
#include "version.h"
#include "CryGame/Bots/BotCombat.h"
#include "CryGame/Bots/BotDifficulty.h"
#include "CryGame/Bots/BotMatch.h"
#include "CryGame/Bots/BotRemovalPolicy.h"
#include "CryGame/Bots/BotTeamPlan.h"
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
#include <ctime>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace WarsBots
{
namespace
{
int g_fireBudget=20000;
bool FireTrace() {
    if (!BotConfig::Get().fireDiagnostics || !gEnv->bServer || g_fireBudget<=0) return false;
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
bool ProbeRunnerEnabled() { return BotConfig::Get().probeRunner == 1; }

bool ReadProbeFile(const char* path, std::string& text)
{
	if (!path || !*path) return false;
	std::ifstream in(path, std::ios::binary);
	if (!in) return false;
	text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return in.good() || in.eof();
}

// DS-047: the runner core emits setup verbs; only the ones the engine host implements are executed.
bool ProbeSetupSupported(const std::string& command)
{
	static const char* const unsupported[] = { "wb_probe_dummy", "wb_probe_burst", "wb_probe_peek", "wb_probe_cloak" };
	for (const char* name : unsupported)
	{
		const size_t n = std::strlen(name);
		if (command.compare(0, n, name) == 0 && (command.size() == n || command[n] == ' '))
			return false;
	}
	return true;
}

const char* kCommands[] = { "wb_demo_start", "wb_demo_stop", "wb_demo_mark", "wb_qa_time_scale", "wb_nav_audit",
	"wb_spawn_box", "wb_revive_at", "wb_teleport", "wb_probe_item", "wb_watch", "wb_watch_info", "wb_watch_check", "wb_geom_export", "wb_nav_export", "wb_nav_explain",
	"wb_add", "wb_remove", "wb_remove_all", "wb_kick_bots", "wb_fill", "wb_skill_all", "wb_mark", "wb_rules_info", "wb_list", "wb_status", "wb_debug", "wb_select", "wb_nav",
	"wb_probe_spawn", "wb_probe_remove", "wb_probe_status", "wb_probe_revive", "wb_ps_summary",
	"wb_probe_load", "wb_probe_start", "wb_probe_run", "wb_probe_stop", "wb_probe_runner_status", "wb_probe_goal",
	"wb_bot_start", "wb_bot_stop", "wb_bot_status", "wb_bot_verbose",
	"wb_bot_jump", "wb_bot_gait", "wb_bot_strafe", "wb_bot_suit", "wb_bot_abilities",
};
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

	m_demo.Initialize(framework,this);
	IConsole* console = gEnv->pConsole;
    m_config.Register(*console);

    for(const char* command:{"wb_demo_start","wb_demo_stop","wb_demo_mark"})console->AddCommand(command,DemoCommand,0,"Local IA server: start [label], stop, mark <text>; optional diagnostic recording");

    console->AddCommand("wb_watch", WatchCommand, 0, "Spectators only: wb_watch list|<botNumber>|next|prev|status|off; native F1 toggles first person");
    console->AddCommand("wb_watch_info", WatchInfoCommand, 0, "Local IA server only: decisions of bots followed by real spectators");
    console->AddCommand("wb_watch_check", WatchCheckCommand, 0, "Local IA server/no clients: assert native Lua spectator selection without changing any actor");
	console->AddCommand("wb_add", AddCommand, 0, "wb_add [count] [beginner|normal|veteran|elite] [style]: add autonomous bots");
	console->AddCommand("wb_remove", RemoveCommand, 0, "wb_remove <botNumber>: remove one bot (number from wb_list, not EntityId)");
	console->AddCommand("wb_remove_all", RemoveAllCommand, 0, "Remove every WarsBot");
	console->AddCommand("wb_kick_bots", RemoveAllCommand, 0, "Same as wb_remove_all");
	console->AddCommand("wb_fill", FillCommand, 0, "wb_fill <n> [beginner|normal|veteran|elite]: add or remove bots until there are n");
	console->AddCommand("wb_skill_all", SkillAllCommand, 0, "wb_skill_all <beginner|normal|veteran|elite>: re-add every bot at that skill (styles kept, bots respawn)");
	console->AddCommand("wb_rules_info", RulesInfoCommand, 0, "wb_rules_info: read-only game rules diagnostic (class, teams, spawn groups) - C-109 PS baseline");
	console->AddCommand("wb_mark", MarkCommand, 0, "wb_mark [text]: mark this moment for the playtest report (bind to F8); works on the client and the server");
	console->AddCommand("wb_list", ListCommand, 0, "List bots: number, EntityId, profile, state");
	console->AddCommand("wb_status", StatusCommand, 0, "Detailed status and per-bot update cost");
	console->AddCommand("wb_debug", DebugCommand, 0, "wb_debug 0|1: verbose telemetry for all bots");
    console->AddCommand("wb_spawn_box",SpawnBoxCommand,0,"wb_spawn_box <spawnId>: read-only local IA physics2m box names/classes/proxies");
    console->AddCommand("wb_revive_at",ReviveAtCommand,0,"wb_revive_at <spawnEntityId> [botNumber|me]: manual native kill/revive; selected bot by default; local IA only");
    console->AddCommand("wb_teleport",TeleportCommand,0,"wb_teleport <botNumber> x y z: manual owned live bot relocation; local IA only");
    console->AddCommand("wb_probe_item",ProbeItemCommand,0,"wb_probe_item <class> x y z: T-4 scenario probe - place a pickup (e.g. SCAR, DSG1, GaussRifle) at a fixed point; local IA only");
	console->AddCommand("wb_nav_explain", NavExplainCommand, 0, "wb_nav_explain <fromNode> <toNode> or after <seconds> <from> <to>: local IA bidirectional ray diagnostics");
    console->AddCommand("wb_qa_time_scale",QATimeScaleCommand,0,"Diagnostic only:1..8, password-protected local dedicated IA/no humans; resets on level end");
    console->AddCommand("wb_geom_export",GeometryExportCommand,0,"Offline local IA or PowerStruggle/no actors: static collision OBJ + spawn/item JSON; optional path.obj and play-area box x0 y0 x1 y1");
	console->AddCommand("wb_nav_export", NavExportCommand, 0, "wb_nav_export [path.json] or after <1..30 seconds>: local IA graph snapshot, no rays/memory changes");
	console->AddCommand("wb_nav_audit",NavAuditCommand,0,"Local/no actors: wb_nav_audit [0|1|2|stop]; explicit export-only knee variant, default0; stop resets ordinary graph");
    console->AddCommand("wb_nav", NavCommand, 0, "Navigation graph status for this level");
	console->AddCommand("wb_select", SelectCommand, 0, "wb_select <botNumber>: target of the legacy wb_bot_*/wb_probe_* commands");
	console->AddCommand("wb_probe_spawn", ProbeSpawnCommand, 0, "wb_probe_spawn x y z: add one bot without AI");
	console->AddCommand("wb_probe_remove", ProbeRemoveCommand, 0, "Remove the selected bot");
	console->AddCommand("wb_probe_status", ProbeStatusCommand, 0, "Status of the selected bot");
	console->AddCommand("wb_probe_revive", ProbeReviveCommand, 0, "Revive the selected dead bot");
	console->AddCommand("wb_ps_summary", PSSummaryCommand, 0, "PowerStruggle only: log team/PP/rank/buys per bot and owned capture points");
	// DS-047: BotProbeRunner.h host behind wb_probe_runner (default 0).
	console->AddCommand("wb_probe_load", ProbeLoadCommand, 0, "wb_probe_load <scenario.json>: parse a probe v1 scenario (needs wb_probe_runner 1)");
	console->AddCommand("wb_probe_start", ProbeStartCommand, 0, "wb_probe_start: run the loaded scenario setup and start the metrics window");
	console->AddCommand("wb_probe_run", ProbeRunCommand, 0, "wb_probe_run <id> [seconds]: start observing without a scenario file (external setup); seconds defaults to the loaded scenario duration");
	console->AddCommand("wb_probe_stop", ProbeStopCommand, 0, "wb_probe_stop: stop the running probe without a result file");
	console->AddCommand("wb_probe_runner_status", ProbeRunnerStatusCommand, 0, "Probe runner state: flag, loaded scenario, remaining seconds, live metrics");
	console->AddCommand("wb_probe_goal", ProbeGoalCommand, 0, "wb_probe_goal x y z [radius]: arrival marker for the loaded scenario");
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
		m_config.maxBots, kHardLimit);
}

void BotManager::Shutdown()
{
	if (!m_initialized)
		return;
	m_initialized = false;
    m_demo.Shutdown();
	for (const char* name : kCommands) gEnv->pConsole->RemoveCommand(name);
	m_config.Unregister(*gEnv->pConsole);
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

bool BotManager::ShouldYieldLadder(unsigned long long ref, const Vec3& a, float myDist2, int myNumber) const
{
	for (const auto& entry : m_bots)
		if (entry.second && entry.second->AIEnabled() && entry.second->Brain().YieldLadderTo(ref, a, myDist2, myNumber))
			return true;
	return false;
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
    if(m_demo.Active() && !std::strcmp(phase,"after-Lua"))m_demo.Hit(hit);
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
    // DS-035: distance to the target at the shot (was only on hits, so misses had no range band)
    IEntity* shooterEntity=gEnv->pEntitySystem->GetEntity(shooter);
    IEntity* targetEntity=bot->Brain().Target()?gEnv->pEntitySystem->GetEntity(bot->Brain().Target()):nullptr;
    const float distance=shooterEntity && targetEntity?(targetEntity->GetWorldPos()-shooterEntity->GetWorldPos()).GetLength():-1.0f;
    WB_LOG(2,"[WarsBots] FIRE SHOT shooter=%u target=%u projectile=%u weapon=%u class=%s mode=%s rpm=%.2f clip=%d remote=%d distance=%.2f.",
        shooter,bot->Brain().Target(),projectile,item?item->GetEntityId():0,
        item?item->GetEntity()->GetClass()->GetName():"unknown",mode?mode->GetType():"unknown",mode?mode->GetFireRate():0,mode?mode->GetAmmoCount():-1,bullet?int(bullet->IsRemote()):-1,distance);
}

void BotManager::TraceKill(EntityId targetId, EntityId shooterId, bool killerEventEmitted)
{
    m_demo.Kill(targetId,shooterId);
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
	return CryMPEngineAdapter::LocalIAGate(m_framework, m_levelReady, m_config.gateOpen, false, true);
}

bool BotManager::CanRunLocalExport() const
{
	return CryMPEngineAdapter::LocalExportGate(m_framework, m_levelReady, m_config.gateOpen, true);
}

int BotManager::MaxBots() const
{
	return clamp_tpl(m_config.maxBots, 1, kHardLimit);
}

EntityId BotManager::ChooseSpawnLocation(EntityId self, EntityId avoid, const std::vector<EntityId>* rejected) const
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
            if(BotConfig::Get().spawnExclude195==1 && candidate==195){
                if(!m_spawnExcludeLogged){WB_LOG(0,"[WarsBots] SPAWN EXCLUDE id=195 reason=revive_freeze");m_spawnExcludeLogged=true;}
                continue;
            }
			if(rejected && std::find(rejected->begin(),rejected->end(),candidate)!=rejected->end())continue;
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
    if(m_nav.AuditOnly()){WB_LOG(0,"[WarsBots] Add refused: export-only NAV AUDIT active");return nullptr;}
	if (!CryMPEngineAdapter::LocalIAGate(m_framework, m_levelReady, m_config.gateOpen, true, true))
		return nullptr;
	if (Count() >= MaxBots())
	{
		WB_LOG(0, "[WarsBots] Add refused: %d/%d bots (raise wb_maxbots, hard limit %d).", Count(), MaxBots(), kHardLimit);
		return nullptr;
	}
	const int number = FreeNumber();
	if (!number) return nullptr;
	const bool psGroups = g_pGame && g_pGame->GetGameRules() && BotMatch::Mode(*g_pGame->GetGameRules()).Policy().spawnGroups;
	if (!position && !psGroups && !ChooseSpawnLocation(0, 0))   // C-136: PS picks inside the team group in AddWith
	{
		WB_LOG(0, "[WarsBots] Add deferred: no safe IA spawn point free of other players; try again or pass x y z.");
		return nullptr;
	}
	// Default rotation through the six styles so a full roster never moves in lockstep.
	const BotStyle chosenStyle = style ? *style : BotStyle((number - 1) % int(BotStyle::Count));
	BotProfile profile=BotProfile::Make(chosenStyle, skill);
	if (BotConfig::Get().difficulty==1) ApplyDifficulty(profile); // DS-014 ladder; a restored roster keeps its profile
	return AddWith(position, profile, number, startAI);
}

BotInstance* BotManager::AddWith(const Vec3* position, const BotProfile& profile, int number, bool startAI)
{
	if (!number || ByNumber(number) || Count() >= MaxBots()) return nullptr;
	Vec3 where(0,0,0);
	// C-136 (C-135 X-183, DS-052 #1): the very first spawn happens here, before the PS team/spawn group exist on the
	// actor; the IA picker put PS bots on neutral bunkers. In a spawn-group mode pick inside the slot's team default
	// group (team base), the same team BotInstance::Spawn assigns (TeamForSlot).
	EntityId groupSpawn = 0;
	if (!position)
		if (CGameRules* rules = g_pGame ? g_pGame->GetGameRules() : nullptr)
			if (BotMatch::Mode(*rules).Policy().spawnGroups)
			{
				const int team = TeamForSlot(number);
				const EntityId group = rules->GetTeamDefaultSpawnGroup(team);
				float zOff = 0.f;
				groupSpawn = group ? rules->GetSpawnLocation(0, true, false, group, 0.f, Vec3(0,0,0), &zOff, 0) : 0;
				WB_LOG(1, "[WarsBots] #%d PS initial spawn team=%d group=%u spawn=%u.", number, team, group, groupSpawn);
				if (!groupSpawn) return nullptr;   // never the IA picker in PS
			}
	if (position) where = *position;
	else
	{
		const EntityId spawnId = groupSpawn ? groupSpawn : ChooseSpawnLocation(0, 0);
		IEntity* spawn = spawnId ? gEnv->pEntitySystem->GetEntity(spawnId) : nullptr;
		if (!spawn) return nullptr;
		where = spawn->GetWorldPos() + Vec3(0,0,0.2f);
	}
	if(m_config.demoRecord==1 && !m_demo.Active())m_demo.Start(nullptr,m_config.demoHz);
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
    m_demo.EndLevel();
	m_probe.Stop(); // DS-047: a scenario cannot outlive its level
    if(m_qaTimeScale!=1.f){gEnv->pTimer->SetTimeScale(1.f);m_qaTimeScale=1.f;}
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
	// DS-079: only a true external deletion restores. An explicit removal may be deferred, so `Removing()` (transient,
	// true only inside RemoveEntity) is not enough: RemovalPending() carries the durable intent, Expired() covers a
	// duplicate DONE after OnEntityDone. A foreign entity never resolves to a registered bot (known=false).
	// True external case: native IA RestartGame -> GameFramework::Reset deletes runtime actors without
	// eAE_resetBegin, so the same bot is re-created next frame.
	const BotDoneAction action = BotDoneDecision(true, m_levelReady, bot->Removing(), bot->RemovalPending(), bot->Expired());
	if (action == BotDoneAction::RestoreExternal)
	{
		m_roster.push_back({ bot->Number(), bot->Profile(), bot->AIEnabled() });
		m_restorePending = true;
		WB_LOG(0, "[WarsBots] #%d removed externally (game reset?); will re-create.", bot->Number());
	}
	bot->OnEntityDone();
}

void BotManager::OnPostUpdate(float dt)
{
    if(m_qaTimeScale!=1.f){
        CGameRules::TPlayers humans;if(g_pGame && g_pGame->GetGameRules())g_pGame->GetGameRules()->GetPlayers(humans);
        if(!humans.empty() || !CanRunLocalIA()){
            gEnv->pTimer->SetTimeScale(1.f);m_qaTimeScale=1.f;
            WB_LOG(0,"[WarsBots] QA TIME SCALE reset reason=human_or_gate");
        }
    }
    m_moveTrace.Update(m_framework,*this);
	UpdateProbe(); // DS-047: runs even when the bot roster is empty (dummy/goal scenarios)
	// Restore outside the action-event callback, once the reset level is usable again.
	if (m_restorePending && m_levelReady && CanRunLocalIA()) { Sweep(); RestoreRoster(); }
    if(m_nav.AuditOnly()){if(CanRunLocalIA()){m_nav.Begin();m_nav.Step(2.f);}m_demo.Update(m_config.demoRecord==1,m_config.demoHz);return;}
	if (m_bots.empty()){m_demo.Update(m_config.demoRecord==1,m_config.demoHz);return;}
	// Navigation graph is built lazily once bots exist, a few ms per frame; geometry survives
	// IA game resets, so only level changes discard it.
	if (m_levelReady && CanRunLocalIA()) { m_nav.Begin(); m_nav.Step(2.0f); m_nav.Tick(dt); }
	m_clock += dt;
	// C-115: server frame time and bot AI cost, logged every 10 s (Codex had no native frame timer)
	const float realFrame = gEnv->pTimer->GetRealFrameTime();
	if (m_frameCount < kFrameSamples) m_frames[m_frameCount++] = realFrame; else m_frames[(m_frameNext++) % kFrameSamples] = realFrame;
	const auto aiStart = std::chrono::steady_clock::now();
	// Stable order by display number; vector copy keeps iteration valid if a callback sweeps.
	for (BotInstance* bot : Ordered()) bot->Update(dt);
	m_aiSeconds += std::chrono::duration<float>(std::chrono::steady_clock::now() - aiStart).count(); ++m_aiFrames;
	Sweep();
	if (m_clock >= m_nextFrameLog && m_frameCount > 0)
	{
		m_nextFrameLog = m_clock + 10.0f;
		std::vector<float> f(m_frames, m_frames + m_frameCount); std::sort(f.begin(), f.end());
		float sum = 0; for (float x : f) sum += x;
		WB_LOG(1, "[WarsBots] FRAME avg=%.2fms p95=%.2fms max=%.2fms botAI=%.3fms/frame bots=%d server=%d.",
			1000.f * sum / f.size(), 1000.f * f[size_t(0.95f * (f.size() - 1))], 1000.f * f.back(),
			m_aiFrames ? 1000.f * m_aiSeconds / m_aiFrames : 0.f, Count(), gEnv->bServer ? 1 : 0);
		m_frameCount = 0; m_frameNext = 0; m_aiSeconds = 0; m_aiFrames = 0;
	}
	if (m_defaultVerbose && m_clock >= m_nextProfileLog)
	{
		StatusReport();
		m_nextProfileLog = m_clock + 10.0f;
	}
    m_demo.Update(m_config.demoRecord==1,m_config.demoHz);
}

void BotManager::OnLevelNotFound(const char*) { m_restorePending = false; m_roster.clear(); EndLevel(); m_nav.Reset(); }
void BotManager::OnLoadingStart(ILevelInfo*) { m_restorePending = false; m_roster.clear(); EndLevel(); m_nav.Reset(); m_moveTrace.Reset(); }
void BotManager::OnLoadingComplete(ILevel*) { m_spawnExcludeLogged=false; m_levelReady = true; g_fireBudget=20000; }
void BotManager::OnLoadingError(ILevelInfo*, const char*) { EndLevel(); m_nav.Reset(); }
void BotManager::OnLoadGame(ILoadGame*) { EndLevel(); m_nav.Reset(); }
void BotManager::OnLevelEnd(const char*) { EndLevel(); m_nav.Reset(); m_moveTrace.Reset(); }

void BotManager::OnActionEvent(const SActionEvent& event)
{
	if (event.m_event == eAE_resetBegin)
	{
		// Native IA restart (e.g. PreGame start timer). Remember who existed, then clean up.
		// DS-080: keep external-DONE entries already queued, add live bots, but never resurrect an explicit pending
		// removal or an already-DONE slot (the v3 log shows a deferred removal pending for ~0.2 s).
		for (BotInstance* bot : Ordered())
		{
			const bool queued = std::any_of(m_roster.begin(), m_roster.end(),
				[number = bot->Number()](const RosterEntry& entry) { return entry.number == number; });
			if (!ShouldRosterForReset(bot->RemovalPending(), bot->Expired(), queued)) continue;
			m_roster.push_back({ bot->Number(), bot->Profile(), bot->AIEnabled() });
		}
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
	WB_LOG(0, "[WarsBots] PROFILE bots=%d avgSum=%.3fms/frame worstFrame=%.3fms rays=%u targetScans=%u (since last report).",
		Count(), total, worst, BotCombat::TakeRayCount(), BotCombat::TakeScanCount());
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
        const size_t len=std::strlen(name); // names may carry " [Skill/Style]" (wb_bot_name_profile)
        for (EntityId id:ids)
        {
            const char* n=gEnv->pEntitySystem->GetEntity(id)->GetName();
            if (!std::strncmp(n,name,len) && (n[len]==0 || n[len]==' ')) target=id;
        }
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
	BotSkill skill = BotSkill(clamp_tpl(self->m_config.skill, 0, int(BotSkill::Count) - 1));
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

void BotManager::FillCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!self || !args || !gEnv->bServer) return;
	int target = 0;
	BotSkill skill = BotSkill(clamp_tpl(self->m_config.skill, 0, int(BotSkill::Count) - 1));
	if (args->GetArgCount() < 2 || args->GetArgCount() > 3 || !ParseInt(args->GetArg(1), target) || target < 0 || target > kHardLimit ||
		(args->GetArgCount() == 3 && !BotProfile::ParseSkill(args->GetArg(2), skill)))
	{ WB_LOG(0, "[WarsBots] Usage: wb_fill <0..%d> [beginner|normal|veteran|elite].", kHardLimit); return; }
	// DS-024 #3: a removal the engine defers keeps its slot, so `while (Count() > target)` removed the same bot forever.
	// Each bot is removed at most once, highest number first; slots pending removal do not count as live bots.
	self->Sweep();
	std::vector<BotInstance*> order = self->Ordered();
	int live = 0;
	for (BotInstance* b : order) if (!b->RemovalPending() && !b->Expired()) ++live;
	std::sort(order.begin(), order.end(), [](BotInstance* a, BotInstance* b) { return a->Number() > b->Number(); });
	for (BotInstance* b : order)
	{
		if (live <= target) break;
		if (b->RemovalPending() || b->Expired()) continue;
		self->RemoveInstance(*b); --live;   // may erase b; not touched again
	}
	for (int guard = 0; live < target && guard < kHardLimit; ++guard) { if (!self->Add(nullptr, skill, nullptr, true)) break; ++live; }
	WB_LOG(0, "[WarsBots] wb_fill: now %d/%d bots.", self->Count(), self->MaxBots());
}

void BotManager::SkillAllCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	BotSkill skill = BotSkill::Normal;
	if (!self || !args || !gEnv->bServer || args->GetArgCount() != 2 || !BotProfile::ParseSkill(args->GetArg(1), skill))
	{ WB_LOG(0, "[WarsBots] Usage: wb_skill_all <beginner|normal|veteran|elite>."); return; }
	std::vector<BotStyle> styles;
	for (auto& entry : self->m_bots) styles.push_back(entry.second->Profile().style);
	self->RemoveAll("wb_skill_all");
	int added = 0;
	for (const BotStyle& style : styles) if (self->Add(nullptr, skill, &style, true)) ++added;
	WB_LOG(0, "[WarsBots] wb_skill_all: %d bots re-added as %s.", added, BotProfile::SkillName(skill));
}

// Owner presses F8 when a bot does something dumb; the playtest report turns each mark into a "why did it do that" card
// from the logs around it (DS-022). Client log on the owner's machine, server log on a local server; never changes state.
void BotManager::MarkCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	std::string text;
	for (int i = 1; args && i < args->GetArgCount(); ++i) { if (i > 1) text += ' '; text += args->GetArg(i); }
	CPlayer* me = self && self->m_framework ? CPlayer::FromIActor(self->m_framework->GetClientActor()) : nullptr;
	const float now = gEnv->pTimer->GetCurrTime();
	// DS-022: the mark also goes into the demo (event 10) with the wall clock, so a report card finds it in both
	time_t wall = time(nullptr); char clock[32] = {0}; strftime(clock, sizeof(clock), "%Y-%m-%d %H:%M:%S", localtime(&wall));
	WB_LOG(0, "[WarsBots] OWNER MARK wall=%s t=%.3f", clock, now);
	if (self) self->Demo().Mark(text.empty() ? "owner" : text.c_str());
	if (!me) { WB_LOG(0, "[WarsBots] OWNER MARK t=%.3f side=%s text=\"%s\"", now, gEnv->bServer ? "server" : "client", text.c_str()); return; }
	const Vec3 pos = me->GetEntity()->GetWorldPos(), view = me->GetViewRotation().GetColumn1();
	const EntityId watched = me->GetSpectatorTarget();
	WB_LOG(0, "[WarsBots] OWNER MARK t=%.3f side=%s pos=(%.2f,%.2f,%.2f) yaw=%.1f pitch=%.1f spectating=%u text=\"%s\"",
		now, gEnv->bServer ? "server" : "client", pos.x, pos.y, pos.z, RAD2DEG(atan2f(view.x, view.y)),
		RAD2DEG(atan2f(view.z, sqrtf(view.x * view.x + view.y * view.y))), unsigned(watched), text.c_str());
	static int s_marks = 0;
	CryLogAlways("$3[WarsBots] mark #%d saved", ++s_marks);   // visible confirmation for the owner (playtest 3: marks lost)
}

// C-109: Codex needs the runtime PS baseline (rules class, team count, spawn groups) without a Lua console. Read-only.
void BotManager::RulesInfoCommand(IConsoleCmdArgs*)
{
	CGameRules* rules = g_pGame ? g_pGame->GetGameRules() : nullptr;
	if (!rules) { WB_LOG(0, "[WarsBots] RULES INFO none (no game rules yet)."); return; }
	const int teams = rules->GetTeamCount();
	std::string names;
	for (int t = 1; t <= teams; ++t) { if (t > 1) names += ','; const char* n = rules->GetTeamName(t); names += n ? n : "?"; }
	WB_LOG(0, "[WarsBots] RULES INFO class=%s teams=%d names=%s spawnGroups=%d.",
		rules->GetEntity() ? rules->GetEntity()->GetClass()->GetName() : "?", teams, names.c_str(), rules->GetSpawnGroupCount());
}

int PsLuaInt(CGameRules* rules, EntityId id, const char* func)
{
	if (!rules || !gEnv || !gEnv->pScriptSystem) return -1;
	IEntity* entity = rules->GetEntity();
	IScriptTable* table = entity ? entity->GetScriptTable() : nullptr;
	if (!table) return -1;
	IScriptSystem* ss = gEnv->pScriptSystem;
	int value = -1;
	if (ss->BeginCall(table, func)) { ss->PushFuncParam(table); ss->PushFuncParam(ScriptHandle(id)); ss->EndCall(value); }
	return value;
}

void BotManager::PSSummaryCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	CGameRules* rules = g_pGame ? g_pGame->GetGameRules() : nullptr;
	if (!self || !rules) return;
	if (!BotMatch::IsPowerStruggle(*rules)) { WB_LOG(0, "[WarsBots] PS SUMMARY skipped: not PowerStruggle."); return; }
	std::vector<PsCapturePoint> points;
	PSCaptureSnapshot(points, *rules, nullptr); // summary counts all native capturables, not one actor's route filter
	int held[3] = {0, 0, 0};
	for (const PsCapturePoint& point : points) if (point.ownerTeam >= 0 && point.ownerTeam < 3) ++held[point.ownerTeam];
	WB_LOG(0, "[WarsBots] PS SUMMARY points=%d team1_held=%d team2_held=%d.", int(points.size()), held[1], held[2]);
	for (BotInstance* bot : self->Ordered())
	{
		CActor* actor = bot->GetActor();
		if (!actor) continue;
		const EntityId id = actor->GetEntityId();
		WB_LOG(0, "[WarsBots] PS SUMMARY bot=%d id=%u team=%d pp=%d rank=%d buys=%d.",
			bot->Number(), id, rules->GetTeam(id), PsLuaInt(rules, id, "GetPlayerPP"), PsLuaInt(rules, id, "GetPlayerRank"), bot->Brain().PSBuys());
	}
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

// T-4 (PROBES_SPEC.md hook 1): scenario probes need a fixed item at a fixed point (weapon_pickup, sniper_reach).
void BotManager::ProbeItemCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self(); Vec3 position(0,0,0);
    if(!self || !self->CanRunLocalIA()) {WB_LOG(0,"[WarsBots] PROBE ITEM refused: local IA gate closed");return;}
    if(!args || args->GetArgCount()!=5 || !ParsePosition(args,2,position)) {WB_LOG(0,"[WarsBots] Usage: wb_probe_item <class> x y z");return;}
    IEntityClass* cls=gEnv->pEntitySystem->GetClassRegistry()->FindClass(args->GetArg(1));
    if(!cls || !g_pGame->GetIGameFramework()->GetIItemSystem()->IsItemClass(args->GetArg(1))) {WB_LOG(0,"[WarsBots] PROBE ITEM refused: %s is not an item class",args->GetArg(1));return;}
    static int serial=0; char name[64]; std::snprintf(name,sizeof name,"wb_probe_%s_%d",args->GetArg(1),++serial);
    SEntitySpawnParams params; params.pClass=cls; params.sName=name; params.vPosition=position; params.qRotation=Quat::CreateIdentity();
    IEntity* item=gEnv->pEntitySystem->SpawnEntity(params,true);
    WB_LOG(0,"[WarsBots] PROBE ITEM %s id=%u at (%.2f,%.2f,%.2f)",args->GetArg(1),item?item->GetId():0u,position.x,position.y,position.z);
}

void BotManager::SpawnBoxCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();int id=0;
    if(!self || !self->CanRunLocalIA()) {WB_LOG(0,"[WarsBots] SPAWN BOX refused: local IA gate closed");return;}
    if(!args || args->GetArgCount()!=2 || !ParseInt(args->GetArg(1),id) || id<=0)return;
    CGameRules* rules=g_pGame->GetGameRules();bool registered=false;
    for(int i=0;i<rules->GetSpawnLocationCount();++i)registered |= rules->GetSpawnLocation(i)==EntityId(id);
    IEntity* spawn=registered?gEnv->pEntitySystem->GetEntity(EntityId(id)):nullptr;if(!spawn)return;
    IPhysicalEntity** entities=nullptr;const Vec3 center=spawn->GetWorldPos(),radius(2,2,2);
    const int count=gEnv->pPhysicalWorld->GetEntitiesInBox(center-radius,center+radius,entities,ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid|ent_living);
    WB_LOG(0,"[WarsBots] SPAWN BOX spawn=%d center=(%.3f,%.3f,%.3f) count=%d cap=128",id,center.x,center.y,center.z,count);
    for(int i=0;i<count && i<128;++i) {
        IPhysicalEntity* physics=entities[i];if(!physics)continue;
        IEntity* entity=static_cast<IEntity*>(physics->GetForeignData(PHYS_FOREIGN_ID_ENTITY));
        unsigned proxies=0;if(entity)for(int p=0;p<ENTITY_PROXY_LAST;++p)if(entity->GetProxy(EEntityProxy(p)))proxies|=1u<<p;
        pe_status_pos pose;const bool valid=physics->GetStatus(&pose)!=0;
        WB_LOG(0,"[WarsBots] SPAWN BOX item=%d actor=%u name=%s class=%s type=%d foreign=%d proxies=0x%x pose=%d pos=(%.3f,%.3f,%.3f)",i,entity?entity->GetId():0,entity?entity->GetName():"none",entity?entity->GetClass()->GetName():"static_or_terrain",physics->GetType(),physics->GetiForeignData(),proxies,valid?1:0,valid?pose.pos.x:0,valid?pose.pos.y:0,valid?pose.pos.z:0);
    }
}

void BotManager::ReviveAtCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();int spawnNumber=0,number=0;
    if(self && self->CanRunLocalIA() && args && args->GetArgCount()==5 && !std::strcmp(args->GetArg(1),"after")) {
        int delay=0,spawn=0,bot=0;
        if(ParseInt(args->GetArg(2),delay) && delay>=1 && delay<=300 && ParseInt(args->GetArg(3),spawn) && spawn>0 && ParseInt(args->GetArg(4),bot) && bot>0 && bot<=16) {
            char script[192];std::snprintf(script,sizeof(script),"Script.SetTimer(%d,function() System.ExecuteCommand('wb_revive_at %d %d') end)",delay*1000,spawn,bot);
            gEnv->pScriptSystem->ExecuteBuffer(script,std::strlen(script),"WarsBots explicit delayed diagnostic revive");
        }
        return;
    }
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
    float offset=.2f;
    const Vec3 position=spawn->GetWorldPos()+Vec3(0,0,offset); const Ang3 angles(spawn->GetWorldRotation());
    const bool running=bot && bot->AIEnabled();
    if(running)bot->Brain().Stop(*bot);
    WB_LOG(0,"[WarsBots] DEBUG REVIVE actor=%u bot=%d spawnId=%d manual=1 nativeKill=%d zOffset=%.3f hooks=%d",id,bot?bot->Number():0,spawnNumber,actor->GetHealth()>0?1:0,offset,0);
    if(actor->GetHealth()>0)rules->KillPlayer(actor,false,true,id,0,float(actor->GetHealth()),0,0,Vec3(0,0,0));
    // Kill callbacks may remove an entity; never use a stale actor pointer.
    CActor* current=static_cast<CActor*>(self->m_framework->GetIActorSystem()->GetActor(id));
    if(current && current==actor && current->GetHealth()<=0) {
        if(bot)bot->Revive(EntityId(spawnNumber),offset);
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

void BotManager::DemoCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();if(!self || !self->CanRunLocalIA() || !gEnv->bServer || !args)return;
    const char* command=args->GetArg(0);
    if(!std::strcmp(command,"wb_demo_start")){
        if(args->GetArgCount()>2)return;
        if(self->m_demo.Start(args->GetArgCount()==2?args->GetArg(1):nullptr,self->m_config.demoHz))self->m_config.demoRecord=1;
    }else if(!std::strcmp(command,"wb_demo_stop")){self->m_config.demoRecord=0;self->m_demo.Stop();}
    else if(args->GetArgCount()>1){std::string text;for(int i=1;i<args->GetArgCount();++i){if(i>1)text+=' ';text+=args->GetArg(i);}self->m_demo.Mark(text.c_str());}
}

void BotManager::QATimeScaleCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();CGameRules* rules=g_pGame?g_pGame->GetGameRules():nullptr;
    CGameRules::TPlayers humans;if(rules)rules->GetPlayers(humans);
    ICVar* password=gEnv->pConsole->GetCVar("sv_password");float value=0;
    if(!self || !self->CanRunLocalIA() || !gEnv->pSystem->IsDedicated() || !humans.empty() ||
       !password || std::strlen(password->GetString())<16 || !args || args->GetArgCount()!=2 ||
       !ParseCoordinate(args->GetArg(1),value) || value<1 || value>8)
    {WB_LOG(0,"[WarsBots] QA TIME SCALE refused: dedicated local IA/password>=16/no humans; value1..8");return;}
    gEnv->pTimer->SetTimeScale(value);self->m_qaTimeScale=value;
    WB_LOG(0,"[WarsBots] QA TIME SCALE requested=%.3f actual=%.3f",value,gEnv->pTimer->GetTimeScale());
}

void BotManager::GeometryExportCommand(IConsoleCmdArgs* args)
{
    BotManager* self=Self();CGameRules* rules=g_pGame?g_pGame->GetGameRules():nullptr;
    CGameRules::TPlayers humans;if(rules)rules->GetPlayers(humans);
    if(!self || !gEnv->bServer || !self->CanRunLocalExport() || self->Count()!=0 || !humans.empty())
    {WB_LOG(0,"[WarsBots] GEOM export refused: offline local IA or PowerStruggle (sv_bind 127.0.0.1, sv_lanonly 1, gate opt-in)/no bots or real players required");return;}
    const int argc=args?args->GetArgCount():0;
    if(!args || (argc!=1 && argc!=2 && argc!=5 && argc!=6)){WB_LOG(0,"[WarsBots] Usage: wb_geom_export [path.obj] [x0 y0 x1 y1]");return;}
    float region[4];const bool hasRegion=argc>=5;const int r0=argc==6?2:1;
    if(hasRegion)for(int i=0;i<4;++i){char* end=nullptr;const char* tok=args->GetArg(r0+i);region[i]=std::strtof(tok,&end);if(!end || end==tok || *end){WB_LOG(0,"[WarsBots] GEOM export: bad region number %s",args->GetArg(r0+i));return;}}
    ExportLevelGeometry(argc==2||argc==6?args->GetArg(1):nullptr,hasRegion?region:nullptr);
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

void BotManager::NavAuditCommand(IConsoleCmdArgs* args)
{
 BotManager* self=Self();if(!self || !self->CanRunLocalIA() || !args)return;
 CGameRules::TPlayers players;g_pGame->GetGameRules()->GetPlayers(players);
 if(self->Count() || !players.empty()){WB_LOG(0,"[WarsBots] NAV AUDIT refused: no bots/players allowed");return;}
 if(args->GetArgCount()==2 && !std::strcmp(args->GetArg(1),"stop")){self->m_nav.Reset();return;}
 // C154 net-zero flags: retire the latched wb_nav_knee_fix switch; preserve its diagnostic modes per command.
 int variant=0;
 if(args->GetArgCount()==2) {
  if(!ParseInt(args->GetArg(1),variant) || variant<0 || variant>2) {WB_LOG(0,"[WarsBots] Usage: wb_nav_audit [0|1|2|stop]");return;}
 } else if(args->GetArgCount()!=1) {WB_LOG(0,"[WarsBots] Usage: wb_nav_audit [0|1|2|stop]");return;}
 self->m_nav.StartAudit(variant);
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
	if (!self || !args || args->GetArgCount() < 4 || args->GetArgCount() > 7 || !ParsePosition(args, 1, position))
	{ WB_LOG(0, "[WarsBots] Usage: wb_probe_spawn x y z [beginner|normal|veteran|elite] [style] [0|1]."); return; }
	BotSkill skill = BotSkill(clamp_tpl(self->m_config.skill, 0, int(BotSkill::Count) - 1));
	BotStyle style;
	bool hasStyle = false, ai = false;
	if (args->GetArgCount() >= 5 && !BotProfile::ParseSkill(args->GetArg(4), skill))
	{ WB_LOG(0, "[WarsBots] wb_probe_spawn: unknown skill '%s'.", args->GetArg(4)); return; }
	if (args->GetArgCount() >= 6)
	{
		if (!BotProfile::ParseStyle(args->GetArg(5), style))
		{ WB_LOG(0, "[WarsBots] wb_probe_spawn: unknown style '%s'.", args->GetArg(5)); return; }
		hasStyle = true;
	}
	if (args->GetArgCount() >= 7) ai = std::strcmp(args->GetArg(6), "0") != 0;
	// DS-047: scenario setup sends skill/style/ai; the legacy three-argument form is unchanged.
    const BotStyle qaStyle=BotStyle::Universal;
	if (BotInstance* bot = self->Add(&position, skill, hasStyle?&style:(BotConfig::Get().qaSocomOnly==1?&qaStyle:nullptr), ai))
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

// --------------------------------------------------------------------------------------------- DS-047 probe host
// BotProbeRunner.h owns parsing/metrics/assertions; this adapter owns file IO, console setup and the result file.
namespace
{
std::string ProbeSafeName(const std::string& id)
{
	std::string out = id.empty() ? "run" : id;
	for (char& c : out) if (!std::isalnum((unsigned char)c) && c != '-' && c != '_') c = '_';
	return out;
}
}

void BotManager::ProbeLoadCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!ProbeRunnerEnabled()) { WB_LOG(0, "[WarsBots] PROBE RUNNER OFF: set wb_probe_runner 1 first."); return; }
	if (!self || !args || args->GetArgCount() != 2)
	{ WB_LOG(0, "[WarsBots] Usage: wb_probe_load <scenario.json>."); return; }
	if (self->m_probe.Running())
	{ WB_LOG(0, "[WarsBots] PROBE LOAD refused: a scenario is running (wb_probe_stop)."); return; }
	std::string text, error;
	if (!ReadProbeFile(args->GetArg(1), text))
	{ WB_LOG(0, "[WarsBots] PROBE LOAD failed: cannot read %s.", args->GetArg(1)); return; }
	if (!self->m_probe.Load(text, &error))
	{ WB_LOG(0, "[WarsBots] PROBE LOAD failed: %s.", error.c_str()); return; }
	const Probe::Scenario& s = self->m_probe.scenario;
	WB_LOG(0, "[WarsBots] PROBE LOAD id=%s map=%s duration=%.1f bots=%d items=%d assertions=%d goal=%d.",
		s.id.c_str(), s.map.c_str(), s.duration, int(s.bots.size()), int(s.items.size()), int(s.assertions.size()), s.hasGoal ? 1 : 0);
}

void BotManager::ProbeStartCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (!ProbeRunnerEnabled()) { WB_LOG(0, "[WarsBots] PROBE RUNNER OFF: set wb_probe_runner 1 first."); return; }
	if (!self) return;
	if (!self->m_probe.loaded)
	{ WB_LOG(0, "[WarsBots] PROBE START refused: wb_probe_load <scenario.json> first."); return; }
	if (self->m_probe.Running())
	{ WB_LOG(0, "[WarsBots] PROBE START refused: already running id=%s.", self->m_probe.scenario.id.c_str()); return; }
	int ran = 0, unsupported = 0;
	for (const std::string& command : Probe::BuildSetupCommands(self->m_probe.scenario))
	{
		if (command.compare(0, 12, "wb_probe_run") == 0) continue;
		if (!ProbeSetupSupported(command))
		{ WB_LOG(0, "[WarsBots] PROBE UNSUPPORTED %s", command.c_str()); ++unsupported; continue; }
		if (gEnv->pConsole) { gEnv->pConsole->ExecuteString(command.c_str()); ++ran; }
	}
	self->m_probeGoalDone = false;
	self->m_probeNextGoalCheck = 0;
	self->m_probe.Start(gEnv->pTimer->GetFrameStartTime().GetSeconds());
	WB_LOG(0, "[WarsBots] PROBE START id=%s duration=%.1f setup=%d unsupported=%d goal=%d cvars=%d.",
		self->m_probe.scenario.id.c_str(), self->m_probe.end - self->m_probe.start,
		ran, unsupported, self->m_probe.scenario.hasGoal ? 1 : 0, int(self->m_probe.scenario.cvars.size()));
}

void BotManager::ProbeRunCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	if (!ProbeRunnerEnabled()) { WB_LOG(0, "[WarsBots] PROBE RUNNER OFF: set wb_probe_runner 1 first."); return; }
	if (!self || !args || (args->GetArgCount() != 2 && args->GetArgCount() != 3))
	{ WB_LOG(0, "[WarsBots] Usage: wb_probe_run <id> [seconds]."); return; }
	float seconds = 0;
	if (args->GetArgCount() == 3)
	{
		if (!ParseCoordinate(args->GetArg(2), seconds) || seconds <= 0 || seconds > 3600)
		{ WB_LOG(0, "[WarsBots] wb_probe_run: duration must be 0..3600 s."); return; }
	}
	else if (self->m_probe.loaded && self->m_probe.scenario.duration > 0) seconds = float(self->m_probe.scenario.duration);
	else seconds = 60.0f;
	if (self->m_probe.Running())
	{ WB_LOG(0, "[WarsBots] PROBE RUN refused: already running id=%s.", self->m_probe.scenario.id.c_str()); return; }
	if (!self->m_probe.loaded) { self->m_probe.scenario = Probe::Scenario(); self->m_probe.loaded = true; }
	self->m_probe.scenario.id = args->GetArg(1);
	self->m_probe.scenario.duration = seconds;
	const double now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	self->m_probeGoalDone = false;
	self->m_probeNextGoalCheck = 0;
	self->m_probe.Start(now, seconds);
	WB_LOG(0, "[WarsBots] PROBE START id=%s duration=%.1f setup=external assertions=%d.",
		self->m_probe.scenario.id.c_str(), seconds, int(self->m_probe.scenario.assertions.size()));
}

void BotManager::ProbeStopCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (!self) return;
	const std::string id = self->m_probe.scenario.id;
	self->m_probe.Stop();
	WB_LOG(0, "[WarsBots] PROBE STOP id=%s active=0.", id.c_str());
}

void BotManager::ProbeRunnerStatusCommand(IConsoleCmdArgs*)
{
	BotManager* self = Self();
	if (!self) return;
	const double now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	WB_LOG(0, "[WarsBots] PROBE RUNNER flag=%d loaded=%d active=%d id=%s remaining=%.1f shots=%d hits=%d kills=%d deaths=%d pickups=%d revives=%d.",
		BotConfig::Get().probeRunner, self->m_probe.loaded ? 1 : 0, self->m_probe.Running() ? 1 : 0,
		self->m_probe.scenario.id.c_str(), self->m_probe.Remaining(now),
		self->m_probe.metrics.shots, self->m_probe.metrics.hits, self->m_probe.metrics.kills,
		self->m_probe.metrics.deaths, self->m_probe.metrics.pickups, self->m_probe.metrics.revives);
}

void BotManager::ProbeGoalCommand(IConsoleCmdArgs* args)
{
	BotManager* self = Self();
	Vec3 position(0,0,0);
	if (!ProbeRunnerEnabled()) { WB_LOG(0, "[WarsBots] PROBE RUNNER OFF: set wb_probe_runner 1 first."); return; }
	if (!self || !args || args->GetArgCount() < 4 || !ParsePosition(args, 1, position))
	{ WB_LOG(0, "[WarsBots] Usage: wb_probe_goal x y z [radius]."); return; }
	float radius = 1.5f;
	if (args->GetArgCount() >= 5 && (!ParseCoordinate(args->GetArg(4), radius) || radius <= 0))
	{ WB_LOG(0, "[WarsBots] wb_probe_goal: radius must be positive."); return; }
	if (!self->m_probe.loaded) { self->m_probe.scenario = Probe::Scenario(); self->m_probe.loaded = true; }
	self->m_probe.scenario.hasGoal = true;
	self->m_probe.scenario.goal[0] = position.x; self->m_probe.scenario.goal[1] = position.y; self->m_probe.scenario.goal[2] = position.z;
	self->m_probe.scenario.goalRadius = radius;
	self->m_probeGoalDone = false;
	WB_LOG(0, "[WarsBots] PROBE GOAL set x=%.2f y=%.2f z=%.2f radius=%.2f.", position.x, position.y, position.z, radius);
}

void BotManager::ObserveProbeLine(const char* line)
{
	if (!line || m_probeObserving || !m_probe.Running()) return;
	m_probeObserving = true;
	m_probe.Observe(line);
	m_probeObserving = false;
}

void BotManager::UpdateProbe()
{
	if (!m_probe.Running()) return;
	const double now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	if (m_probe.scenario.hasGoal && !m_probeGoalDone && now >= m_probeNextGoalCheck)
	{
		m_probeNextGoalCheck = float(now) + 0.1f;
		const Vec3 goal(float(m_probe.scenario.goal[0]), float(m_probe.scenario.goal[1]), float(m_probe.scenario.goal[2]));
		for (BotInstance* bot : Ordered())
		{
			CActor* actor = bot->GetActor();
			if (!actor || !actor->GetEntity()) continue;
			const double distance = (actor->GetEntity()->GetWorldPos() - goal).GetLength();
			if (distance <= m_probe.scenario.goalRadius)
			{
				m_probeGoalDone = true;
				WB_LOG(0, "[WarsBots] PROBE GOAL reached actor=%u elapsed=%.2f distance=%.2f.",
					actor->GetEntityId(), now - m_probe.start, distance);
				break;
			}
		}
	}
	if (!m_probe.Due(now)) return;
	const std::string json = m_probe.Finish(now);
	int passed = 0;
	for (const Probe::AssertionResult& result : m_probe.results) if (result.pass) ++passed;
	time_t wall = time(nullptr); char stamp[32] = {0};
	strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", localtime(&wall));
	const std::string name = "probe-result-" + ProbeSafeName(m_probe.scenario.id) + "-" + stamp + ".json";
	const std::filesystem::path file = DataDirectory(L"Logs") / name;
	{
		std::ofstream out(file, std::ios::binary);
		out << json;
		if (!out) WB_LOG(1, "[WarsBots] PROBE RESULT file write failed: %s.", file.string().c_str());
	}
	WB_LOG(0, "[WarsBots] PROBE RESULT id=%s pass=%d assertions=%d/%d file=%s.",
		m_probe.scenario.id.c_str(), passed == int(m_probe.results.size()) ? 1 : 0, passed,
		int(m_probe.results.size()), file.string().c_str());
}

void ProbeObserveLine(const char* line)
{
	if (g_pGame) g_pGame->GetBotManager().ObserveProbeLine(line);
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
		bot = self->Add(hasPosition ? &position : nullptr, BotSkill(clamp_tpl(self->m_config.skill, 0, 3)), nullptr, false);
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

