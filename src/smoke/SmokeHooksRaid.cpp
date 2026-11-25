#include "SmokeTestHooks.hpp"
#include "SmokeHooksCommon.hpp"

#include "../entity.hpp"
#include "../game.hpp"
#include "../items.hpp"
#include "../net.hpp"
#include "../player.hpp"
#include "../raid.hpp"
#include "../stat.hpp"
#include "../engine/audio/sound.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace SmokeTestHooks::Raid
{
namespace
{
	constexpr ItemType lootType = TOOL_SKELETONKEY;
	constexpr Sint16 lootBeatitude = 3;

	struct SmokeState
	{
		const int pass = SmokeHooksCommon::parseEnvInt("BARONY_SMOKE_RAID_PASS", 0, 0, 3);
		const std::string sync = SmokeHooksCommon::parseEnvString("BARONY_SMOKE_RAID_SYNC", "");
		bool observed = false;
		bool pickedUp = false;
		bool committed = false;
		bool fixtureFailed = false;
        bool deathObserved = false;
        const bool faults = SmokeHooksCommon::parseEnvInt("BARONY_SMOKE_RAID_FAULTS",0,0,1) != 0;
		Uint32 lootUid = 0;
		Uint32 exitUid = 0;
		Uint32 nextClickTick = 0;
	};

    Uint32 equipmentHash(int player) {
        const Stat& s = *stats[player];
        Item* equipped[] = {s.weapon,s.shield,s.mask,s.helmet,s.gloves,s.shoes,s.breastplate,s.cloak,s.amulet,s.ring};
        Uint32 hash = 2166136261u;
        for (const Item* item : equipped) {
            Uint32 fields[] = {item ? 1u : 0u, item ? static_cast<Uint32>(item->type) : 0u,
                item ? static_cast<Uint32>(item->status) : 0u, item ? item->appearance : 0u,
                item ? static_cast<Uint32>(item->beatitude) : 0u, item ? static_cast<Uint32>(item->count) : 0u};
            for (Uint32 value : fields) { hash ^= value; hash *= 16777619u; }
        }
        return hash;
    }

	int inventoryCount(int player, bool markedOnly)
	{
		int count = 0;
		for ( auto* node = stats[player]->inventory.first; node; node = node->next )
		{
			const auto* item = static_cast<const Item*>(node->element);
			if ( !markedOnly || (item->type == lootType && item->beatitude == lootBeatitude) )
			{
				count += item->count;
			}
		}
		return count;
	}

	int tile(real_t coordinate)
	{
		return static_cast<int>(coordinate / 16.0);
	}

	bool withinReach(const Entity& player, const Entity& target)
	{
		return std::hypot(player.x - target.x, player.y - target.y) <= TOUCHRANGE;
	}

	bool publishTarget(const SmokeState& state, const char* file, Uint32 uid)
	{
		std::ofstream output(state.sync + "/" + file);
		output << uid << '\n';
		output.close();
		return output.good();
	}

	Uint32 readTarget(const SmokeState& state, const char* file)
	{
		std::ifstream input(state.sync + "/" + file);
		Uint32 uid = 0;
		// Read only complete messages from the other process.
		if ( input >> uid && input.peek() == '\n' )
		{
			return uid;
		}
		return 0;
	}

	void clickFromGuest(SmokeState& state, const Entity& guest, Uint32 uid, const char* target)
	{
		if ( uid == 0 || ticks < state.nextClickTick )
		{
			return;
		}
		const auto* entity = uidToEntity(uid);
		if ( !entity || !withinReach(guest, *entity) )
		{
			return;
		}

		// Send the same in-range use packet as actPlayer. The host's item and ladder actors consume it.
		std::memcpy(net_packet->data, "CKIR", 4);
		net_packet->data[4] = clientnum;
		SDLNet_Write32(uid, &net_packet->data[5]);
		net_packet->address = net_server;
		net_packet->len = 9;
		sendPacketSafe(net_sock, -1, net_packet, 0);
		state.nextClickTick = ticks + TICKS_PER_SECOND / 4;
		printlog("[SMOKE][RAID] clicked target=%s uid=%u slot=%d", target, uid, clientnum);
	}

	Entity* findExit()
	{
		for ( auto* node = map.entities->first; node; node = node->next )
		{
			auto* entity = static_cast<Entity*>(node->element);
			if ( entity->behavior == &actLadder && entity->skill[3] == 0 )
			{
				return entity;
			}
		}
		return nullptr;
	}

	bool moveGuestNearExit(Entity& guest, Entity& ladder)
	{
		if ( withinReach(guest, ladder) )
		{
			return true;
		}
		const int ladderX = tile(ladder.x);
		const int ladderY = tile(ladder.y);
		static constexpr int offsets[][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0, 0} };
		for ( const auto& offset : offsets )
		{
			const int x = ladderX + offset[0];
			const int y = ladderY + offset[1];
			if ( x > 0 && y > 0 && x < map.width - 1 && y < map.height - 1 && guest.teleport(x, y) )
			{
				return true;
			}
		}
		return false;
	}

	void tickHost(SmokeState& state, Entity& host, Entity& guest)
	{
        if (!::Raid::hasLoadout(1)) return;
		if ( !state.observed )
		{
			state.observed = true;
            printlog("[SMOKE][RAID] remote_equipment hash=%u ac=%d", equipmentHash(1), AC(stats[1]));
			printlog("[SMOKE][RAID] spawns pass=%d seed=%u host_x=%d host_y=%d guest_x=%d guest_y=%d inventory=%d",
				state.pass, mapseed, tile(host.x), tile(host.y), tile(guest.x), tile(guest.y), inventoryCount(0, false));
			if ( state.pass == 1 )
			{
				Item* item = newItem(lootType, EXCELLENT, lootBeatitude, 1, 0, true, nullptr);
				Entity* dropped = dropItemMonster(item, &host, stats[0]);
				if ( dropped )
				{
					dropped->x = guest.x;
					dropped->y = guest.y;
					dropped->vel_x = 0;
					dropped->vel_y = 0;
					state.lootUid = dropped->getUID();
				}
				if ( !state.lootUid || !publishTarget(state, "loot", state.lootUid) )
				{
					state.fixtureFailed = true;
					printlog("[SMOKE][RAID] fail reason=loot_fixture");
				}
			}
		}

		if (state.pass == 3 && !state.deathObserved) {
            state.deathObserved = true;
            guest.modHP(-stats[1]->HP);
            printlog("[SMOKE][RAID] death_applied slot=1 hp=%d", stats[1]->HP);
        }
        if ( state.pass != 1 || state.fixtureFailed )
		{
			return;
		}
		if ( !state.exitUid && !uidToEntity(state.lootUid) )
		{
			Entity* ladder = findExit();
			if ( !ladder || !moveGuestNearExit(guest, *ladder) || !publishTarget(state, "exit", ladder->getUID()) )
			{
				state.fixtureFailed = true;
				printlog("[SMOKE][RAID] fail reason=exit_fixture");
				return;
			}
			state.exitUid = ladder->getUID();
		}
		if ( !state.committed && ::Raid::isExtracted(1) )
		{
			state.committed = true;
			printlog("[SMOKE][RAID] host_confirmed slot=1");
            if (state.faults) {
                std::memcpy(net_packet->data, "REXT", 4);
                SDLNet_Write32(uniqueGameKey, &net_packet->data[4]);
                net_packet->address = net_clients[0]; net_packet->len = 8;
                sendPacketSafe(net_sock,-1,net_packet,0);
                sendPacketSafe(net_sock,-1,net_packet,0);
                printlog("[SMOKE][RAID] duplicate_approval_sent");
            }
		}
	}

	void tickGuest(SmokeState& state, Entity* guest)
	{
		if ( !state.committed && ::Raid::isExtracted(clientnum) )
		{
			state.committed = true;
			printlog("[SMOKE][RAID] exit_committed pass=%d seed=%u slot=%d", state.pass, mapseed, clientnum);
		}
		if ( !guest || state.committed )
		{
			return;
		}
		if (state.pass == 3 && !state.deathObserved && stats[clientnum]->HP <= 0) {
            state.deathObserved = true;
            printlog("[SMOKE][RAID] death_observed slot=%d hp=%d",clientnum,stats[clientnum]->HP);
        }
        const int lootCount = inventoryCount(clientnum, true);
		if ( !state.observed )
		{
			state.observed = true;
            if (state.pass == 1 && state.faults) {
                std::error_code ec;
                std::filesystem::create_directory("raid-profile.json.tmp", ec);
                std::ofstream("raid-profile.json.tmp/fault-blocker") << "test fixture";
                printlog("[SMOKE][RAID] save_failure_fixture_created");
            }
            printlog("[SMOKE][RAID] local_equipment hash=%u ac=%d", equipmentHash(clientnum), AC(stats[clientnum]));
			printlog("[SMOKE][RAID] guest_inventory pass=%d seed=%u slot=%d x=%d y=%d inventory=%d marked=%d",
				state.pass, mapseed, clientnum, tile(guest->x), tile(guest->y), inventoryCount(clientnum, false), lootCount);
		}
		if ( state.pass == 1 )
		{
			if ( !state.pickedUp && lootCount > 0 )
			{
				state.pickedUp = true;
				printlog("[SMOKE][RAID] guest_picked_up marked=%d", lootCount);
			}
			if ( state.pickedUp )
			{
				clickFromGuest(state, *guest, readTarget(state, "exit"), "exit");
			}
			else
			{
				clickFromGuest(state, *guest, readTarget(state, "loot"), "loot");
			}
		}
	}
}

void tick()
{
#ifdef USE_FMOD
    static const std::string audioSync = SmokeHooksCommon::parseEnvString("BARONY_SMOKE_AUDIO_SYNC", "");
    if (!audioSync.empty() && initialized) {
        const std::string commandPath = audioSync + "/native-audio-command";
        std::ifstream command(commandPath);
        double master = 0, music = 0, gameplay = 0;
        int sound = -1;
        if (command >> master >> music >> gameplay >> sound) {
            command.close();
            std::remove(commandPath.c_str());
            setGlobalVolume(master, music, gameplay, gameplay, gameplay, gameplay);
            FMOD::Channel* channel = sound >= 0 ? playSound(sound, 128) : nullptr;
            bool playing = false;
            float groupVolume = 0;
            if (channel) channel->isPlaying(&playing);
            if (sound_group) sound_group->getVolume(&groupVolume);
            printlog("[NATIVE-AUDIO-TEST] sound=%d playing=%d nosound=%d group=%.3f", sound, playing, no_sound, groupVolume);
        }
    }
#endif
	static SmokeState state;
	if ( state.pass == 0 || state.sync.empty() || !::Raid::active() || intro || currentlevel != 1 )
	{
		return;
	}
	if ( multiplayer == SERVER && players[0] && players[0]->entity && players[1]
		&& !client_disconnected[1] && players[1]->entity )
	{
		tickHost(state, *players[0]->entity, *players[1]->entity);
	}
	else if ( multiplayer == CLIENT && clientnum > 0 && clientnum < MAXPLAYERS && players[clientnum] && stats[clientnum] )
	{
		tickGuest(state, players[clientnum]->entity);
	}
}
}
