#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "../Weapon.h"
#include "../client/ClientEffect.h"

#ifndef __GAME_PROJECTILE_H__
#include "../Projectile.h"
#endif

class rvWeaponFF : public rvWeapon {
public:

	CLASS_PROTOTYPE(rvWeaponFF);

	rvWeaponFF(void);
	~rvWeaponFF(void);

	virtual void			Spawn(void);
	virtual void			Think(void);

	void					Save(idSaveGame* saveFile) const;
	void					Restore(idRestoreGame* saveFile);
	void					PreSave(void);
	void					PostSave(void);


#ifdef _XENON
	virtual bool		AllowAutoAim(void) const { return false; }
#endif

protected:

	virtual void			OnLaunchProjectile(idProjectile* proj);

	void					SetFFState(const char* state, int blendFrames);

	rvClientEntityPtr<rvClientEffect>	guideEffect;
	idList< idEntityPtr<idEntity> >		guideEnts;
	float								guideSpeedSlow;
	float								guideSpeedFast;
	float								guideRange;
	float								guideAccelTime;

	rvStateThread						FFThread;

	float								reloadRate;

	bool								idleEmpty;

private:

	stateResult_t		State_Idle(const stateParms_t& parms);
	stateResult_t		State_Fire(const stateParms_t& parms);
	stateResult_t		State_Raise(const stateParms_t& parms);
	stateResult_t		State_Lower(const stateParms_t& parms);

	stateResult_t		State_FF_Idle(const stateParms_t& parms);
	stateResult_t		State_FF_Reload(const stateParms_t& parms);

	stateResult_t		Frame_AddToClip(const stateParms_t& parms);

	CLASS_STATES_PROTOTYPE(rvWeaponFF);
};

CLASS_DECLARATION(rvWeapon, rvWeaponFF)
END_CLASS

/*
================
rvWeaponFF::rvWeaponFF
================
*/
rvWeaponFF::rvWeaponFF(void) {
}

/*
================
rvWeaponFF::~rvWeaponFF
================
*/
rvWeaponFF::~rvWeaponFF(void) {
	if (guideEffect) {
		guideEffect->Stop();
	}
}

/*
================
rvWeaponFF::Spawn
================
*/
void rvWeaponFF::Spawn(void) {
	float f;

	idleEmpty = false;

	spawnArgs.GetFloat("lockRange", "0", guideRange);

	spawnArgs.GetFloat("lockSlowdown", ".25", f);
	attackDict.GetFloat("speed", "0", guideSpeedFast);
	guideSpeedSlow = guideSpeedFast * f;

	reloadRate = SEC2MS(spawnArgs.GetFloat("reloadRate", ".8"));

	guideAccelTime = SEC2MS(spawnArgs.GetFloat("lockAccelTime", ".25"));

	// Start FF thread
	FFThread.SetName(viewModel->GetName());
	FFThread.SetOwner(this);

	// Adjust reload animations to match the fire rate
	idAnim* anim;
	int		animNum;
	float	rate;
	animNum = viewModel->GetAnimator()->GetAnim("reload");
	if (animNum) {
		anim = (idAnim*)viewModel->GetAnimator()->GetAnim(animNum);
		rate = (float)anim->Length() / (float)SEC2MS(spawnArgs.GetFloat("reloadRate", ".8"));
		anim->SetPlaybackRate(rate);
	}

	animNum = viewModel->GetAnimator()->GetAnim("reload_empty");
	if (animNum) {
		anim = (idAnim*)viewModel->GetAnimator()->GetAnim(animNum);
		rate = (float)anim->Length() / (float)SEC2MS(spawnArgs.GetFloat("reloadRate", ".8"));
		anim->SetPlaybackRate(rate);
	}

	SetState("Raise", 0);
	SetFFState("FF_Idle", 0);
}

/*
================
rvWeaponFF::Think
================
*/
void rvWeaponFF::Think(void) {
	trace_t	tr;
	int		i;

	FFThread.Execute();

	// Let the real weapon think first
	rvWeapon::Think();

	// IF no guide range is set then we dont have the mod yet	
	if (!guideRange) {
		return;
	}

	if (!wsfl.zoom) {
		if (guideEffect) {
			guideEffect->Stop();
			guideEffect = NULL;
		}

		for (i = guideEnts.Num() - 1; i >= 0; i--) {
			idGuidedProjectile* proj = static_cast<idGuidedProjectile*>(guideEnts[i].GetEntity());
			if (!proj || proj->IsHidden()) {
				guideEnts.RemoveIndex(i);
				continue;
			}

			// If the FF is still guiding then stop the guide and slow it down
			if (proj->GetGuideType() != idGuidedProjectile::GUIDE_NONE) {
				proj->CancelGuide();
				proj->SetSpeed(guideSpeedFast, (1.0f - (proj->GetSpeed() - guideSpeedSlow) / (guideSpeedFast - guideSpeedSlow)) * guideAccelTime);
			}
		}

		return;
	}

	// Cast a ray out to the lock range
// RAVEN BEGIN
// ddynerman: multiple clip worlds
	gameLocal.TracePoint(owner, tr,
		playerViewOrigin,
		playerViewOrigin + playerViewAxis[0] * guideRange,
		MASK_SHOT_RENDERMODEL, owner);
	// RAVEN END

	for (i = guideEnts.Num() - 1; i >= 0; i--) {
		idGuidedProjectile* proj = static_cast<idGuidedProjectile*>(guideEnts[i].GetEntity());
		if (!proj || proj->IsHidden()) {
			guideEnts.RemoveIndex(i);
			continue;
		}

		// If the FF isnt guiding yet then adjust its speed back to normal
		if (proj->GetGuideType() == idGuidedProjectile::GUIDE_NONE) {
			proj->SetSpeed(guideSpeedSlow, (proj->GetSpeed() - guideSpeedSlow) / (guideSpeedFast - guideSpeedSlow) * guideAccelTime);
		}
		proj->GuideTo(tr.endpos);
	}

	if (!guideEffect) {
		guideEffect = gameLocal.PlayEffect(gameLocal.GetEffect(spawnArgs, "fx_guide"), tr.endpos, tr.c.normal.ToMat3(), true, vec3_origin, true);
	}
	else {
		guideEffect->SetOrigin(tr.endpos);
		guideEffect->SetAxis(tr.c.normal.ToMat3());
	}
}

/*
================
rvWeaponFF::OnLaunchProjectile
================
*/
void rvWeaponFF::OnLaunchProjectile(idProjectile* proj) {
	rvWeapon::OnLaunchProjectile(proj);

	// Double check that its actually a guided projectile
	if (!proj || !proj->IsType(idGuidedProjectile::GetClassType())) {
		return;
	}

	// Launch the projectile
	idEntityPtr<idEntity> ptr;
	ptr = proj;
	guideEnts.Append(ptr);
}

/*
================
rvWeaponFF::SetFFState
================
*/
void rvWeaponFF::SetFFState(const char* state, int blendFrames) {
	FFThread.SetState(state, blendFrames);
}

/*
=====================
rvWeaponFF::Save
=====================
*/
void rvWeaponFF::Save(idSaveGame* saveFile) const {
	saveFile->WriteObject(guideEffect);

	idEntity* ent = NULL;
	saveFile->WriteInt(guideEnts.Num());
	for (int ix = 0; ix < guideEnts.Num(); ++ix) {
		ent = guideEnts[ix].GetEntity();
		if (ent) {
			saveFile->WriteObject(ent);
		}
	}

	saveFile->WriteFloat(guideSpeedSlow);
	saveFile->WriteFloat(guideSpeedFast);
	saveFile->WriteFloat(guideRange);
	saveFile->WriteFloat(guideAccelTime);

	saveFile->WriteFloat(reloadRate);

	FFThread.Save(saveFile);
}

/*
=====================
rvWeaponFF::Restore
=====================
*/
void rvWeaponFF::Restore(idRestoreGame* saveFile) {
	int numEnts = 0;
	idEntity* ent = NULL;
	rvClientEffect* clientEffect = NULL;

	saveFile->ReadObject(reinterpret_cast<idClass*&>(clientEffect));
	guideEffect = clientEffect;

	saveFile->ReadInt(numEnts);
	guideEnts.Clear();
	guideEnts.SetNum(numEnts);
	for (int ix = 0; ix < numEnts; ++ix) {
		saveFile->ReadObject(reinterpret_cast<idClass*&>(ent));
		guideEnts[ix] = ent;
	}

	saveFile->ReadFloat(guideSpeedSlow);
	saveFile->ReadFloat(guideSpeedFast);
	saveFile->ReadFloat(guideRange);
	saveFile->ReadFloat(guideAccelTime);

	saveFile->ReadFloat(reloadRate);

	FFThread.Restore(saveFile, this);
}

/*
================
rvWeaponFF::PreSave
================
*/
void rvWeaponFF::PreSave(void) {
}

/*
================
rvWeaponFF::PostSave
================
*/
void rvWeaponFF::PostSave(void) {
}


/*
===============================================================================

	States

===============================================================================
*/

CLASS_STATES_DECLARATION(rvWeaponFF)
STATE("Idle", rvWeaponFF::State_Idle)
STATE("Fire", rvWeaponFF::State_Fire)
STATE("Raise", rvWeaponFF::State_Raise)
STATE("Lower", rvWeaponFF::State_Lower)

STATE("FF_Idle", rvWeaponFF::State_FF_Idle)
STATE("FF_Reload", rvWeaponFF::State_FF_Reload)

STATE("AddToClip", rvWeaponFF::Frame_AddToClip)
END_CLASS_STATES


/*
================
rvWeaponFF::State_Raise

Raise the weapon
================
*/
stateResult_t rvWeaponFF::State_Raise(const stateParms_t& parms) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};
	switch (parms.stage) {
		// Start the weapon raising
	case STAGE_INIT:
		SetStatus(WP_RISING);
		PlayAnim(ANIMCHANNEL_LEGS, "raise", 0);
		return SRESULT_STAGE(STAGE_WAIT);

	case STAGE_WAIT:
		if (AnimDone(ANIMCHANNEL_LEGS, 4)) {
			SetState("Idle", 4);
			return SRESULT_DONE;
		}
		if (wsfl.lowerWeapon) {
			SetState("Lower", 4);
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponFF::State_Lower

Lower the weapon
================
*/
stateResult_t rvWeaponFF::State_Lower(const stateParms_t& parms) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
		STAGE_WAITRAISE
	};
	switch (parms.stage) {
	case STAGE_INIT:
		SetStatus(WP_LOWERING);
		PlayAnim(ANIMCHANNEL_LEGS, "putaway", parms.blendFrames);
		return SRESULT_STAGE(STAGE_WAIT);

	case STAGE_WAIT:
		if (AnimDone(ANIMCHANNEL_LEGS, 0)) {
			SetStatus(WP_HOLSTERED);
			return SRESULT_STAGE(STAGE_WAITRAISE);
		}
		return SRESULT_WAIT;

	case STAGE_WAITRAISE:
		if (wsfl.raiseWeapon) {
			SetState("Raise", 0);
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponFF::State_Idle
================
*/
stateResult_t rvWeaponFF::State_Idle(const stateParms_t& parms) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};
	switch (parms.stage) {
	case STAGE_INIT:
		if (!AmmoAvailable()) {
			SetStatus(WP_OUTOFAMMO);
		}
		else {
			SetStatus(WP_READY);
		}

		PlayCycle(ANIMCHANNEL_LEGS, "idle", parms.blendFrames);
		return SRESULT_STAGE(STAGE_WAIT);

	case STAGE_WAIT:
		if (wsfl.lowerWeapon) {
			SetState("Lower", 4);
			return SRESULT_DONE;
		}
		if (gameLocal.time > nextAttackTime && wsfl.attack && (gameLocal.isClient || AmmoInClip())) {
			SetState("Fire", 2);
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponFF::State_Fire
================
*/
stateResult_t rvWeaponFF::State_Fire(const stateParms_t& parms) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};
	switch (parms.stage) {
	case STAGE_INIT:
		nextAttackTime = gameLocal.time + (fireRate * owner->PowerUpModifier(PMOD_FIRERATE));
		Attack(false, 10, 5.0f, 0, 10.0f);
		/*OG Attack ( false, 1, spread, 0, 1.0f );*/
		/*Attack ( false, 10, 0.0f, 0, 10.0f );*/
/*TODO ys56
* altAttack, num_attacks, spread, fuseOffset, power

*/
		PlayAnim(ANIMCHANNEL_LEGS, "fire", parms.blendFrames);
		return SRESULT_STAGE(STAGE_WAIT);

	case STAGE_WAIT:
		if (wsfl.attack && gameLocal.time >= nextAttackTime && (gameLocal.isClient || AmmoInClip()) && !wsfl.lowerWeapon) {
			SetState("Fire", 0);
			return SRESULT_DONE;
		}
		if (gameLocal.time > nextAttackTime && AnimDone(ANIMCHANNEL_LEGS, 4)) {
			SetState("Idle", 4);
			return SRESULT_DONE;
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponFF::State_FF_Idle
================
*/
stateResult_t rvWeaponFF::State_FF_Idle(const stateParms_t& parms) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
		STAGE_WAITEMPTY,
	};

	switch (parms.stage) {
	case STAGE_INIT:
		if (AmmoAvailable() <= AmmoInClip()) {
			PlayAnim(ANIMCHANNEL_TORSO, "idle_empty", parms.blendFrames);
			idleEmpty = true;
		}
		else {
			PlayAnim(ANIMCHANNEL_TORSO, "idle", parms.blendFrames);
		}
		return SRESULT_STAGE(STAGE_WAIT);

	case STAGE_WAIT:
		if (AmmoAvailable() > AmmoInClip()) {
			if (idleEmpty) {
				SetFFState("FF_Reload", 0);
				return SRESULT_DONE;
			}
			else if (ClipSize() > 1) {
				if (gameLocal.time > nextAttackTime && AmmoInClip() < ClipSize()) {
					if (!AmmoInClip() || !wsfl.attack) {
						SetFFState("FF_Reload", 0);
						return SRESULT_DONE;
					}
				}
			}
			else {
				if (AmmoInClip() == 0) {
					SetFFState("FF_Reload", 0);
					return SRESULT_DONE;
				}
			}
		}
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponFF::State_FF_Reload
================
*/
stateResult_t rvWeaponFF::State_FF_Reload(const stateParms_t& parms) {
	enum {
		STAGE_INIT,
		STAGE_WAIT,
	};

	switch (parms.stage) {
	case STAGE_INIT: {
		const char* animName;
		int			animNum;

		if (idleEmpty) {
			animName = "ammo_pickup";
			idleEmpty = false;
		}
		else if (AmmoAvailable() == AmmoInClip() + 1) {
			animName = "reload_empty";
		}
		else {
			animName = "reload";
		}

		animNum = viewModel->GetAnimator()->GetAnim(animName);
		if (animNum) {
			idAnim* anim;
			anim = (idAnim*)viewModel->GetAnimator()->GetAnim(animNum);
			anim->SetPlaybackRate((float)anim->Length() / (reloadRate * owner->PowerUpModifier(PMOD_FIRERATE)));
		}

		PlayAnim(ANIMCHANNEL_TORSO, animName, parms.blendFrames);

		return SRESULT_STAGE(STAGE_WAIT);
	}

	case STAGE_WAIT:
		if (AnimDone(ANIMCHANNEL_TORSO, 0)) {
			if (!wsfl.attack && gameLocal.time > nextAttackTime && AmmoInClip() < ClipSize() && AmmoAvailable() > AmmoInClip()) {
				SetFFState("FF_Reload", 0);
			}
			else {
				SetFFState("FF_Idle", 0);
			}
			return SRESULT_DONE;
		}
		/*
		if ( gameLocal.isMultiplayer && gameLocal.time > nextAttackTime && wsfl.attack ) {
			if ( AmmoInClip ( ) == 0 )
			{
				AddToClip ( ClipSize() );
			}
			SetFFState ( "FF_Idle", 0 );
			return SRESULT_DONE;
		}
		*/
		return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}

/*
================
rvWeaponFF::Frame_AddToClip
================
*/
stateResult_t rvWeaponFF::Frame_AddToClip(const stateParms_t& parms) {
	AddToClip(1);
	return SRESULT_OK;
}

