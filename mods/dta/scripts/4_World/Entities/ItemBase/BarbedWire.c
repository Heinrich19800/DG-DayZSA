class BarbedWire extends ItemBase
{
	// Sounds lists
	const static int		SOUNDS_SPARK_COUNT							= 4;
	const static int		SOUNDS_CUT_COUNT							= 3;
	const static int		SOUNDS_COLLISION_COUNT						= 4;
	const static int		SOUNDS_SHOCK_COUNT							= 4;
	const static float		RANDOM_SPARK_INTERVAL						= 5.0; // TO DO! Currently not used.
	
	const static string 	m_SoundsSpark[SOUNDS_SPARK_COUNT] 			= {"electricFenceSpark1", "electricFenceSpark2", "electricFenceSpark3", "electricFenceSpark4"};
	const static string 	m_SoundsCut[SOUNDS_CUT_COUNT] 				= {"barbedFenceCut1", "barbedFenceCut2", "barbedFenceCut3"};
	const static string 	m_SoundsCollision[SOUNDS_COLLISION_COUNT]	= {"barbedFenceCollision1", "barbedFenceCollision2", "barbedFenceCollision3", "barbedFenceCollision4"};
	const static string 	m_SoundsShock[SOUNDS_SHOCK_COUNT] 			= {"electricFenceShock1", "electricFenceShock2", "electricFenceShock3", "electricFenceShock4"};
	const static string 	m_SoundBuzzLoop 							= "electricFenceBuzzLoop1";
	ref protected EffectSound 	m_DeployLoopSound;
	
	SoundOnVehicle m_BuzzSoundLoop;
	
	ref Timer m_SparkEvent;
	private ref AreaDamageBase m_AreaDamage;
	
	private bool m_TriggerActive;
	private bool m_IsPlaced;
	private bool m_IsMounted;
	private bool m_IsMountedClient = -1;
	
	//mounting
	const string 				SOUND_MOUNT	= "putDown_BarbedWire_SoundSet";
	protected EffectSound 		m_MountSound;
	
	
	void BarbedWire()
	{
		m_SparkEvent 	= new Timer( CALL_CATEGORY_SYSTEM );
		m_TriggerActive = false;
		m_IsPlaced 		= false;
		
		//synchronized variables
		RegisterNetSyncVariableBool( "m_IsMounted" );	
		RegisterNetSyncVariableBool( "m_IsSoundSynchRemote" );	
	}
	
	void ~BarbedWire() {}
	
	bool IsMounted()
	{
		return m_IsMounted;
	}
	
	void SetMountedState( bool is_mounted )
	{
		m_IsMounted = is_mounted;
		
		Synchronize();
	}

	// --- SYNCHRONIZATION
	void Synchronize()
	{
		if ( GetGame().IsServer() )
		{
			SetSynchDirty();
			
			if ( GetGame().IsMultiplayer() )
			{
				RefreshParent();
			}
		}
	}
	
	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();

		//update parent (client)
		RefreshParent();

		//mounting sounds
		if ( IsMounted() != m_IsMountedClient )
		{
			m_IsMountedClient = IsMounted();
			
			//Play sound
			PlaySoundSet( m_MountSound, SOUND_MOUNT, 0.1, 0.1 );
		}
		
		if ( IsDeploySound() )
		{
			PlayDeploySound();
		}
		
		if ( CanPlayDeployLoopSound() )
		{
			PlayDeployLoopSound();
		}
					
		if ( m_DeployLoopSound && !CanPlayDeployLoopSound() )
		{
			StopDeployLoopSound();
		}
	}	

	void PlayDeployLoopSound()
	{		
		if ( GetGame().IsMultiplayer() && GetGame().IsClient() || !GetGame().IsMultiplayer() )
		{		
			m_DeployLoopSound = SEffectManager.PlaySound( GetLoopDeploySoundset(), GetPosition() );
		}
	}
	
	void StopDeployLoopSound()
	{
		if ( GetGame().IsMultiplayer() && GetGame().IsClient() || !GetGame().IsMultiplayer() )
		{	
			m_DeployLoopSound.SoundStop();
			delete m_DeployLoopSound;
		}
	}
	
	void RefreshParent()
	{
		EntityAI parent = GetHierarchyParent();
		
		//Base building objects
		BaseBuildingBase base_building = BaseBuildingBase.Cast( parent );
		if ( base_building )
		{
			base_building.Refresh();
		}
	}
	
	// --- EVENTS
	override void OnStoreSave( ParamsWriteContext ctx )
	{   
		super.OnStoreSave( ctx );
		
		//sync parts 01
		ctx.Write( m_IsMounted );
	}
	
	override void OnStoreLoad( ParamsReadContext ctx, int version )
	{
		super.OnStoreLoad( ctx, version );
		
		//Restore synced parts data
		bool mounted_state;
		ctx.Read( mounted_state );
		SetMountedState( mounted_state );
	}
	
	// ---
	override void OnWorkStart()
	{
		SoundBuzzLoopStart();
		if (m_TriggerActive)
			{ DestroyDamageTrigger(); }

		if (m_IsPlaced)
		{
			//TimerRandomSpark();
			CreateElectrifiedDamageTrigger();
		}
	}	
	
	override void OnWorkStop()
	{
		SoundBuzzLoopStop();
		if (m_TriggerActive)
			{ DestroyDamageTrigger(); }
		
		if (m_IsPlaced)
			{ CreateDamageTrigger(); }
		
		m_SparkEvent.Stop();
	}

	override void OnWork( float consumed_energy ) {}

	override void OnIsPlugged(EntityAI source_device)
	{
		SoundCut();
	}

	override void OnIsUnplugged( EntityAI last_energy_source )
	{
		if (m_TriggerActive)
			{ DestroyDamageTrigger(); }
		SoundCut();
	}
	
	override void OnInventoryEnter(Man player)
	{
		super.OnInventoryEnter(player);
		HideSelection("placing");
		ShowSelection("zbytek");
		if (m_TriggerActive)
			{ DestroyDamageTrigger(); }
		GetCompEM().UnplugThis();
		GetCompEM().UnplugAllDevices();
	}
	
	// Area Damage triggers
	// ---------------------------------------------------------			
	protected void CreateElectrifiedDamageTrigger()
	{
		m_AreaDamage = new AreaDamageRegular(this);
		m_AreaDamage.SetExtents("-1 0 -0.4", "1 0.7 0.4");
		m_AreaDamage.SetLoopInterval(0.3);
		m_AreaDamage.SetHitZones({"RightLeg", "LeftLeg", "RightFoot", "LeftFoot"});
		m_AreaDamage.SetAmmoName("MeleeDamage");
		m_AreaDamage.Spawn();
		m_TriggerActive = true;						
	}
	
	protected void CreateDamageTrigger()
	{
		m_AreaDamage = new AreaDamageOneTime(this);
		m_AreaDamage.SetExtents("-1 0 -0.4", "1 0.7 0.4");
		m_AreaDamage.SetLoopInterval(0.1);
		m_AreaDamage.SetHitZones({"RightLeg", "LeftLeg", "RightFoot", "LeftFoot"});
		m_AreaDamage.SetAmmoName("MeleeDamage");
		m_AreaDamage.Spawn();
		m_TriggerActive = true;
	}
	
	protected void DestroyDamageTrigger()
	{
		m_AreaDamage.DestroyDamageTrigger();
		m_TriggerActive = false;
	}
	// ---------------------------------------------------------
	
	// Controls spawn of random sparks
	/*
	protected void TimerRandomSpark() // TO DO: Come up with randomized functionality.
	{
		if ( GetCompEM().IsSwitchedOn() )
		{
			int plugged_devices = GetCompEM().GetEnergySource().GetCompEM().GetPluggedDevicesCount();
			float rnd_time = Math.RandomFloat(0.3, RANDOM_SPARK_INTERVAL / plugged_devices + 1.0);
			m_SparkEvent.Run(rnd_time + 0.3, this, "Spark", NULL, true);
		}
	}
	*/

	// Spawns spark particle effect and plays sound.
	void Spark()
	{
		Particle.Play( ParticleList.BARBED_WIRE_SPARKS, this);
		SoundSpark();
	}
	
	
	// SOUNDS
	// ---------------------------------------------------------
	void SoundCut()
	{
		int random_index = Math.RandomInt(0, SOUNDS_CUT_COUNT);
		string sound_type = m_SoundsCut[random_index];
		//PlaySound(sound_type, 50); // Removed as a quick fix
	}

	// Plays sound
	void SoundSpark()
	{
		int random_index = Math.RandomInt(0, SOUNDS_SPARK_COUNT);
		string sound_type = m_SoundsSpark[random_index];
		//PlaySound(sound_type, 50); // Removed as a quick fix
	}

	// Plays sound
	void SoundBuzzLoopStart()
	{
		if (!m_BuzzSoundLoop)
		{
			// m_BuzzSoundLoop = PlaySoundLoop(m_SoundBuzzLoop, 50); // Removed as a quick fix
		}
	}

	// Stops sound
	void SoundBuzzLoopStop()
	{
		if (m_BuzzSoundLoop)
		{
			GetGame().ObjectDelete(m_BuzzSoundLoop);
			m_BuzzSoundLoop = NULL;
		}
	}
	
	// Plays an electric shock sound
	void SoundElectricShock()
	{
		int random_index = Math.RandomInt(0, SOUNDS_SHOCK_COUNT);
		string sound_type = m_SoundsShock[random_index];
		//PlaySound(sound_type, 50); // Removed as a quick fix
	}
	
	// Plays a collision sound
	void SoundCollision()
	{
		int random_index = Math.RandomInt(0, SOUNDS_COLLISION_COUNT);
		string sound_type = m_SoundsCollision[random_index];
		PlaySound(sound_type, 50);
	}
	// ---------------------------------------------------------

	// Area Damage Pre/Post actions	
	// ---------------------------------------------------------
	override void PreAreaDamageActions()
	{
		if ( GetCompEM().IsPlugged() && GetCompEM().IsSwitchedOn() )
		{
			Spark();
			SoundElectricShock();
		}
		SoundCollision();
	}
	
	override void PostAreaDamageActions() {}
	// ---------------------------------------------------------	
	
	
	// TODO: proper handling can be done once the ticket DAYZ-26145 is resolved
	override void OnItemLocationChanged(EntityAI old_owner, EntityAI new_owner)
	{
		super.OnItemLocationChanged(old_owner, new_owner);

		if (m_TriggerActive)
		{
			DestroyDamageTrigger();
			m_IsPlaced = false;
		}
	}
	
	//================================================================
	// ADVANCED PLACEMENT
	//================================================================
	
	override void OnPlacementComplete( Man player )
	{
		super.OnPlacementComplete( player );
		
		if ( GetGame().IsServer() )
		{
			ShowAllSelections();
			HideSelection("zbytek");
	
			if (!GetHierarchyParent())
			{
				if (GetCompEM().IsPlugged() && GetCompEM().IsWorking() )
					{ CreateElectrifiedDamageTrigger(); }
				else
					{ CreateDamageTrigger(); }
				m_IsPlaced = true;
			}
		}	
		
		SetIsDeploySound( true );
	}
	
	override string GetDeploySoundset()
	{
		return "placeBarbedWire_SoundSet";
	}
	
	override string GetLoopDeploySoundset()
	{
		return "barbedwire_deploy_SoundSet";
	}	
}