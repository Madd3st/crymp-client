-- WarsBots: PowerStruggle integration for a channel-0 bot. Native channels and Actor:IsPlayer stay unchanged.
-- Attach once per rules object; Register is called by BotMatch after SetTeam, before the bot's first revive.
WarsBotsPS = WarsBotsPS or {};

function WarsBotsPS:Attach(rules)
    if rules.warsBotsPSAttached then return; end
    self.rules = rules;
    rules.botPlayers = rules.botPlayers or {};
    rules.warsBotsPSAttached = true;
end

function WarsBotsPS:Register(playerId, teamId)
    local rules = self.rules;
    if not rules then return false; end
    local player = System.GetEntity(playerId);
    if not player or not player.actor then return false; end
    rules.botPlayers[playerId] = true;
    player.warsBot = true;

    -- PP/CP/score/revive bookkeeping normally done by PowerStruggle.Server:OnClientConnect. Some of these live in the
    -- stock game scripts, not in our pak: call what exists and log what does not (a missing one must not abort).
    for _, fn in ipairs({"ResetScore", "ResetPP", "ResetCP", "ResetRevive"}) do
        if type(rules[fn]) == "function" then rules[fn](rules, playerId);
        else Log("WarsBots PS Register: rules:%s missing", fn); end
    end
    if type(rules.SetupPlayer) == "function" then rules:SetupPlayer(player);
    else Log("WarsBots PS Register: rules:SetupPlayer missing"); end

    -- Keep the team set by BotInstance::Spawn; the spawn group is what PS RevivePlayer needs with USE_SPAWN_GROUPS.
    local group = rules.game:GetTeamDefaultSpawnGroup(teamId);
    if group and group ~= NULL_ENTITY then
        player.spawnGroupId = group;
        rules:SetPlayerSpawnGroup(playerId, group);
    end
    return true;
end

function WarsBotsPS:IsBot(playerId)
    return self.rules and self.rules.botPlayers and self.rules.botPlayers[playerId] == true;
end
