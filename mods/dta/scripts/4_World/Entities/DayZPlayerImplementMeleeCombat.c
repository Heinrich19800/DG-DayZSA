enum MeleeCombatHit
{
	NONE = -1,

	LIGHT,
	HEAVY,
	SPRINT,
	KICK,

	WPN_HIT,
	WPN_STAB,
}

class DayZPlayerImplementMeleeCombat
{
	// target selection
	protected const float				TARGETING_ANGLE_NORMAL	= 30.0;
	protected const float				TARGETING_ANGLE_SPRINT	= 15.0;
	protected const float				TARGETING_MIN_HEIGHT	= -2.0;
	protected const float				TARGETING_MAX_HEIGHT	= 2.0;
	protected const float				TARGETING_RAY_RADIUS	= 0.22;
	protected const float				TARGETING_RAY_DIST		= 5.0;

	protected const float 				RANGE_EXTENDER_NORMAL	= 0.65;
	protected const float 				RANGE_EXTENDER_SPRINT	= 1.35;

	protected const string 				DUMMY_LIGHT_AMMO		= "Dummy_Light";
	protected const string				DUMMY_HEAVY_AMMO		= "Dummy_Heavy";
	
	// members
	protected DayZPlayerImplement		m_DZPlayer;
	
	protected Object					m_TargetObject;
	protected ref array<Object> 		m_AllTargetObjects;
	protected ref array<typename>		m_TargetableObjects;
	protected ref array<typename>		m_NonAlignableObjects;

	protected InventoryItem				m_Weapon;
	protected int						m_WeaponMode;
	protected float						m_WeaponRange;
	
	protected bool 						m_SprintAttack;
	
	protected vector 					m_RayStart;
	protected vector	 				m_RayEnd;	

	protected int 						m_HitZoneIdx;
	protected string					m_HitZoneName;
	protected vector					m_HitPositionWS;
	
	protected MeleeCombatHit			m_HitMask;

	// ------------------------------------------------------------
	// CONSTRUCTOR
	// ------------------------------------------------------------

	void DayZPlayerImplementMeleeCombat(DayZPlayerImplement player)
	{
		Init(player);
	}
	
	void Init(DayZPlayerImplement player)
	{
		m_DZPlayer 		= player;
		
		m_HitZoneName	= "";
		m_HitZoneIdx 	= -1;
		m_HitPositionWS = "-1 -1 -1";
		
		m_TargetObject      = null;
		m_AllTargetObjects 	= new array<Object>;

		m_TargetableObjects = new array<typename>;
		m_TargetableObjects.Insert(DayZPlayer);
		m_TargetableObjects.Insert(DayZInfected);
		m_TargetableObjects.Insert(DayZAnimal);
		//m_TargetableObjects.Insert(Building);
		//m_TargetableObjects.Insert(Transport);

		m_NonAlignableObjects = new array<typename>;		
		//m_NonAlignableObjects.Insert(Building);
		//m_NonAlignableObjects.Insert(Transport);
	}

	void ~DayZPlayerImplementMeleeCombat() {}

	// ------------------------------------------------------------
	// PUBLIC
	// ------------------------------------------------------------

	MeleeCombatHit GetHitMask()
	{
		return m_HitMask;
	}

	void SetTargetObject(Object target)
		{ m_TargetObject = target; }
	
	void SetHitZoneIdx(int hitZone)
		{ m_HitZoneIdx = hitZone; }
	
	EntityAI GetTargetObject()
	{
		EntityAI target;
		Class.CastTo(target, m_TargetObject);

		return target;
	}

	void Update(InventoryItem weapon, MeleeCombatHit hitMask)
	{
		m_Weapon = weapon;
		m_HitMask = hitMask;
		m_SprintAttack = (hitMask & MeleeCombatHit.SPRINT) == MeleeCombatHit.SPRINT;
		m_WeaponMode = SelectWeaponMode(weapon);
		m_WeaponRange = GetWeaponRange(weapon, m_WeaponMode);
		m_AllTargetObjects.Clear();
		m_HitPositionWS = "0.5 0.5 0.5";

		if( !GetGame().IsMultiplayer() || !GetGame().IsServer() )
		{
			if( !ScriptInputUserData.CanStoreInputUserData() )
			{
				Error("DayZPlayerImplementMeleeCombat - ScriptInputUserData already posted");
				return;
			}

			TargetSelection();
			HitZoneSelection();

			//! store target into input packet
			if( GetGame().IsMultiplayer() )
			{
				ScriptInputUserData ctx = new ScriptInputUserData;
				ctx.Write(INPUT_UDT_MELEE_TARGET);
				ctx.Write(m_TargetObject);
				ctx.Write(m_HitZoneIdx);
				ctx.Send();
			}
		}
	}
	
	void ProcessHit()
	{
		if( m_TargetObject )
		{
			bool noDamage = false;
			string ammo = DUMMY_LIGHT_AMMO;
			vector hitPosWS = m_HitPositionWS;
			vector modelPos = m_TargetObject.WorldToModel(m_HitPositionWS);
			PlayerBase targetedPlayer = NULL;
			
			EntityAI m_TargetEntity = EntityAI.Cast(m_TargetObject);
			
			//! sound
			if( GetGame().IsMultiplayer() && GetGame().IsServer() ) // HOTFIX for old melee hit sounds in MP - has to be redone
			{
				hitPosWS = m_TargetObject.GetPosition();
			}

			//! Melee Hit/Impact modifiers
			if( m_TargetObject.IsInherited(DayZPlayer) )
			{
				if( Class.CastTo(targetedPlayer, m_TargetObject) )
				{
					//! if the oponnent is in Melee Block decrease the damage
					if( targetedPlayer.GetMeleeFightLogic() && targetedPlayer.GetMeleeFightLogic().IsInBlock())
					{
						if( m_WeaponMode > 0 )
							m_WeaponMode--; // Heavy -> Light
						else
							noDamage = true;
					}
				}
			}

			//! in case of kick (on back or push from erc) change the ammo type to dummy 
			if((m_HitMask & (MeleeCombatHit.KICK|MeleeCombatHit.WPN_HIT)) == (MeleeCombatHit.KICK|MeleeCombatHit.WPN_HIT))
			{
				ammo = DUMMY_HEAVY_AMMO;
				noDamage = true;
			}

			if(!noDamage)
			{
				//! normal hit with applied damage to targeted component
				if (m_HitZoneIdx > -1)
				{
					//hitPosWS = m_TargetEntity.ModelToWorld(m_TargetEntity.GetDefaultHitPosition());
					m_DZPlayer.ProcessMeleeHit(m_Weapon, m_WeaponMode, m_TargetObject, m_HitZoneIdx, hitPosWS);
				}
				else
				{
					hitPosWS = m_TargetEntity.ModelToWorld(m_TargetEntity.GetDefaultHitPosition()); //! override hit pos by pos defined in type
					m_DZPlayer.ProcessMeleeHitName(m_Weapon, m_WeaponMode, m_TargetObject, m_TargetEntity.GetDefaultHitComponent(), hitPosWS);
				}
			}
			else
			{
				//! play hit animation for dummy hits
				if( GetGame().IsServer() && targetedPlayer )
				{
					hitPosWS = m_TargetEntity.GetDefaultHitPosition(); //! override hit pos by pos defined in type
					targetedPlayer.EEHitBy(null, 0, EntityAI.Cast(m_DZPlayer), m_HitZoneIdx, m_HitZoneName, ammo, hitPosWS);
				}
			}
		}
	}
			
	// ------------------------------------------------------------
	// protected
	// ------------------------------------------------------------

	protected int SelectWeaponMode(InventoryItem weapon)
	{
		if( weapon )
		{
			if (weapon.IsInherited(Weapon))
			{
				switch(m_HitMask)
				{
					case MeleeCombatHit.WPN_STAB :
						return weapon.GetMeleeMode();
					break;
					case MeleeCombatHit.WPN_HIT :
						return weapon.GetMeleeHeavyMode();
					break;
				}
			}
			else
			{
				switch(m_HitMask)
				{
					case MeleeCombatHit.LIGHT :
						return weapon.GetMeleeMode();
					break;
					case MeleeCombatHit.HEAVY :
						return weapon.GetMeleeHeavyMode();
					break;
					case MeleeCombatHit.SPRINT :
						return weapon.GetMeleeSprintMode();
					break;
				}
			}
		}

		switch(m_HitMask)
		{
			case MeleeCombatHit.HEAVY :
				return 1;
			break;
			case MeleeCombatHit.SPRINT :
				return 2;
			break;
		}
		return 0;
	}
	
	protected float GetWeaponRange(InventoryItem weapon, int weaponMode)
	{
		if( weapon )
		{
			return weapon.GetMeleeCombatData().GetModeRange(weaponMode);
		}
		else
		{
			return m_DZPlayer.GetMeleeCombatData().GetModeRange(weaponMode);
		}
	}

	protected void TargetSelection()
	{
		PlayerBase player = PlayerBase.Cast(m_DZPlayer);
		vector pos = m_DZPlayer.GetPosition();
		vector dir = MiscGameplayFunctions.GetHeadingVector(player);

		float dist = m_WeaponRange + RANGE_EXTENDER_NORMAL;
		float tgtAngle = TARGETING_ANGLE_NORMAL;
		if (m_SprintAttack)
		{
			dist = m_WeaponRange + RANGE_EXTENDER_SPRINT;
			tgtAngle = TARGETING_ANGLE_SPRINT;
		}

		m_TargetObject = DayZPlayerUtils.GetMeleeTarget(pos, dir, tgtAngle, dist, TARGETING_MIN_HEIGHT, TARGETING_MAX_HEIGHT, m_DZPlayer, m_TargetableObjects, m_AllTargetObjects);

		if(IsObstructed(m_TargetObject))
			m_TargetObject = null;
	}

	protected void HitZoneSelection()
	{
		Object cursorTarget = null;
		PlayerBase player = PlayerBase.Cast(m_DZPlayer);

		// ray properties 
		vector pos;
		vector cameraDirection = GetGame().GetCurrentCameraDirection();

		MiscGameplayFunctions.GetHeadBonePos(player, pos);
		m_RayStart = pos;
		m_RayEnd = pos + cameraDirection * TARGETING_RAY_DIST;

		// raycast
		vector hitPos;
		vector hitNormal;	
		ref set<Object> hitObjects = new set<Object>;

		if ( DayZPhysics.RaycastRV( m_RayStart, m_RayEnd, m_HitPositionWS, hitNormal, m_HitZoneIdx, hitObjects, null, player, false, false, ObjIntersectIFire, TARGETING_RAY_RADIUS ) )
		{
			if( hitObjects.Count() )
			{
				cursorTarget = hitObjects.Get(0);
				//! just for buidling and transports (big objects)
				if( cursorTarget.IsAnyInherited(m_NonAlignableObjects) )
				{
					//! if no object in cone, set this object from raycast for these special cases
					if (m_TargetObject == null)
						m_TargetObject = cursorTarget;
				}

				if ( cursorTarget == m_TargetObject )
					m_HitZoneName = cursorTarget.GetDamageZoneNameByComponentIndex(m_HitZoneIdx);
			}
		}
		else
			m_HitZoneIdx = -1;
	}

	protected bool IsObstructed(Object object)
	{
		// check direct visibility of object (obstruction check)
		PhxInteractionLayers collisionLayerMask = PhxInteractionLayers.BUILDING|PhxInteractionLayers.DOOR|PhxInteractionLayers.VEHICLE|PhxInteractionLayers.ROADWAY|PhxInteractionLayers.TERRAIN|PhxInteractionLayers.ITEM_SMALL|PhxInteractionLayers.ITEM_LARGE|PhxInteractionLayers.FENCE;
		int hitComponentIndex;
		float hitFraction;
		vector start, end, hitNormal, hitPosObstructed;
		Object hitObject = null;
		PlayerBase player = PlayerBase.Cast(m_DZPlayer);

		if(object)
		{
			MiscGameplayFunctions.GetHeadBonePos(player, start);
			end = start + MiscGameplayFunctions.GetHeadingVector(player) * vector.Distance(player.GetPosition(), object.GetPosition());

			return DayZPhysics.RayCastBullet( start, end, collisionLayerMask, null, hitObject, hitPosObstructed, hitNormal, hitFraction);
		}

		return false;
	}
	
#ifdef DEVELOPER
	// ------------------------------------------------------------
	// DEBUG
	// ------------------------------------------------------------
	protected ref array<Shape> dbgConeShapes = new array<Shape>();
	protected ref array<Shape> dbgTargets = new array<Shape>();
	protected ref array<Shape> hitPosShapes = new array<Shape>();
	
	void Debug(InventoryItem weapon, MeleeCombatHit hitMask)
	{
		bool show_targets = DiagMenu.GetBool(DiagMenuIDs.DM_MELEE_SHOW_TARGETS) && DiagMenu.GetBool(DiagMenuIDs.DM_MELEE_DEBUG_ENABLE);
		bool draw_targets = DiagMenu.GetBool(DiagMenuIDs.DM_MELEE_DRAW_TARGETS) && DiagMenu.GetBool(DiagMenuIDs.DM_MELEE_DEBUG_ENABLE);
		bool draw_range = DiagMenu.GetBool(DiagMenuIDs.DM_MELEE_DRAW_RANGE) && DiagMenu.GetBool(DiagMenuIDs.DM_MELEE_DEBUG_ENABLE);

		if( show_targets || draw_targets || draw_range )
		{
			if( !GetGame().IsMultiplayer() || !GetGame().IsServer() )
			{
				m_Weapon = weapon;
				m_HitMask = hitMask;
				m_SprintAttack = (hitMask & MeleeCombatHit.SPRINT) == MeleeCombatHit.SPRINT;
				m_WeaponMode = SelectWeaponMode(weapon);
				m_WeaponRange = GetWeaponRange(weapon, m_WeaponMode);
				m_AllTargetObjects.Clear();
				m_HitPositionWS = "0.5 0.5 0.5";
				
				TargetSelection();
				HitZoneSelection();
			}
		}

		ShowDebugMeleeTarget(show_targets);
		DrawDebugTargets(show_targets);
		DrawDebugMeleeHitPosition(draw_targets);
		DrawDebugMeleeCone(draw_range);
	}

	//! shows target in DbgUI 'window'
	protected void ShowDebugMeleeTarget(bool enabled)
	{
		int windowPosX = 0;
		int windowPosY = 500;

		//DbgUI.BeginCleanupScope();
		DbgUI.Begin("Melee Target", windowPosX, windowPosY);
		if (enabled )
		{
			if ( m_TargetObject /*&& m_TargetObject.IsMan()*/ )
			{
				DbgUI.Text("Character: " + m_TargetObject.GetDisplayName());
				DbgUI.Text("HitZone: " + m_HitZoneName + "(" + m_HitZoneIdx + ")");
				DbgUI.Text("HitPosWS:" + m_HitPositionWS);
			}
		}
		DbgUI.End();
		//DbgUI.EndCleanupScope();
	}

	//! shows debug sphere above the target
	protected void DrawDebugTargets(bool enabled)
	{
		vector w_pos, w_pos_sphr, w_pos_lend;
		Object obj;

		if ( enabled )
		{
			CleanupDebugShapes(dbgTargets);

			for (int i = 0; i < m_AllTargetObjects.Count(); i++ )
			{
				if ( m_TargetObject && m_AllTargetObjects.Count() )
				{
					obj = m_AllTargetObjects.Get(i);
					w_pos = obj.GetPosition();
					// sphere pos tweaks
					w_pos_sphr = w_pos;
					w_pos_sphr[1] = w_pos_sphr[1] + 1.8;
					// line pos tweaks
					w_pos_lend = w_pos;
					w_pos_lend[1] = w_pos_lend[1] + 1.8;
					
					if ( m_AllTargetObjects.Get(i) == m_TargetObject )
					{
						dbgTargets.Insert( Debug.DrawSphere(w_pos_sphr, 0.05, COLOR_RED, ShapeFlags.NOOUTLINE) );
						dbgTargets.Insert( Debug.DrawLine(w_pos, w_pos_lend, COLOR_RED) );
					}
					else
					{
						dbgTargets.Insert( Debug.DrawSphere(w_pos_sphr, 0.05, COLOR_YELLOW, ShapeFlags.NOOUTLINE) );
						dbgTargets.Insert( Debug.DrawLine(w_pos, w_pos_lend, COLOR_YELLOW) );
					}
				}
			}
		}
		else
			CleanupDebugShapes(dbgTargets);
	}
		
	protected void DrawDebugMeleeCone(bool enabled)
	{
		// "cone" settings
		vector start, end, endL, endR;
		float playerAngle;
		float xL,xR,zL,zR;
		float dist = m_WeaponRange + RANGE_EXTENDER_NORMAL;
		float tgtAngle = TARGETING_ANGLE_NORMAL;
		
		PlayerBase player = PlayerBase.Cast(m_DZPlayer);
		if (m_SprintAttack)
		{
			dist = m_WeaponRange + RANGE_EXTENDER_SPRINT;
			tgtAngle = TARGETING_ANGLE_SPRINT;
		}

		if (enabled)
		{
			CleanupDebugShapes(dbgConeShapes);

			start = m_DZPlayer.GetPosition();
			playerAngle = MiscGameplayFunctions.GetHeadingAngle(player);
			
			endL = start;
			endR = start;
			xL = dist * Math.Cos(playerAngle + Math.PI_HALF + tgtAngle * Math.DEG2RAD); // x
			zL = dist * Math.Sin(playerAngle + Math.PI_HALF + tgtAngle * Math.DEG2RAD); // z
			xR = dist * Math.Cos(playerAngle + Math.PI_HALF - tgtAngle * Math.DEG2RAD); // x
			zR = dist * Math.Sin(playerAngle + Math.PI_HALF - tgtAngle * Math.DEG2RAD); // z
			endL[0] = endL[0] + xL;
			endL[2] = endL[2] + zL;
			endR[0] = endR[0] + xR;
			endR[2] = endR[2] + zR;

			dbgConeShapes.Insert( Debug.DrawLine(start, endL, COLOR_BLUE ) );
			dbgConeShapes.Insert( Debug.DrawLine(start, endR, COLOR_BLUE) ) ;
			dbgConeShapes.Insert( Debug.DrawLine(endL, endR, COLOR_BLUE  ) );
		}
		else
			CleanupDebugShapes(dbgConeShapes);		
	}	

	protected void DrawDebugMeleeHitPosition(bool enabled)
	{
		if (enabled && m_TargetObject)
		{
			CleanupDebugShapes(hitPosShapes);
			hitPosShapes.Insert( Debug.DrawSphere(m_HitPositionWS, TARGETING_RAY_RADIUS, COLOR_YELLOW, ShapeFlags.NOOUTLINE|ShapeFlags.TRANSP) );
		}
		else
			CleanupDebugShapes(hitPosShapes);
	}

	protected void CleanupDebugShapes(array<Shape> shapes)
	{
		for ( int it = 0; it < shapes.Count(); ++it )
		{
			Debug.RemoveShape( shapes[it] );
		}

		shapes.Clear();
	}
#endif
}