#pragma once
#include "BotConfig.h"
#include "BotPacePolicy.h"
namespace WarsBots {
inline bool BotPaceEnabled(){const BotConfig& config=BotConfig::Get();return config.registered && config.botPace==1;}
}
