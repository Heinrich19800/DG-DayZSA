class ConstructionActionData
{
	Object m_Target;
	
	//base building
	ref array<ConstructionPart> m_BuildParts;
	int 						m_PartIndex;
	string 						m_MainPartName;
	ref ConstructionPart 		m_TargetPart;
	
	//combination lock
	CombinationLock 			m_CombinationLock;
	int 						m_DialIndex;
	
	//attaching
	int 						m_SlotId;
	//detaching
	ref array<EntityAI> 		m_Attachments;
	int 						m_AttachmentsIndex;
	
	void ConstructionActionData()
	{
		m_BuildParts = new ref array<ConstructionPart>;
		m_DialIndex = 0;
		
		m_Attachments = new ref array<EntityAI>;
		m_AttachmentsIndex = 0;
	}
	
	//************************************************/
	//  Base building
	//************************************************/
	string GetMainPartName()
	{
		return m_MainPartName;
	}
	
	void SetTarget( Object target )
	{
		m_Target = target;
	}
	
	Object GetTarget()
	{
		return m_Target;
	}
	
	void SetTargetPart( ConstructionPart target_part )
	{
		m_TargetPart = target_part;
	}

	ConstructionPart GetTargetPart()
	{
		return m_TargetPart;
	}
	
	void SetSlotId( int slot_id )
	{
		m_SlotId = slot_id;
	}
	
	int GetSlotId()
	{
		return m_SlotId;
	}
	
	void SetNextIndex()
	{
		if ( m_BuildParts.Count() > 1 )
		{
			if ( m_PartIndex <= m_BuildParts.Count() - 2 )
			{
				m_PartIndex++;
			}
			else if ( m_PartIndex >= m_BuildParts.Count() >  - 1 )
			{
				m_PartIndex = 0;
			}
		}
		else
		{
			m_PartIndex = 0;
		}
	}
	
	void RefreshPartsToBuild( string main_part_name, ItemBase tool )
	{
		m_MainPartName = main_part_name;
		BaseBuildingBase base_building_object = BaseBuildingBase.Cast( m_Target );
		if ( base_building_object )
		{
			base_building_object.GetConstruction().GetConstructionPartsToBuild( m_MainPartName, m_BuildParts, tool );
		}
	}
	
	int GetConstructionPartsCount()
	{
		return m_BuildParts.Count();
	}
	
	ConstructionPart GetCurrentBuildPart()
	{
		BaseBuildingBase base_building_object = BaseBuildingBase.Cast( m_Target );
		
		if ( base_building_object )
		{
			Construction construction = base_building_object.GetConstruction();
			
			if ( construction )
			{
				if ( m_BuildParts.Count() > 0 ) 
				{
					m_PartIndex = Math.Clamp( m_PartIndex, 0, m_BuildParts.Count() - 1 );
					
					return m_BuildParts.Get( m_PartIndex );
				}
			}
		}
		
		return NULL;
	}

	//************************************************/
	//  Combination lock
	//************************************************/	
	int GetDialIndex()
	{
		return m_DialIndex;
	}
	
	CombinationLock GetCombinationLock()
	{
		return m_CombinationLock;
	}
	
	void SetCombinationLock( CombinationLock combination_lock )
	{
		m_CombinationLock = CombinationLock.Cast( combination_lock );
	}
		
	void SetNextDialIndex()
	{
		if ( m_CombinationLock.COMBINATION_LENGTH > 1 )
		{
			if ( m_DialIndex <= m_CombinationLock.COMBINATION_LENGTH - 2 )
			{
				m_DialIndex++;
			}
			else if ( m_DialIndex >= m_CombinationLock.COMBINATION_LENGTH >  - 1 )
			{
				m_DialIndex = 0;
			}
		}
		else
		{
			m_DialIndex = 0;
		}
	}

	string GetDialNumberText()
	{
		string dial_text;
		string combination_text = m_CombinationLock.m_Combination.ToString();
		
		//insert zeros to dials with 0 value
		int length_diff = m_CombinationLock.COMBINATION_LENGTH - combination_text.Length();
		for ( int i = 0; i < length_diff; ++i )
		{
			combination_text = "0" + combination_text;
		}
		
		//assemble the whole combination with selected part
		for ( int j = 0; j < m_CombinationLock.COMBINATION_LENGTH; ++j )
		{
			if ( j == m_DialIndex )
			{
				dial_text += string.Format( "[%1]", combination_text.Get( j ) );
			}
			else
			{
				dial_text += string.Format( " %1 ", combination_text.Get( j ) );
			}
		}
		
		return dial_text;
	}
	
	
	//************************************************/
	//  Attach/Detach actions
	//************************************************/		
	int GetAttachmentSlotFromSelection( EntityAI target, ItemBase item_to_attach, string selection )
	{
		string cfg_path = "cfgVehicles" + " " + target.GetType() + " "+ "GUIInventoryAttachmentsProps";
		
		if ( GetGame().ConfigIsExisting( cfg_path ) )
		{
			int	child_count = GetGame().ConfigGetChildrenCount( cfg_path );
			
			for ( int i = 0; i < child_count; i++ )
			{
				string child_name;
				GetGame().ConfigGetChildName( cfg_path, i, child_name );
				
				string child_selection;
				GetGame().ConfigGetText( cfg_path + " " + child_name + " " + "selection", child_selection );
				
				if ( selection == child_selection )
				{
					ref array<string> attachment_slots = new array<string>;
					GetGame().ConfigGetTextArray( cfg_path + " " + child_name + " " + "attachmentSlots", attachment_slots );
					
					for ( int j = 0; j < attachment_slots.Count(); ++j )
					{
						int target_slot_id = InventorySlots.GetSlotIdFromString( attachment_slots.Get( j ) );
						int item_slot_count = item_to_attach.GetInventory().GetSlotIdCount();
						
						for ( int k = 0; k < item_slot_count; ++k )
						{
							int item_slot_id = item_to_attach.GetInventory().GetSlotId( k );
							
							if ( target_slot_id == item_slot_id )
							{
								return item_slot_id;
							}
						}
					}
				}
			}
		}
		
		return -1;
	}
	
	void GetAttachmentsFromSelection( EntityAI target, string selection, out array<EntityAI> attachments )
	{
		attachments.Clear();		//clear output
		
		string cfg_path = "cfgVehicles" + " " + target.GetType() + " "+ "GUIInventoryAttachmentsProps";
		if ( GetGame().ConfigIsExisting( cfg_path ) )
		{
			int	child_count = GetGame().ConfigGetChildrenCount( cfg_path );
			
			for ( int i = 0; i < child_count; i++ )
			{
				string child_name;
				GetGame().ConfigGetChildName( cfg_path, i, child_name );
				
				string child_selection;
				GetGame().ConfigGetText( cfg_path + " " + child_name + " " + "selection", child_selection );
				
				if ( selection == child_selection )
				{
					ref array<string> attachment_slots = new array<string>;
					GetGame().ConfigGetTextArray( cfg_path + " " + child_name + " " + "attachmentSlots", attachment_slots );
					
					for ( int j = 0; j < attachment_slots.Count(); ++j )
					{
						int target_slot_id = InventorySlots.GetSlotIdFromString( attachment_slots.Get( j ) );
						
						//is attached and can be detached
						EntityAI attachment = target.GetInventory().FindAttachment( target_slot_id );
						if ( attachment && target.GetInventory().CanRemoveAttachmentEx( attachment, target_slot_id ) && !target.GetInventory().GetSlotLock( target_slot_id ) )
						{
							attachments.Insert( attachment );
						}
					}
				}
			}
		}
	}
	
	void CombineItems( ItemBase target, ItemBase item )
	{
		if ( target.ConfigGetBool( "canBeSplit" ) && item && !target.IsFullQuantity() )
		{
			int quantity_used = target.ComputeQuantityUsed( item, true );
			if( quantity_used != 0 )
			{
				target.AddQuantity( quantity_used );
				item.AddQuantity( -quantity_used );
			}
		}
	}
	
	void RefreshAttachmentsToDetach( EntityAI target, string main_part_name )
	{
		GetAttachmentsFromSelection( target, main_part_name, m_Attachments );
	}
	
	void SetNextAttachmentIndex()
	{
		if ( GetAttachmentsToDetachCount() > 1 )
		{
			if ( m_AttachmentsIndex <= GetAttachmentsToDetachCount() - 2 )
			{
				m_AttachmentsIndex++;
			}
			else if ( m_AttachmentsIndex >= GetAttachmentsToDetachCount() >  - 1 )
			{
				m_AttachmentsIndex = 0;
			}
		}
		else
		{
			m_AttachmentsIndex = 0;
		}
	}
		
	int GetAttachmentsToDetachCount()
	{
		return m_Attachments.Count();
	}	
		
	EntityAI GetActualAttachmentToDetach()
	{
		if ( GetAttachmentsToDetachCount() > 0 )
		{
			m_AttachmentsIndex = Math.Clamp( m_AttachmentsIndex, 0, GetAttachmentsToDetachCount() - 1 );
			
			if ( m_Attachments && GetAttachmentsToDetachCount() > ( m_AttachmentsIndex ) )
			{
				return m_Attachments.Get( m_AttachmentsIndex );
			}		
		}

		return NULL;
	}
}