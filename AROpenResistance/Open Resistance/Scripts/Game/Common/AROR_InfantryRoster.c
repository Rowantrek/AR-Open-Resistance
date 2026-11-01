//! Server: the members of an enemy (Occupier or Invader) infantry group, shared by QRFs, patrols and missions so every
//! system fields the same soldiers. When the role's preset dresses its own infantry (AROR_FactionPreset.DressesInfantry,
//! e.g. the WCS armies) the authored characters are replaced by the preset's units at fire team positions, so they are
//! dressed by Readiness (AROR_AIOutfitter). In every group some members then become AT or AA specialists: one per
//! AROR_FactionPresetsComponent "members per specialist", alternating AT and AA when the preset has both.
class AROR_InfantryRoster
{
	//------------------------------------------------------------------------------------------------
	//! The unit type at a position of a fire team: a leader, then automatic rifleman, grenadier, rifleman, machine gunner,
	//! medic, rifleman and marksman, repeating for larger groups
	static AROR_EUnitType TeamPosition(int index)
	{
		if (index == 0)
			return AROR_EUnitType.LEADER;

		switch ((index - 1) % 7)
		{
			case 0: return AROR_EUnitType.AUTORIFLEMAN;
			case 1: return AROR_EUnitType.GRENADIER;
			case 2: return AROR_EUnitType.RIFLEMAN;
			case 3: return AROR_EUnitType.MACHINEGUNNER;
			case 4: return AROR_EUnitType.MEDIC;
			case 5: return AROR_EUnitType.RIFLEMAN;
		}

		return AROR_EUnitType.MARKSMAN;
	}

	//------------------------------------------------------------------------------------------------
	//! Rewrites authored members (e.g. a QRF group prefab's slots) for the role, keeping their count. assetFactionKey is
	//! the native faction of the authored characters: another army's preset that does not dress its own infantry is
	//! left out, since its characters would fail the authored faction checks.
	static void Apply_S(AROR_EFactionRole role, notnull array<ResourceName> members, string assetFactionKey = string.Empty)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!Replication.IsServer() || !presets || members.IsEmpty() || !IsEnemy(role))
			return;

		AROR_FactionPreset preset = presets.GetPreset(role);
		if (!preset)
			return;

		bool dresses = preset.DressesInfantry();
		string army = preset.GetCatalogFactionKey();
		if (!dresses && !assetFactionKey.IsEmpty() && !army.IsEmpty() && army != assetFactionKey)
			return;

		if (dresses)
		{
			for (int index = 0; index < members.Count(); index++)
			{
				ResourceName unit = UnitOf(presets, preset, role, TeamPosition(index));
				if (!unit.IsEmpty())
					members[index] = unit;
			}
		}

		AddSpecialists_S(role, members);
	}

	//------------------------------------------------------------------------------------------------
	//! Turns members into AT and AA specialists, from the last member backwards; the first member (the leader) stays
	static void AddSpecialists_S(AROR_EFactionRole role, notnull array<ResourceName> members)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!Replication.IsServer() || !presets || !IsEnemy(role))
			return;

		int count = presets.GetSpecialistCount(members.Count());
		if (count < 1)
			return;

		ResourceName antiTank = SpecialistPrefab(role, AROR_EUnitType.AT);
		ResourceName antiAir = SpecialistPrefab(role, AROR_EUnitType.AA);
		if (antiTank.IsEmpty() && antiAir.IsEmpty())
			return;

		bool both = !antiTank.IsEmpty() && !antiAir.IsEmpty();
		bool nextAntiAir = antiTank.IsEmpty() || (both && Math.RandomInt(0, 2) == 1);
		int index = members.Count() - 1;
		for (int i = 0; i < count && index > 0; i++)
		{
			if (nextAntiAir)
				members[index] = antiAir;
			else
				members[index] = antiTank;

			if (both)
				nextAntiAir = !nextAntiAir;

			index--;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The role's AT or AA soldier, or empty when its preset has no unit of the type
	static ResourceName SpecialistPrefab(AROR_EFactionRole role, AROR_EUnitType type)
	{
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
			return string.Empty;

		AROR_FactionPreset preset = presets.GetPreset(role);
		if (!preset || !preset.GetUnit(type))
			return string.Empty;

		return presets.GetUnitPrefab(role, type);
	}

	//------------------------------------------------------------------------------------------------
	//! The preset's unit of the type, else its rifleman (each the role default when the unit names no character)
	protected static ResourceName UnitOf(AROR_FactionPresetsComponent presets, AROR_FactionPreset preset, AROR_EFactionRole role, AROR_EUnitType type)
	{
		if (!preset.GetUnit(type))
			type = AROR_EUnitType.RIFLEMAN;

		return presets.GetUnitPrefab(role, type);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsEnemy(AROR_EFactionRole role)
	{
		return role == AROR_EFactionRole.OCCUPIERS || role == AROR_EFactionRole.INVADERS;
	}
}
