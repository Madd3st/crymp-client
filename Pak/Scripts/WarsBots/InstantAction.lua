-- WarsBots: instance-local IA integration; native channels and Actor:IsPlayer stay unchanged.
-- Native behavior verified against Crysis 1.2.1 GameData/ZPatch1 and CryMP v26.
WarsBotsIA = WarsBotsIA or {};

function WarsBotsIA:Attach(rules)
    if rules.warsBotsIAAttached then return; end
    if rules.USE_SPAWN_GROUPS then return; end
    local originalScores = rules.ProcessScores;
    local originalDamage = rules.GetDamageTable;
    local originalRevive = rules.ReviveAllPlayers;
    if not originalScores or not originalDamage or not originalRevive then return; end

    local function bot(entity)
        return entity and entity.actor and entity.warsBotParticipant == true;
    end
    local function participant(entity)
        return entity and entity.actor and (entity.actor:IsPlayer() or bot(entity));
    end

    rules.GetDamageTable = function(self, source, target)
        if not bot(source) and not bot(target) then return originalDamage(self,source,target); end
        local sp = participant(source);
        local tp = participant(target);
        if sp then
            if tp then return self.DamagePlayerToPlayer; end
            if target and target.actor then return self.DamagePlayerToAI; end
        elseif source and source.actor then
            if tp then return self.DamageAIToPlayer; end
            if target and target.actor then return self.DamageAIToAI; end
        end
        return originalDamage(self,source,target);
    end

    rules.ProcessScores = function(self, hit)
        local target, shooter = hit.target, hit.shooter;
        if not bot(target) and not bot(shooter) then return originalScores(self,hit); end
        if self:GetState() == "PostGame" then return; end
        local deaths = bot(target) and self:GetPlayerDeaths(target.id);
        local kills = bot(shooter) and self:GetPlayerScore(shooter.id);
        -- Native awards the real human side exactly once. Add only the missing bot side.
        originalScores(self,hit);
        if bot(target) and self:GetPlayerDeaths(target.id) == deaths then self:Award(target,1,0,0); end
        if bot(shooter) and self:GetPlayerScore(shooter.id) == kills then
            if target ~= shooter then self:Award(shooter,0,1,self:IsHeadShot(hit) and 1 or 0);
            else self:Award(shooter,0,-1,0); end
        end
    end

    rules.ReviveAllPlayers = function(self, keepEquip)
        -- Preserve the original human transition logic, but its enumeration is
        -- connection-only in this scope. BotBrain owns the separate bot lifecycle.
        local previous = self.warsBotsConnectionOnly;
        self.warsBotsConnectionOnly = true;
        local ok, message = pcall(originalRevive,self,keepEquip);
        self.warsBotsConnectionOnly = previous;
        if not ok then error(message); end
    end
    rules.warsBotsIAAttached = true;
end
