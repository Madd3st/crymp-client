// Tests for BotNetCompat.h (owner 26.10: "Malformed packet" on a public server with the WarsBots client).
#include "CryGame/Bots/BotNetCompat.h"
#include <cassert>
#include <cstdio>
using namespace WarsBots;
int main()
{
	// Oracle: the owner's failing case — a public server must get the vanilla wire in auto mode.
	assert(!WireExtensions(-1, false, "193.165.100.242:11522"));
	// Local WarsBots servers keep the extensions.
	assert(WireExtensions(-1, false, "127.0.0.1:64087"));
	assert(WireExtensions(-1, false, "192.168.1.20:64087"));
	assert(WireExtensions(-1, false, "10.0.0.5"));
	assert(WireExtensions(-1, false, "172.20.1.1:1"));
	assert(!WireExtensions(-1, false, "172.32.1.1:1"));
	assert(WireExtensions(-1, false, "localhost:64087"));
	assert(WireExtensions(-1, false, "[::1]:64087"));
	assert(WireExtensions(-1, false, "::1"));
	// Unknown / garbage -> vanilla (safe).
	assert(!WireExtensions(-1, false, nullptr));
	assert(!WireExtensions(-1, false, ""));
	assert(!WireExtensions(-1, false, "<local>"));
	assert(!WireExtensions(-1, false, "127.0.0.1x:5"));
	assert(!WireExtensions(-1, false, "300.1.1.1:5"));
	// Server side and overrides.
	assert(WireExtensions(-1, true, nullptr));
	assert(!WireExtensions(0, true, "127.0.0.1:1"));
	assert(WireExtensions(1, false, "193.165.100.242:11522"));
	std::puts("BotNetCompatTests OK");
	return 0;
}
