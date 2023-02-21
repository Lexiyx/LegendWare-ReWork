// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "local_animations.h"

void local_animations::run(ClientFrameStage_t stage)
{
	if (m_clientstate()->iDeltaTick == -1) return;

	if (!fakelag::get().condition && key_binds::get().get_key_bind_state(20))
	{
		if (stage == FRAME_NET_UPDATE_END)
		{
			fake_server_update = false;

			if (g_ctx.local()->m_flSimulationTime() != fake_simulation_time) //-V550 //-V807
			{
				fake_server_update = true;
				fake_simulation_time = g_ctx.local()->m_flSimulationTime();
			}

			g_ctx.local()->get_animlayers()[3].m_flWeight = 0.0f;
			g_ctx.local()->get_animlayers()[3].m_flCycle = 0.0f;
			g_ctx.local()->get_animlayers()[12].m_flWeight = 0.0f;

			update_fake_animations();
		}
		else if (stage == FRAME_RENDER_START)
		{
			auto animstate = g_ctx.local()->get_animation_state(); //-V807

			if (!animstate)
				return;

			real_server_update = false;

			if (g_ctx.local()->m_flSimulationTime() != real_simulation_time) //-V550
			{
				real_server_update = true;
				real_simulation_time = g_ctx.local()->m_flSimulationTime();
			}

			g_ctx.local()->get_animlayers()[3].m_flWeight = 0.0f;
			g_ctx.local()->get_animlayers()[3].m_flCycle = 0.0f;
			g_ctx.local()->get_animlayers()[12].m_flWeight = 0.0f;

			if (g_cfg.misc.legs_movement == 2)
			{
				static auto alpha = 1.0f;
				static auto switch_alpha = false;

				if (alpha <= 0.0f || alpha >= 1.0f)
					switch_alpha = !switch_alpha;

				alpha += switch_alpha ? 1.75f * m_globals()->m_frametime : -1.75f * m_globals()->m_frametime;
				alpha = math::clamp(alpha, 0.0f, 1.0f);

				if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
				{
					if (switch_alpha) 
					{
						g_ctx.local()->m_flPoseParameter()[0] = -1.f;
						g_ctx.local()->m_flPoseParameter()[1] = 1.f;
					}
					else if (!switch_alpha) 
					{
						g_ctx.local()->m_flPoseParameter()[0] = 1.f;
						g_ctx.local()->m_flPoseParameter()[1] = -1.f;
					}
				}
			}
			update_local_animations(animstate);

			if (g_ctx.local()->is_alive())
			{
				if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
					animstate->m_bOnGround = true;
			}

			if (g_cfg.misc.legs_movement == 2)
			{
				if (!(g_ctx.local()->m_fFlags() & FL_ONGROUND) && animstate->m_bInHitGroundAnimation)
				{
					g_ctx.local()->m_flPoseParameter()[6] = 1;
					g_ctx.local()->m_flPoseParameter()[3] = 1;
					g_ctx.local()->m_flPoseParameter()[7] = 1;
				}
			}
		}
	}
	else if (stage == FRAME_RENDER_START)
	{
		auto animstate = g_ctx.local()->get_animation_state(); //-V807

		if (!animstate)
			return;

		real_server_update = false;
		fake_server_update = false;

		if (g_ctx.local()->m_flSimulationTime() != real_simulation_time || g_ctx.local()->m_flSimulationTime() != fake_simulation_time) //-V550
		{
			real_server_update = fake_server_update = true;
			real_simulation_time = fake_simulation_time = g_ctx.local()->m_flSimulationTime();
		}

		g_ctx.local()->get_animlayers()[3].m_flWeight = 0.0f;
		g_ctx.local()->get_animlayers()[3].m_flCycle = 0.0f;
		g_ctx.local()->get_animlayers()[12].m_flWeight = 0.0f;

		if (g_cfg.misc.legs_movement == 2)
		{
			static auto alpha = 1.0f;
			static auto switch_alpha = false;

			if (alpha <= 0.0f || alpha >= 1.0f)
				switch_alpha = !switch_alpha;

			alpha += switch_alpha ? 1.75f * m_globals()->m_frametime : -1.75f * m_globals()->m_frametime;
			alpha = math::clamp(alpha, 0.0f, 1.0f);

			if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
			{
				if (switch_alpha) 
				{
					g_ctx.local()->m_flPoseParameter()[0] = -1.f;
					g_ctx.local()->m_flPoseParameter()[1] = 1.f;
				}
				else if (!switch_alpha) 
				{
					g_ctx.local()->m_flPoseParameter()[0] = 1.f;
					g_ctx.local()->m_flPoseParameter()[1] = -1.f;
				}
			}
		}

		update_fake_animations();
		update_local_animations(animstate);

		if (g_cfg.misc.legs_movement == 2)
		{
			if (!(g_ctx.local()->m_fFlags() & FL_ONGROUND) && animstate->m_bInHitGroundAnimation)
			{
				g_ctx.local()->m_flPoseParameter()[6] = 1;
				g_ctx.local()->m_flPoseParameter()[3] = 1;
				g_ctx.local()->m_flPoseParameter()[7] = 1;
			}
		}

		if (g_ctx.local()->is_alive())
		{
			if (g_ctx.local()->m_fFlags() & FL_ONGROUND)
				animstate->m_bOnGround = true;
		}

		if (g_cfg.antiaim.static_legs)
			g_ctx.local()->m_flPoseParameter()[6] = 1; // static legs | joke?
	}
}

void local_animations::update_fake_animations()
{
	auto alloc = !local_data.animstate;
	auto change = !alloc && handle != &g_ctx.local()->GetRefEHandle(); //-V807
	auto reset = !alloc && !change && g_ctx.local()->m_flSpawnTime() != spawntime; //-V550

	if (change)
		m_memalloc()->Free(local_data.animstate);

	if (reset)
	{
		util::reset_state(local_data.animstate);
		spawntime = g_ctx.local()->m_flSpawnTime();
	}

	if (alloc || change)
	{
		local_data.animstate = (c_baseplayeranimationstate*)m_memalloc()->Alloc(sizeof(c_baseplayeranimationstate));

		if (local_data.animstate)
			util::create_state(local_data.animstate, g_ctx.local());

		handle = (CBaseHandle*)&g_ctx.local()->GetRefEHandle();
		spawntime = g_ctx.local()->m_flSpawnTime();
	}

	if (!alloc && !change && !reset && fake_server_update)
	{
		float pose_parameter[24]; //-V688
		memcpy(pose_parameter, &g_ctx.local()->m_flPoseParameter(), 24 * sizeof(float));

		AnimationLayer layers[13]; //-V688
		memcpy(layers, g_ctx.local()->get_animlayers(), g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));

		auto backup_frametime = m_globals()->m_frametime; //-V807
		auto backup_curtime = m_globals()->m_curtime;

		m_globals()->m_frametime = m_globals()->m_intervalpertick;
		m_globals()->m_curtime = g_ctx.local()->m_flSimulationTime();

		local_data.animstate->m_pBaseEntity = g_ctx.local();
		util::update_state(local_data.animstate, local_animations::get().local_data.fake_angles);

		local_data.animstate->m_bInHitGroundAnimation = false;
		local_data.animstate->m_fLandingDuckAdditiveSomething = 0.0f;
		local_data.animstate->m_flHeadHeightOrOffsetFromHittingGroundAnimation = 1.0f;

		g_ctx.local()->invalidate_bone_cache();

		g_ctx.local()->setup_bones_rebuilt(g_ctx.globals.fake_matrix, BONE_USED_BY_ANYTHING);
		local_data.visualize_lag = g_cfg.player.visualize_lag;

		if (!local_data.visualize_lag)
		{
			for (auto& i : g_ctx.globals.fake_matrix)
			{
				i[0][3] -= g_ctx.local()->GetRenderOrigin().x; //-V807
				i[1][3] -= g_ctx.local()->GetRenderOrigin().y;
				i[2][3] -= g_ctx.local()->GetRenderOrigin().z;
			}
		}

		m_globals()->m_frametime = backup_frametime;
		m_globals()->m_curtime = backup_curtime;

		memcpy(&g_ctx.local()->m_flPoseParameter(), pose_parameter, 24 * sizeof(float));
		memcpy(g_ctx.local()->get_animlayers(), layers, g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));
	}
}

void local_animations::update_local_animations(c_baseplayeranimationstate* animstate)
{
	if (tickcount != m_globals()->m_tickcount) //-V550
	{
		memcpy(layers, g_ctx.local()->get_animlayers(), g_ctx.local()->animlayer_count() * sizeof(AnimationLayer)); //-V807

		//if (local_data.animstate)
		//	animstate->m_fDuckAmount = local_data.animstate->m_fDuckAmount;

		animstate->m_iLastClientSideAnimationUpdateFramecount = 0;
		util::update_state(animstate, local_animations::get().local_data.fake_angles);
		g_ctx.local()->update_clientside_animation();

		if (real_server_update)
		{
			abs_angles = animstate->m_flGoalFeetYaw;
			memcpy(pose_parameter, &g_ctx.local()->m_flPoseParameter(), 24 * sizeof(float));
		}
	}
	else
		animstate->m_iLastClientSideAnimationUpdateFramecount = m_globals()->m_framecount;

	animstate->m_flGoalFeetYaw = antiaim::get().condition(g_ctx.get_command()) ? abs_angles : local_animations::get().local_data.real_angles.y;

	// fix feet spin.
	animstate->m_flFeetYawRate = 0.f;

	g_ctx.local()->set_abs_angles(Vector(0.0f, abs_angles, 0.0f));
	memcpy(g_ctx.local()->get_animlayers(), layers, g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));
	memcpy(&g_ctx.local()->m_flPoseParameter(), pose_parameter, 24 * sizeof(float));
}

void local_animations::update_prediction_animations()
{
	auto alloc = !local_data.prediction_animstate;
	auto change = !alloc && handle != &g_ctx.local()->GetRefEHandle();
	auto reset = !alloc && !change && g_ctx.local()->m_flSpawnTime() != spawntime;

	if (change)
		m_memalloc()->Free(local_data.prediction_animstate);

	if (reset)
	{
		util::reset_state(local_data.prediction_animstate);
		spawntime = g_ctx.local()->m_flSpawnTime();
	}

	if (alloc || change)
	{
		local_data.prediction_animstate = (c_baseplayeranimationstate*)m_memalloc()->Alloc(sizeof(c_baseplayeranimationstate));

		if (local_data.prediction_animstate)
			util::create_state(local_data.prediction_animstate, g_ctx.local());

		handle = (CBaseHandle*)&g_ctx.local()->GetRefEHandle();
		spawntime = g_ctx.local()->m_flSpawnTime();
	}

	if (!alloc && !change && !reset)
	{
		float pose_parameter[24];
		memcpy(pose_parameter, &g_ctx.local()->m_flPoseParameter(), 24 * sizeof(float));

		AnimationLayer layers[13];
		memcpy(layers, g_ctx.local()->get_animlayers(), g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));

		local_data.prediction_animstate->m_pBaseEntity = g_ctx.local();
		util::update_state(local_data.prediction_animstate, ZERO);

		g_ctx.local()->setup_bones_rebuilt(g_ctx.globals.prediction_matrix, BONE_USED_BY_HITBOX);

		memcpy(&g_ctx.local()->m_flPoseParameter(), pose_parameter, 24 * sizeof(float));
		memcpy(g_ctx.local()->get_animlayers(), layers, g_ctx.local()->animlayer_count() * sizeof(AnimationLayer));
	}
}