// Wire compatibility (owner 26.10 12:04: the WarsBots client got "Malformed packet" on a public CryMP server).
// WarsBots adds fields to the player stream (spawn info `warsBotActor`, optional groups WarsBotSuitActive and
// WarsBotMotion in eEA_GameServerDynamic). A vanilla server never writes them, so the client misreads the stream.
// The extensions are therefore used only when both ends are WarsBots: `wb_net_ext` -1 auto (server: on; client: on
// only for a loopback/private server address, i.e. the owner's local WarsBots server), 0 vanilla wire, 1 force on.
// The value must not change during a connection (CryNetwork caches aspect layouts): set it before connecting.
// Pure logic, no engine types; Tests/BotNetCompatTests.cpp.
#pragma once
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace WarsBots {

// "ip:port", "ip", "[::1]:port", "localhost:port", "Magic8-PC:46040" (C-106: CryNetwork names a LAN channel by the
// machine name). A single-label host name (no dot) is a LAN/NetBIOS name; public servers are IPs or dotted names.
// Unknown or empty -> false (vanilla wire is the safe default).
inline bool IsPrivateAddress(const char* name)
{
	if (!name || !*name) return false;
	char h[64]; size_t n = 0;
	const char* p = name;
	if (*p == '[') { ++p; while (*p && *p != ']' && n + 1 < sizeof(h)) h[n++] = char(std::tolower((unsigned char)*p++)); }
	else
	{
		const char* colon = std::strchr(p, ':');
		const bool v6 = colon && std::strchr(colon + 1, ':');   // bare IPv6 without brackets
		while (*p && (v6 || *p != ':') && n + 1 < sizeof(h)) h[n++] = char(std::tolower((unsigned char)*p++));
	}
	h[n] = 0;
	if (!std::strcmp(h, "localhost") || !std::strcmp(h, "::1")) return true;
	if (std::isalpha((unsigned char)h[0]) && !std::strchr(h, '.') && !std::strchr(h, ':') && !std::strchr(h, '<'))
	{
		for (const char* c = h; *c; ++c) if (!std::isalnum((unsigned char)*c) && *c != '-' && *c != '_') return false;
		return true;
	}
	int a[4]; const char* s = h;
	for (int i = 0; i < 4; ++i)
	{
		if (!std::isdigit((unsigned char)*s)) return false;
		char* e; long v = std::strtol(s, &e, 10);
		if (v < 0 || v > 255 || e == s) return false;
		a[i] = int(v); s = e;
		if (i < 3) { if (*s != '.') return false; ++s; }
	}
	if (*s) return false;
	return a[0] == 127 || a[0] == 10 || (a[0] == 192 && a[1] == 168) || (a[0] == 172 && a[1] >= 16 && a[1] <= 31);
}

// mode: wb_net_ext; isServer: gEnv->bServer (listen or dedicated); serverAddress: the client channel name.
inline bool WireExtensions(int mode, bool isServer, const char* serverAddress)
{
	if (mode == 0) return false;
	if (mode > 0) return true;
	return isServer || IsPrivateAddress(serverAddress);
}

} // namespace WarsBots
