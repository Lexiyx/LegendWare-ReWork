// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "..\hooks.hpp"
#include "..\..\cheats\misc\prediction_system.h"
#include "..\..\cheats\lagcompensation\local_animations.h"
#include "..\..\cheats\misc\misc.h"
#include "..\..\cheats\misc\logs.h"
#undef max

void FixRevolver()
{
	auto weapon = g_ctx.local()->m_hActiveWeapon().Get();
	if (!weapon)
		return;

	if (weapon->m_iItemDefinitionIndex() == 64 && weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER && weapon->m_flNextSecondaryAttack() == FLT_MAX)
		weapon->m_flNextSecondaryAttack() = g_ctx.local()->m_hActiveWeapon().Get()->m_flNextSecondaryAttack();
};

using RunCommand_t = void(__thiscall*)(void*, player_t*, CUserCmd*, IMoveHelper*);

void __fastcall hooks::hooked_runcommand(void* ecx, void* edx, player_t* player, CUserCmd* m_pcmd, IMoveHelper* move_helper)
{
	static auto original_fn = prediction_hook->get_func_address <RunCommand_t>(19);
	g_ctx.local((player_t*)m_entitylist()->GetClientEntity(m_engine()->GetLocalPlayer()), true);

	if (!m_pcmd || !player || player != g_ctx.local() || !player->is_alive())
		return original_fn(ecx, player, m_pcmd, move_helper);

	// airstuck jitter / overpred fix.
	if (m_pcmd->m_tickcount >= std::numeric_limits<int>::max())
		return;

	/* predict cmd */
	static auto m_tickrate = (int)std::round(1.f / m_globals()->m_intervalpertick);
	static auto SV_MAX_USERCMD_FUTURE_TICKS = m_cvar()->FindVar("sv_max_usercmd_future_ticks")->GetInt();
	if (m_pcmd->m_tickcount > m_globals()->m_tickcount + (m_tickrate + SV_MAX_USERCMD_FUTURE_TICKS)) //-V807
	{
		m_pcmd->m_predicted = true;
		player->set_abs_origin(player->m_vecOrigin());

		// это в кл мув
		//int nCorrectedTick = TIME_TO_TICKS(0.03f) - (13 + m_clientstate()->iChokedCommands) + m_globals()->m_tickcount + engineprediction::get().m_last_cmd_delta + 1;
		//g_ctx.local()->m_nTickBase() = nCorrectedTick;

		if (m_globals()->m_frametime > 0.0f && !m_prediction()->EnginePaused)
			++player->m_nTickBase();


		return;
	}

	// patch attack packet before running command
	engineprediction::get().patch_attack_packet(m_pcmd, true);

	auto weapon = player->m_hActiveWeapon().Get();
	if (weapon)
	{
		static float tickbase_records[MULTIPLAYER_BACKUP];
		static bool in_attack[MULTIPLAYER_BACKUP];
		static bool can_shoot[MULTIPLAYER_BACKUP];

		tickbase_records[m_pcmd->m_command_number % MULTIPLAYER_BACKUP] = player->m_nTickBase();
		in_attack[m_pcmd->m_command_number % MULTIPLAYER_BACKUP] = m_pcmd->m_buttons & IN_ATTACK || m_pcmd->m_buttons & IN_ATTACK2;
		can_shoot[m_pcmd->m_command_number % MULTIPLAYER_BACKUP] = weapon->can_fire(false);

		if (weapon->m_iItemDefinitionIndex() == WEAPON_REVOLVER)
		{
			auto postpone_fire_ready_time = FLT_MAX;
			auto tickrate = (int)(1.0f / m_globals()->m_intervalpertick);

			if (tickrate >> 1 > 1)
			{
				auto command_number = m_pcmd->m_command_number - 1;
				auto shoot_number = 0;

				for (auto i = 1; i < tickrate >> 1; ++i)
				{
					shoot_number = command_number;

					if (!in_attack[command_number % MULTIPLAYER_BACKUP] || !can_shoot[command_number % MULTIPLAYER_BACKUP])
						break;

					--command_number;
				}

				if (shoot_number)
				{
					auto tick = 1 - (int)(-0.03348f / m_globals()->m_intervalpertick);

					if (m_pcmd->m_command_number - shoot_number >= tick)
						postpone_fire_ready_time = TICKS_TO_TIME(tickbase_records[(tick + shoot_number) % MULTIPLAYER_BACKUP]) + 0.2f;
				}
			}

			weapon->m_flPostponeFireReadyTime() = postpone_fire_ready_time;
		}
	}

	// этот фикс из onetap.su v2 я захуярил в физик симулейте.
	auto m_flVelModBackup = player->m_flVelocityModifier();
	//if (g_inputpred.Misc.m_bOverrideModifier && cmd->m_command_number == (g_csgo.m_cl->m_command_ack + 1))
	//	player->m_flVelocityModifier() = g_inputpred.m_stored_variables.m_flVelocityModifier;
	auto backup_velocity_modifier = player->m_flVelocityModifier();
	player->m_flVelocityModifier() = g_ctx.globals.last_velocity_modifier;
	original_fn(ecx, player, m_pcmd, move_helper);

	if (!g_ctx.globals.in_createmove)
		player->m_flVelocityModifier() = backup_velocity_modifier;

	// we predicted patch again
	engineprediction::get().patch_attack_packet(m_pcmd, false);
}

using InPrediction_t = bool(__thiscall*)(void*);

bool __stdcall hooks::hooked_inprediction()
{
	static auto original_fn = prediction_hook->get_func_address <InPrediction_t>(16);
	static auto maintain_sequence_transitions = (void*)util::FindSignature(crypt_str("client.dll"), crypt_str("84 C0 74 17 8B 87"));
	static auto setupbones_timing = (void*)util::FindSignature(crypt_str("client.dll"), crypt_str("84 C0 74 0A F3 0F 10 05 ? ? ? ? EB 05"));
	static void* calcplayerview_return = (void*)util::FindSignature(crypt_str("client.dll"), crypt_str("84 C0 75 0B 8B 0D ? ? ? ? 8B 01 FF 50 4C"));

	if (maintain_sequence_transitions && g_ctx.globals.setuping_bones && _ReturnAddress() == maintain_sequence_transitions)
		return true;

	if (setupbones_timing && _ReturnAddress() == setupbones_timing)
		return false;

	if (m_engine()->IsInGame()) {
		if (_ReturnAddress() == calcplayerview_return)
			return true;

	}

	return original_fn(m_prediction());
}

typedef void(__cdecl* clMove_fn)(float, bool);

void __cdecl hooks::Hooked_CLMove(float flAccumulatedExtraSamples, bool bFinalTick)
{
	
	

		if (g_ctx.globals.startcharge && g_ctx.globals.tocharge < g_ctx.globals.tochargeamount)
		{
			g_ctx.globals.tocharge++;
			//g_ctx.globals.ticks_allowed = g_ctx.globals.tocharge;
			return;
		}

		(clMove_fn(hooks::original_clmove)(flAccumulatedExtraSamples, bFinalTick));

		g_ctx.globals.isshifting = true;
		{
			for (g_ctx.globals.shift_ticks = min(g_ctx.globals.tocharge, g_ctx.globals.shift_ticks); g_ctx.globals.shift_ticks > 0; g_ctx.globals.shift_ticks--, g_ctx.globals.tocharge--)
				(clMove_fn(hooks::original_clmove)(flAccumulatedExtraSamples, bFinalTick));
		}
		g_ctx.globals.isshifting = false;
	
}


using WriteUsercmdDeltaToBuffer_t = bool(__thiscall*)(void*, int, void*, int, int, bool);
void WriteUserCmd(void* buf, CUserCmd* incmd, CUserCmd* outcmd);

void WriteUserCmd(void* buf, CUserCmd* incmd, CUserCmd* outcmd)
{
	using WriteUserCmd_t = void(__fastcall*)(void*, CUserCmd*, CUserCmd*);
	static auto Fn = (WriteUserCmd_t)util::FindSignature(crypt_str("client.dll"), crypt_str("55 8B EC 83 E4 F8 51 53 56 8B D9"));

	__asm
	{
		mov     ecx, buf
		mov     edx, incmd
		push    outcmd
		call    Fn
		add     esp, 4
	}
}

bool __fastcall hooks::hooked_writeusercmddeltatobuffer(void* ecx, void* edx, int slot, bf_write* buf, int from, int to, bool is_new_command)
{
	static auto original_fn = client_hook->get_func_address <WriteUsercmdDeltaToBuffer_t>(24);

	if (!g_ctx.globals.tickbase_shift)
		return original_fn(ecx, slot, buf, from, to, is_new_command);

	if (from != -1)
		return true;

	auto final_from = -1;

	uintptr_t frame_ptr;
	__asm mov frame_ptr, ebp;

	auto backup_commands = reinterpret_cast <int*> (frame_ptr + 0xFD8);
	auto new_commands = reinterpret_cast <int*> (frame_ptr + 0xFDC);

	auto newcmds = *new_commands;
	auto shift = g_ctx.globals.tickbase_shift;

	g_ctx.globals.tickbase_shift = 0;
	*backup_commands = 0;

	auto choked_modifier = newcmds + shift;

	if (choked_modifier > 62)
		choked_modifier = 62;

	*new_commands = choked_modifier;

	auto next_cmdnr = m_clientstate()->iChokedCommands + m_clientstate()->nLastOutgoingCommand + 1;
	auto final_to = next_cmdnr - newcmds + 1;

	if (final_to <= next_cmdnr)
	{
		while (original_fn(ecx, slot, buf, final_from, final_to, true))
		{
			final_from = final_to++;

			if (final_to > next_cmdnr)
				goto next_cmd;
		}

		return false;
	}
next_cmd:

	auto user_cmd = m_input()->GetUserCmd(final_from);

	if (!user_cmd)
		return true;

	CUserCmd to_cmd;
	CUserCmd from_cmd;

	from_cmd = *user_cmd;
	to_cmd = from_cmd;

	to_cmd.m_command_number++;


	to_cmd.m_tickcount += ((int)(1.0f / m_globals()->m_intervalpertick) * 3);

	if (newcmds > choked_modifier)
		return true;

	for (auto i = choked_modifier - newcmds + 1; i > 0; --i)
	{
		WriteUserCmd(buf, &to_cmd, &from_cmd);

		from_cmd = to_cmd;
		to_cmd.m_command_number++;
		to_cmd.m_tickcount++;
	}
	uintptr_t stackbase;
	__asm mov stackbase, ebp;

	auto m_pnNewCmds = reinterpret_cast <int*> (stackbase + 0xFCC);

	int m_nNewCmds = *m_pnNewCmds;

	int m_nTickbase = g_ctx.globals.tickbase_shift;

	int m_nTotalNewCmds = min(m_nNewCmds + abs(m_nTickbase), 62);

	if (from != -1)
		return true;

	int m_nNewCommands = m_nTotalNewCmds;
	int m_nBackupCommands = 0;
	int m_nNextCmd = m_clientstate()->nLastOutgoingCommand + m_clientstate()->iChokedCommands + 1;

	if (to > m_nNextCmd)
	{
	Run:
		CUserCmd* m_pCmd = m_input()->GetUserCmd(from);
		if (m_pCmd)
		{
			CUserCmd m_nToCmd = *m_pCmd, m_nFromCmd = *m_pCmd;
			m_nToCmd.m_command_number++;
			m_nToCmd.m_tickcount += m_globals()->m_tickcount + 2 * m_globals()->m_tickcount;
			for (int i = m_nNewCmds; i <= m_nTotalNewCmds; i++)
			{
				int m_shift = m_nTotalNewCmds - m_nNewCmds + 1;

				do
				{
					m_nFromCmd.m_buttons = m_nToCmd.m_buttons;
					m_nFromCmd.m_viewangles.x = m_nToCmd.m_viewangles.x;
					m_nFromCmd.m_impulse = m_nToCmd.m_impulse;
					m_nFromCmd.m_weaponselect = m_nToCmd.m_weaponselect;
					m_nFromCmd.m_aimdirection.y = m_nToCmd.m_aimdirection.y;
					m_nFromCmd.m_weaponsubtype = m_nToCmd.m_weaponsubtype;
					m_nFromCmd.m_upmove = m_nToCmd.m_upmove;
					m_nFromCmd.m_random_seed = m_nToCmd.m_random_seed;
					m_nFromCmd.m_mousedx = m_nToCmd.m_mousedx;
					m_nFromCmd.pad_0x4C[3] = m_nToCmd.pad_0x4C[3];
					m_nFromCmd.m_command_number = m_nToCmd.m_command_number;
					m_nFromCmd.m_tickcount = m_nToCmd.m_tickcount;
					m_nFromCmd.m_mousedy = m_nToCmd.m_mousedy;
					m_nFromCmd.pad_0x4C[19] = m_nToCmd.pad_0x4C[19];
					m_nFromCmd.m_predicted = m_nToCmd.m_predicted;
					m_nFromCmd.pad_0x4C[23] = m_nToCmd.pad_0x4C[23];
					m_nToCmd.m_command_number++;
					m_nToCmd.m_tickcount++;
					--m_shift;
				} while (m_shift);
			}
			return true;
		}
	}
	return true;
}


/*using SendNetMsgFn = bool(__thiscall*)(void*, void*, bool, bool);

bool __fastcall hooks::hooked_net_message(void* netchan, void* edx, void* msg, bool bForceReliable, bool bVoice)
{
	static auto original_fn = hooks::netchannel_hook->get_func_address<SendNetMsgFn>(40);
	return original_fn(netchan, msg, bForceReliable, bVoice);
}
*/