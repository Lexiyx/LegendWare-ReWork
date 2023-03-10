#include "animation_system.h"
#include "..\ragebot\aim.h"
#include "../visuals/player_esp.h"

/*RESOLVER BY LAITH*/
void CResolver::initialize(player_t* e, adjust_data* record, const float& goal_feet_yaw, const float& pitch)
{
	player = e;
	player_record = record;

	original_pitch = math::normalize_pitch(pitch);
	original_goal_feet_yaw = math::normalize_yaw(goal_feet_yaw);


}

void CResolver::initialize_yaw(player_t* e, adjust_data* record)
{
	player = e;

	player_record = record;

	player_record->left = build_yaw(player, player->m_angEyeAngles().y, 1);
	player_record->right = build_yaw(player, player->m_angEyeAngles().y, 2);
	player_record->middle = build_yaw(player, player->m_angEyeAngles().y, 3);
}

void CResolver::reset()
{
	player = nullptr;
	player_record = nullptr;

	side = false;
	fake = false;

	was_first_bruteforce = false;
	was_second_bruteforce = false;

	original_goal_feet_yaw = 0.0f;
	original_pitch = 0.0f;
}


bool CResolver::IsAdjustingBalance()
{


	for (int i = 0; i < 15; i++)
	{
		const int activity = player->sequence_activity(player_record->layers[i].m_nSequence);
		if (activity == 979)
		{
			return true;
		}
	}
	return false;
}

bool CResolver::is_breaking_lby(AnimationLayer cur_layer, AnimationLayer prev_layer)
{
	if (IsAdjustingBalance())
	{
		if (IsAdjustingBalance())
		{
			if ((prev_layer.m_flCycle != cur_layer.m_flCycle) || cur_layer.m_flWeight == 1.f)
			{
				return true;
			}
			else if (cur_layer.m_flWeight == 0.f && (prev_layer.m_flCycle > 0.92f && cur_layer.m_flCycle > 0.92f))
			{
				return true;
			}
		}
		return false;
	}

	return false;
}

static auto GetSmoothedVelocity = [](float min_delta, Vector a, Vector b) {
	Vector delta = a - b;
	float delta_length = delta.Length();

	if (delta_length <= min_delta)
	{
		Vector result;

		if (-min_delta <= delta_length)
			return a;
		else
		{
			float iradius = 1.0f / (delta_length + FLT_EPSILON);
			return b - ((delta * iradius) * min_delta);
		}
	}
	else
	{
		float iradius = 1.0f / (delta_length + FLT_EPSILON);
		return b + ((delta * iradius) * min_delta);
	}
};

inline float anglemod(float a)
{
	a = (360.f / 65536) * ((int)(a * (65536.f / 360.0f)) & 65535);
	return a;
}

float ApproachAngle(float target, float value, float speed)
{
	target = anglemod(target);
	value = anglemod(value);

	float delta = target - value;

	if (speed < 0)
		speed = -speed;

	if (delta < -180)
		delta += 360;
	else if (delta > 180)
		delta -= 360;

	if (delta > speed)
		value += speed;
	else if (delta < -speed)
		value -= speed;
	else
		value = target;

	return value;
}

float CResolver::build_yaw(player_t* player, float angle, int n)
{
	auto animState = player->get_animation_state();
	float m_flFakeGoalFeetYaw = 0.0f;
	// Rebuild setup velocity to receive flMinBodyYaw and flMaxBodyYaw
	Vector velocity = player->m_vecVelocity();
	float spd = velocity.LengthSqr();
	if (spd > std::powf(1.2f * 260.0f, 2.f)) {
		Vector velocity_normalized = velocity.Normalized();
		velocity = velocity_normalized * (1.2f * 260.0f);
	}

	float m_flChokedTime = animState->m_flLastClientSideAnimationUpdateTime;
	float v25 = math::clamp(player->m_flDuckAmount() + animState->m_fLandingDuckAdditiveSomething, 0.0f, 1.0f);
	float v26 = animState->m_fDuckAmount;
	float v27 = m_flChokedTime * 6.0f;
	float v28;

	// clamp
	if ((v25 - v26) <= v27) {
		if (-v27 <= (v25 - v26))
			v28 = v25;
		else
			v28 = v26 - v27;
	}
	else {
		v28 = v26 + v27;
	}

	float flDuckAmount = math::clamp(v28, 0.0f, 1.0f);

	Vector animationVelocity = GetSmoothedVelocity(m_flChokedTime * 2000.0f, velocity, player->m_vecVelocity());
	float speed = std::fminf(animationVelocity.Length(), 260.0f);

	auto weapon = player->m_hActiveWeapon();

	float flMaxMovementSpeed = 260.0f;
	if (weapon && weapon->get_csweapon_info()) {
		flMaxMovementSpeed = std::fmaxf(weapon->get_csweapon_info()->flMaxPlayerSpeed, 0.001f);
	}

	float flRunningSpeed = speed / (flMaxMovementSpeed * 0.520f);
	float flDuckingSpeed = speed / (flMaxMovementSpeed * 0.340f);

	flRunningSpeed = math::clamp(flRunningSpeed, 0.0f, 1.0f);

	float flYawModifier = (((animState->m_flStopToFullRunningFraction * -0.3f) - 0.2f) * flRunningSpeed) + 1.0f;
	if (flDuckAmount > 0.0f) {
		float flDuckingSpeed = math::clamp(flDuckingSpeed, 0.0f, 1.0f);
		flYawModifier += (flDuckAmount * flDuckingSpeed) * (0.5f - flYawModifier);
	}

	auto test = animState->yaw_desync_adjustment();


	float flMinBodyYaw = -58.f * flYawModifier;
	float flMaxBodyYaw = 58.f * flYawModifier;


	//float flMinBodyYaw = (*(float*)(uintptr_t(animState) + 0x330) * flYawModifier);
	//float flMaxBodyYaw = (*(float*)(uintptr_t(animState) + 0x334) * flYawModifier);

	float flEyeYaw = angle;
	float flEyeDiff = std::remainderf(flEyeYaw - m_flFakeGoalFeetYaw, 360.f);

	if (flEyeDiff <= flMaxBodyYaw) {
		if (flMinBodyYaw > flEyeDiff)
			m_flFakeGoalFeetYaw = fabs(flMinBodyYaw) + flEyeYaw;
	}
	else {
		m_flFakeGoalFeetYaw = flEyeYaw - fabs(flMaxBodyYaw);
	}

	m_flFakeGoalFeetYaw = std::remainderf(m_flFakeGoalFeetYaw, 360.f);

	if (speed > 0.1f || fabs(velocity.z) > 100.0f) {
		m_flFakeGoalFeetYaw = ApproachAngle(
			flEyeYaw,
			m_flFakeGoalFeetYaw,
			((animState->m_flStopToFullRunningFraction * 20.0f) + 30.0f)
			* m_flChokedTime);
	}
	else {
		m_flFakeGoalFeetYaw = ApproachAngle(
			player->m_flLowerBodyYawTarget(),
			m_flFakeGoalFeetYaw,
			m_flChokedTime * 100.0f);
	}

	float Left = flMinBodyYaw;
	float Right = flMaxBodyYaw;
	float middle = m_flFakeGoalFeetYaw;

	math::normalize_yaw(m_flFakeGoalFeetYaw);

	if (n == 1)
		return Left;
	else if (n == 2)
		return Right;
	else if (n == 3)
		return m_flFakeGoalFeetYaw;
}



bool CResolver::is_slow_walking()
{
	auto entity = player;
	//float large = 0;
	float velocity_2D[64], old_velocity_2D[64];
	if (entity->m_vecVelocity().Length2D() != velocity_2D[entity->EntIndex()] && entity->m_vecVelocity().Length2D() != NULL) {
		old_velocity_2D[entity->EntIndex()] = velocity_2D[entity->EntIndex()];
		velocity_2D[entity->EntIndex()] = entity->m_vecVelocity().Length2D();
	}
	//if (large == 0)return false;
	Vector velocity = entity->m_vecVelocity();
	Vector direction = entity->m_angEyeAngles();

	float speed = velocity.Length();
	direction.y = entity->m_angEyeAngles().y - direction.y;
	//method 1
	if (velocity_2D[entity->EntIndex()] > 1) {
		int tick_counter[64];
		if (velocity_2D[entity->EntIndex()] == old_velocity_2D[entity->EntIndex()])
			tick_counter[entity->EntIndex()] += 1;
		else
			tick_counter[entity->EntIndex()] = 0;

		while (tick_counter[entity->EntIndex()] > (1 / m_globals()->m_intervalpertick) * fabsf(0.1f))//should give use 100ms in ticks if their speed stays the same for that long they are definetely up to something..
			return true;
	}


	return false;
}

void CResolver::layer_test()
{
	player_record->type = LAYERS;



	
		float center = (abs(player_record->layers[6].m_flPlaybackRate - resolver_layers[0][6].m_flPlaybackRate)) * 1000.f;
		float positive_full = (abs(player_record->layers[6].m_flPlaybackRate - resolver_layers[1][6].m_flPlaybackRate)) * 1000.f;
		float negative_full = (abs(player_record->layers[6].m_flPlaybackRate - resolver_layers[2][6].m_flPlaybackRate)) * 1000.f;
		float positive_40 = (abs(player_record->layers[6].m_flPlaybackRate - resolver_layers[3][6].m_flPlaybackRate)) * 1000.f;
		float negative_40 = (abs(player_record->layers[6].m_flPlaybackRate - resolver_layers[4][6].m_flPlaybackRate)) * 1000.f;
		float positive_20 = (abs(player_record->layers[6].m_flPlaybackRate - resolver_layers[5][6].m_flPlaybackRate)) * 1000.f;
		float negative_20 = (abs(player_record->layers[6].m_flPlaybackRate - resolver_layers[6][6].m_flPlaybackRate)) * 1000.f;

		if ((positive_full > center && negative_full <= center) || (positive_40 > center && negative_40 <= center) || (positive_20 > center && negative_20 <= center))
		{
			if (positive_full > positive_40 && positive_full > positive_20)
				player_record->side = RESOLVER_SECOND;
			else if (positive_40 > positive_full && positive_40 > positive_20)
				player_record->side = RESOLVER_LOW_SECOND;
			else if (positive_20 > positive_full && positive_20 > positive_40)
				player_record->side = RESOLVER_LOW_SECOND_20;
			else
				player_record->side = RESOLVER_ZERO;
		}

		else if ((negative_full > center && positive_full <= center) || (negative_40 > center && positive_40 <= center) || (negative_20 > center && positive_20 <= center))
		{
			if (negative_full > negative_40 && negative_full > negative_20)
			player_record->side = RESOLVER_FIRST;
			else if (negative_40 > negative_full && negative_40 > negative_20)
			player_record->side = RESOLVER_LOW_FIRST;
			else if (negative_20 > negative_full && negative_20 > negative_40)
			player_record->side = RESOLVER_LOW_FIRST_20;
			else
				player_record->side = RESOLVER_ZERO;
		}
		else
			get_side_trace();
	
	
}

void CResolver::get_side_standing()
{
	player_record->type = LBY;
	float EyeDelta = math::normalize_yaw(player->m_angEyeAngles().y - original_goal_feet_yaw);

	if (EyeDelta > 20.0f)
		player_record->curSide = RIGHT;

	else if (EyeDelta < -20.0f)
		player_record->curSide = LEFT;
	else
		get_side_trace();
}


float get_backward_side(player_t* player)
{
	return math::calculate_angle(g_ctx.local()->m_vecOrigin(), player->m_vecOrigin()).y;
}


void CResolver::detect_side()
{
	player_record->type = ENGINE;
	/* externs */
	Vector src3D, dst3D, forward, right, up, src, dst;
	float back_two, right_two, left_two;
	CGameTrace tr;
	CTraceFilter filter;

	/* angle vectors */
	math::angle_vectors(Vector(0, get_backward_side(player), 0), &forward, &right, &up);

	/* filtering */
	filter.pSkip = player;
	src3D = player->get_shoot_position();
	dst3D = src3D + (forward * 384);

	/* back engine tracers */
	m_trace()->TraceRay(Ray_t(src3D, dst3D), MASK_SHOT, &filter, &tr);
	back_two = (tr.endpos - tr.startpos).Length();

	/* right engine tracers */
	m_trace()->TraceRay(Ray_t(src3D + right * 35, dst3D + right * 35), MASK_SHOT, &filter, &tr);
	right_two = (tr.endpos - tr.startpos).Length();

	/* left engine tracers */
	m_trace()->TraceRay(Ray_t(src3D - right * 35, dst3D - right * 35), MASK_SHOT, &filter, &tr);
	left_two = (tr.endpos - tr.startpos).Length();

	/* fix side */
	if (left_two > right_two) {
		player_record->curSide = LEFT;
	}
	else if (right_two > left_two) {
		player_record->curSide = RIGHT;
	}
	else
		get_side_trace();
}

void CResolver::get_side_trace()
{
	auto m_side = false;
	auto trace = false;
	if (m_globals()->m_curtime - lock_side > 2.0f)
	{
		auto first_visible = util::visible(g_ctx.globals.eye_pos, player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.first), player, g_ctx.local());
		auto second_visible = util::visible(g_ctx.globals.eye_pos, player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.second), player, g_ctx.local());

		if (first_visible != second_visible)
		{
			trace = true;
			m_side = second_visible;
			lock_side = m_globals()->m_curtime;
		}
		else
		{
			auto first_position = g_ctx.globals.eye_pos.DistTo(player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.first));
			auto second_position = g_ctx.globals.eye_pos.DistTo(player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.second));

			if (fabs(first_position - second_position) > 1.0f)
				m_side = first_position > second_position;
		}
	}
	else
		trace = true;

	if (m_side)
	{
		player_record->type = trace ? TRACE : DIRECTIONAL;
		player_record->curSide = RIGHT;
	}
	else
	{
		player_record->type = trace ? TRACE : DIRECTIONAL;
		player_record->curSide = LEFT;
	}
}

float CResolver::GetChokedPackets()
{
	float simtime = player->m_flSimulationTime();
	float oldsimtime = player->m_flOldSimulationTime();
	float simdiff = simtime - oldsimtime;

	auto chokedpackets = TIME_TO_TICKS(max(0, simdiff));

	if (chokedpackets >= 1)
		return chokedpackets;

	return 0;
}

bool CResolver::DesyncDetect()
{
	if (!player->is_alive())
		return false;
	if (player->get_max_desync_delta() < 10)
		return false;
	if (!player->m_iTeamNum() == g_ctx.local()->m_iTeamNum())
		return false;
	if (player->get_move_type() == MOVETYPE_NOCLIP || player->get_move_type() == MOVETYPE_LADDER)
		return false;

	return true;
}

bool CResolver::update_walk_data()
{
	auto e = player;


	auto anim_layers = player_record->layers;
	bool s_1 = false,
		s_2 = false,
		s_3 = false;

	for (int i = 0; i < e->animlayer_count(); i++)
	{
		anim_layers[i] = e->get_animlayers()[i];
		if (anim_layers[i].m_nSequence == 26 && anim_layers[i].m_flWeight < 0.47f)
			s_1 = true;
		if (anim_layers[i].m_nSequence == 7 && anim_layers[i].m_flWeight > 0.001f)
			s_2 = true;
		if (anim_layers[i].m_nSequence == 2 && anim_layers[i].m_flWeight == 0)
			s_3 = true;
	}
	bool  m_fakewalking;
	if (s_1 && s_2)
		if (s_3)
			m_fakewalking = true;
		else
			m_fakewalking = false;
	else
		m_fakewalking = false;

	return m_fakewalking;
}

void CResolver::setmode()
{
	auto e = player;

	float speed = e->m_vecVelocity().Length();


	auto cur_layer = player_record->layers;
	auto prev_layer = player_record->previous_layers;
	
	bool on_ground = e->m_fFlags() & FL_ONGROUND && !e->get_animation_state()->m_bInHitGroundAnimation;

	bool slow_walking1 = is_slow_walking();
	bool slow_walking2 = update_walk_data();

	bool flicked_lby = abs(player_record->layers[3].m_flWeight - player_record->previous_layers[7].m_flWeight) >= 1.1f;
	bool breaking_lby = is_breaking_lby(cur_layer[3], prev_layer[3]);


	bool ducking = player->get_animation_state()->m_fDuckAmount && e->m_fFlags() & FL_ONGROUND && !player->get_animation_state()->m_bInHitGroundAnimation;



	bool stand_anim = false;
	if (player_record->layers[3].m_flWeight == 0.f && player_record->layers[3].m_flCycle == 0.f && player_record->layers[6].m_flWeight == 0.f)
		stand_anim = true;

	bool move_anim = false;
	if (int(player_record->layers[6].m_flWeight * 1000.f) == int(previous_layers[6].m_flWeight * 1000.f))
		move_anim = true;

	auto animstate = player->get_animation_state();
	if (!animstate)
		return;

	auto valid_move = true;
	if (animstate->m_velocity > 0.1f || fabs(animstate->flUpVelocity) > 100.f)
		valid_move = animstate->m_flTimeSinceStartedMoving < 0.22f;


	if (!on_ground)
	{
		player_record->curMode = AIR;
	}
	else if (valid_move && stand_anim || /*micromovement check pog*/ speed < 3.1f && ducking || speed < 1.2f && !ducking || breaking_lby)
	{
		player_record->curMode = STANDING;

	}
	else if (!valid_move && move_anim || /*micromovement check pog*/ speed >= 3.1f && ducking || speed >= 1.2f && !ducking)
	{
		if ((speed >= 1.2f && speed < 134.f) &&!ducking && (slow_walking1 || slow_walking2))
			player_record->curMode = SLOW_WALKING;
		else
		player_record->curMode = MOVING;
	}
	else
		player_record->curMode = FREESTANDING;
}

bool CResolver::MatchShot()
{
	// do not attempt to do this in nospread mode.

	float shoot_time = -1.f;

	auto weapon = player->m_hActiveWeapon();
	if (weapon) {
		// with logging this time was always one tick behind.
		// so add one tick to the last shoot time.
		shoot_time = weapon->m_fLastShotTime() + m_globals()->m_intervalpertick;
	}

	// this record has a shot on it.
	if (TIME_TO_TICKS(shoot_time) == TIME_TO_TICKS(player->m_flSimulationTime()))
	{
		return true;
	}

	return false;
}

void CResolver::final_detection()
{
	switch (player_record->curMode)
	{
	case MOVING:
		layer_test();
		break;
	case STANDING:		
		get_side_standing();
		break;
	case FREESTANDING:		
		detect_side();
		break;
	case SLOW_WALKING:		
		layer_test();
		break;

	}
}


void CResolver::resolve()
{
	
	if (!DesyncDetect())
	{
		player_record->side = RESOLVER_ORIGINAL;
		player_record->curMode = NO_MODE;
		player_record->curSide = NO_SIDE;
		return;
	}



	bool mside;
	auto e = player;
	auto pWeapon = player->m_hActiveWeapon();
	auto simtime = player->m_flSimulationTime();
	auto oldsimtime = player->m_flOldSimulationTime();
	float m_flLastShotTime;
	bool m_shot;
	m_flLastShotTime = pWeapon ? pWeapon->m_fLastShotTime() : 0.f;
	m_shot = m_flLastShotTime > oldsimtime && m_flLastShotTime <= simtime;

	setmode();

	if (m_flLastShotTime <= simtime && m_shot || MatchShot())
	{
		player_record->side = RESOLVER_ON_SHOT;
		player_record->curSide = NO_SIDE;
		player_record->shot = true;
		return;
	}

	if (player_record->curMode == AIR)
	{
		player_record->side = RESOLVER_ORIGINAL;
		player_record->curMode = AIR;
		player_record->curSide = NO_SIDE;
		return;
	}


		final_detection();


	if (player_record->type != LAYERS)
	{
		if (g_ctx.globals.restype[player_record->type].missed_shots[player->EntIndex()] == 1)
		{
			
			g_cfg.player_list.sides[player_record->curSide].low_delta[e->EntIndex()] = true;
			g_cfg.player_list.sides[player_record->curSide].low_delta_20[e->EntIndex()] = false;
			g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = false;
			

		}
		else if (g_ctx.globals.restype[player_record->type].missed_shots[player->EntIndex()] == 2 )
		{
			
			g_cfg.player_list.sides[player_record->curSide].low_delta[e->EntIndex()] = false;
			g_cfg.player_list.sides[player_record->curSide].low_delta_20[e->EntIndex()] = true;
			g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = false;
			

		}
		else if (g_ctx.globals.restype[player_record->type].missed_shots[player->EntIndex()] == 3 )
		{
			
			g_cfg.player_list.sides[player_record->curSide].low_delta[e->EntIndex()] = false;
			g_cfg.player_list.sides[player_record->curSide].low_delta_20[e->EntIndex()] = false;
			g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = true;
		

		}
		else if (g_ctx.globals.restype[player_record->type].missed_shots[player->EntIndex()] == 4 )
		{
			
			g_cfg.player_list.sides[player_record->curSide].low_delta[e->EntIndex()] = true;
			g_cfg.player_list.sides[player_record->curSide].low_delta_20[e->EntIndex()] = false;
			g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = true;
			

		}
		else if (g_ctx.globals.restype[player_record->type].missed_shots[player->EntIndex()] == 5)
		{
			
			g_cfg.player_list.sides[player_record->curSide].low_delta[e->EntIndex()] = false;
			g_cfg.player_list.sides[player_record->curSide].low_delta_20[e->EntIndex()] = true;
			g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = true;
			

		}
		else if (g_ctx.globals.restype[player_record->type].missed_shots[player->EntIndex()] >= 6 )
		{
			
			g_cfg.player_list.sides[player_record->curSide].low_delta[e->EntIndex()] = false;
			g_cfg.player_list.sides[player_record->curSide].low_delta_20[e->EntIndex()] = false;
			g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = false;
			
		}

		final_detection();
		if (player_record->curSide == RIGHT)
			mside = false;
		else if (player_record->curSide == LEFT)
			mside = true;
	
	}


	if (player_record->type == LAYERS || player_record->type == LBY)
		g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = false;


	/*else if (player_record->type == LAYERS && g_ctx.globals.restype[player_record->type].missed_shots[player->EntIndex()] == 1 && g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] == false)
		g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = true;
	else if (player_record->type == LAYERS && g_ctx.globals.restype[player_record->type].missed_shots[player->EntIndex()] == 1 && g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] == true)
		g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()] = false;*/


	player_record->flipped_s = g_cfg.player_list.types[player_record->type].should_flip[e->EntIndex()];

	if ((player_record->curSide == RIGHT || player_record->curSide == LEFT) && player_record->type != LAYERS)
		player_record->side = mside ? RESOLVER_FIRST : RESOLVER_SECOND;
}

float CResolver::resolve_pitch()
{
	if (!g_cfg.ragebot.pitch_correction)
		return original_pitch;

	float liohsdafg = 0.f;
	if (g_cfg.ragebot.pitch_correction)
	{
		if (liohsdafg < -179.f) liohsdafg += 360.f;
		else if (liohsdafg > 90.0 || liohsdafg < -90.0) liohsdafg = 89.f;
		else if (liohsdafg > 89.0 && liohsdafg < 91.0) liohsdafg -= 90.f;
		else if (liohsdafg > 179.0 && liohsdafg < 181.0) liohsdafg -= 180;
		else if (liohsdafg > -179.0 && liohsdafg < -181.0) liohsdafg += 180;

		else if (fabs(liohsdafg) == 0) liohsdafg = copysign(89.0f, liohsdafg);
		else liohsdafg = original_pitch;

		return liohsdafg;
	}
	else
		return original_pitch;
}




