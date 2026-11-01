//! Sorts faction entity catalog entries (characters, vehicles, static weapons, inventory items) into faction preset
//! categories and types, using the base game's own tags (Game Master labels for characters and vehicles, arsenal item
//! type/mode for items) plus a few file-name hints. Shared by the runtime catalog augmentation (AROR_FactionPreset), the
//! catalog generation of content packs (AROR_ContentPack) and the Workbench import of the base presets, so all sort the
//! same way. Its default readiness and import terms price generated content pack entries whose type no other pack lists.
class AROR_CatalogClassifier
{
	//------------------------------------------------------------------------------------------------
	//! Category and type of a catalog entry; false if it fits no preset type
	static bool Classify(EEntityCatalogType catalogType, SCR_EntityCatalogEntry entry, out AROR_EFactionPresetCategory category, out int type)
	{
		switch (catalogType)
		{
			case EEntityCatalogType.CHARACTER:
				category = AROR_EFactionPresetCategory.UNITS;
				type = UnitType(entry);
				return type >= 0;

			case EEntityCatalogType.VEHICLE:
				category = AROR_EFactionPresetCategory.VEHICLES;
				type = VehicleType(entry);
				return type >= 0;

			case EEntityCatalogType.WEAPONS_TRIPOD:
				category = AROR_EFactionPresetCategory.VEHICLES;
				type = StaticWeaponType(entry);
				return true;

			case EEntityCatalogType.ITEM:
				return ItemType(entry, category, type);
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Readiness from which a generated content pack entry may be fielded: heavier equipment later. vehicleMunition: an
	//! item loaded onto aircraft and vehicles (see IsVehicleMunition).
	static int DefaultReadiness(AROR_EFactionPresetCategory category, int type, bool vehicleMunition = false)
	{
		if (vehicleMunition)
			return 4;

		if (category != AROR_EFactionPresetCategory.VEHICLES)
			return 1;

		switch (type)
		{
			case AROR_EVehicleType.ARMED_CAR:
			case AROR_EVehicleType.ARMED_TRUCK:
			case AROR_EVehicleType.HELI_TRANSPORT:
			case AROR_EVehicleType.STATIC_AT:
			case AROR_EVehicleType.STATIC_MORTAR:
				return 2;

			case AROR_EVehicleType.APC:
			case AROR_EVehicleType.IFV:
			case AROR_EVehicleType.AA_VEHICLE:
			case AROR_EVehicleType.RADAR_AA_VEHICLE:
			case AROR_EVehicleType.STATIC_AA:
				return 3;

			case AROR_EVehicleType.HELI_ATTACK:
				return 4;

			case AROR_EVehicleType.TANK:
				return 5;
		}

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Base cost and import facility requirements for a generated content pack entry whose type no other pack lists
	//! (generated entries otherwise copy a listed entry of the same type). Same scale as the base game pack.
	static void DefaultImportTerms(AROR_EFactionPresetCategory category, int type, out int cost, out int importPoints, out AROR_EImportPointRequirement importRequirement, bool vehicleMunition = false)
	{
		cost = 0;
		importPoints = 0;
		importRequirement = AROR_EImportPointRequirement.NONE;
		if (vehicleMunition)
		{
			cost = 3800;
			importPoints = 5;
			importRequirement = AROR_EImportPointRequirement.AIR;
			return;
		}

		switch (category)
		{
			case AROR_EFactionPresetCategory.VEHICLES:
				DefaultVehicleTerms(type, cost, importPoints, importRequirement);
				return;

			case AROR_EFactionPresetCategory.WEAPONS:
				DefaultWeaponTerms(type, cost, importPoints, importRequirement);
				return;

			case AROR_EFactionPresetCategory.WEAPON_ACCESSORIES:
				cost = 400;
				importPoints = 2;
				return;

			case AROR_EFactionPresetCategory.AMMUNITION:
				DefaultAmmunitionTerms(type, cost, importPoints, importRequirement);
				return;

			case AROR_EFactionPresetCategory.GEAR:
				cost = 100;
				return;

			case AROR_EFactionPresetCategory.EQUIPMENT:
				cost = 90;
				importPoints = 1;
				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void DefaultVehicleTerms(int type, out int cost, out int importPoints, out AROR_EImportPointRequirement importRequirement)
	{
		importRequirement = AROR_EImportPointRequirement.PORT;
		switch (type)
		{
			case AROR_EVehicleType.MOTOR_CAR: cost = 1800; importPoints = 1; importRequirement = AROR_EImportPointRequirement.NONE; return;
			case AROR_EVehicleType.BOAT: cost = 2500; importPoints = 1; return;
			case AROR_EVehicleType.AMBULANCE: cost = 3000; importPoints = 2; return;
			case AROR_EVehicleType.MOTOR_TRUCK:
			case AROR_EVehicleType.SUPPLY_TRUCK: cost = 3400; importPoints = 2; return;
			case AROR_EVehicleType.FUEL_TRUCK: cost = 4200; importPoints = 2; return;
			case AROR_EVehicleType.ARMED_CAR: cost = 4800; importPoints = 2; return;
			case AROR_EVehicleType.ARMED_TRUCK: cost = 6200; importPoints = 3; return;
			case AROR_EVehicleType.AA_VEHICLE: cost = 18000; importPoints = 4; return;
			case AROR_EVehicleType.RADAR_AA_VEHICLE: cost = 26000; importPoints = 5; return;
			case AROR_EVehicleType.APC: cost = 14500; importPoints = 4; return;
			case AROR_EVehicleType.IFV: cost = 22000; importPoints = 5; return;
			case AROR_EVehicleType.TANK: cost = 40000; importPoints = 6; return;
			case AROR_EVehicleType.HELI_TRANSPORT: cost = 26000; importPoints = 4; importRequirement = AROR_EImportPointRequirement.AIR; return;
			case AROR_EVehicleType.HELI_ATTACK: cost = 60000; importPoints = 6; importRequirement = AROR_EImportPointRequirement.AIR; return;
		}

		// Static weapons are not sold (Imports skips them)
		cost = 0;
		importPoints = 0;
		importRequirement = AROR_EImportPointRequirement.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DefaultWeaponTerms(int type, out int cost, out int importPoints, out AROR_EImportPointRequirement importRequirement)
	{
		importRequirement = AROR_EImportPointRequirement.NONE;
		importPoints = 1;
		switch (type)
		{
			case AROR_EWeaponType.PISTOL: cost = 150; return;
			case AROR_EWeaponType.SHOTGUN: cost = 400; return;
			case AROR_EWeaponType.SMG: cost = 500; return;
			case AROR_EWeaponType.EXPLOSIVE_GRENADE: cost = 60; return;
			case AROR_EWeaponType.SMOKE_GRENADE: cost = 40; return;
			case AROR_EWeaponType.MARKSMAN_RIFLE: cost = 1200; importPoints = 2; return;
			case AROR_EWeaponType.MACHINEGUN: cost = 2200; importPoints = 3; return;
			case AROR_EWeaponType.SNIPER_RIFLE: cost = 1800; importPoints = 3; return;
			case AROR_EWeaponType.LAUNCHER_AT: cost = 1800; importPoints = 1; importRequirement = AROR_EImportPointRequirement.PORT; return;
			case AROR_EWeaponType.LAUNCHER_AA: cost = 2400; importPoints = 1; importRequirement = AROR_EImportPointRequirement.PORT; return;
		}

		// Rifles and carbines
		cost = 600;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DefaultAmmunitionTerms(int type, out int cost, out int importPoints, out AROR_EImportPointRequirement importRequirement)
	{
		importRequirement = AROR_EImportPointRequirement.NONE;
		importPoints = 1;
		switch (type)
		{
			case AROR_EAmmunitionType.MAGAZINE: cost = 25; importPoints = 0; return;
			case AROR_EAmmunitionType.GRENADE_ROUND: cost = 60; return;
			case AROR_EAmmunitionType.SHELL: cost = 150; importPoints = 3; return;
			case AROR_EAmmunitionType.ROCKET: cost = 350; importPoints = 1; importRequirement = AROR_EImportPointRequirement.PORT; return;
			case AROR_EAmmunitionType.MISSILE: cost = 900; importPoints = 1; importRequirement = AROR_EImportPointRequirement.PORT; return;
		}

		// Belts
		cost = 90;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab exists in one of the addon filesystems (e.g. ArmaReforger, RHS_Core)
	static bool IsFromSource(ResourceName prefab, array<string> sources)
	{
		if (!sources)
			return false;

		string path = prefab.GetPath();
		foreach (string source : sources)
		{
			if (!source.IsEmpty() && FileIO.FileExists("$" + source + ":" + path))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab's lower-case file name contains one of the fragments
	static bool HasNameFragment(ResourceName prefab, array<string> fragments)
	{
		if (!fragments)
			return false;

		string name = LowerName(prefab);
		foreach (string fragment : fragments)
		{
			if (!fragment.IsEmpty() && name.Contains(fragment))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A hand-held weapon, accessory or ammunition from no excluded source and with no excluded name fragment
	static bool IsHandHeldFromAllowedSource(AROR_EFactionPresetCategory category, ResourceName prefab, array<string> excludedSources, array<string> excludedNames)
	{
		if (category != AROR_EFactionPresetCategory.WEAPONS && category != AROR_EFactionPresetCategory.WEAPON_ACCESSORIES
			&& category != AROR_EFactionPresetCategory.AMMUNITION)
			return false;

		return !HasNameFragment(prefab, excludedNames) && !IsFromSource(prefab, excludedSources);
	}

	//------------------------------------------------------------------------------------------------
	//! Lower-case file name of a prefab, for name hints
	static string LowerName(ResourceName prefab)
	{
		string path = prefab;
		int slash = path.LastIndexOf("/");
		if (slash >= 0)
			path = path.Substring(slash + 1, path.Length() - slash - 1);

		path.ToLower();
		return path;
	}

	//------------------------------------------------------------------------------------------------
	// Characters
	//------------------------------------------------------------------------------------------------
	static int UnitType(SCR_EntityCatalogEntry entry)
	{
		string name = LowerName(entry.GetPrefab());

		// Roles the labels cannot tell apart are recognised by file name
		if (name.Contains("pilot") || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_HELI_CREW))
			return AROR_EUnitType.PILOT;
		if (name.Contains("crew") || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_VEHICLE_CREW))
			return AROR_EUnitType.CREW;
		if (name.Contains("officer"))
			return AROR_EUnitType.OFFICER;
		if (name.Contains("sniper"))
			return AROR_EUnitType.SNIPER;
		if (name.Contains("_aa.") || name.Contains("_aa_") || name.Contains("antiair"))
			return AROR_EUnitType.AA;
		if (name.Contains("_ar.") || name.Contains("_ar_") || name.Contains("autorifle"))
			return AROR_EUnitType.AUTORIFLEMAN;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.FACTION_CIV) || name.Contains("civilian"))
			return AROR_EUnitType.CIVILIAN;

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_LEADER))
			return AROR_EUnitType.LEADER;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_MEDIC))
			return AROR_EUnitType.MEDIC;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_ANTITANK))
			return AROR_EUnitType.AT;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_MACHINEGUNNER))
			return AROR_EUnitType.MACHINEGUNNER;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_GRENADIER))
			return AROR_EUnitType.GRENADIER;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_SHARPSHOOTER))
			return AROR_EUnitType.MARKSMAN;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_SAPPER))
			return AROR_EUnitType.ENGINEER;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_RADIOOPERATOR))
			return AROR_EUnitType.RADIO_OPERATOR;
		if (entry.HasEditableEntityLabel(EEditableEntityLabel.ROLE_RIFLEMAN))
			return AROR_EUnitType.RIFLEMAN;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	// Vehicles
	//------------------------------------------------------------------------------------------------
	static int VehicleType(SCR_EntityCatalogEntry entry)
	{
		string name = LowerName(entry.GetPrefab());

		if (name.Contains("boat"))
			return AROR_EVehicleType.BOAT;

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TURRET))
			return StaticWeaponType(entry);

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_HELICOPTER))
		{
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED))
				return AROR_EVehicleType.HELI_ATTACK;

			return AROR_EVehicleType.HELI_TRANSPORT;
		}

		// RHS tanks use VEHICLE_APC because the native label enum has no dedicated tank label.
		// Recognise the hull before that broad label; fuel tankers must remain trucks.
		if (IsTankName(name))
			return AROR_EVehicleType.TANK;

		// Self-propelled anti-air (Shilka, Tunguska, ...) before the broad APC label they often carry
		if (IsAntiAirName(name) && !entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_CAR) && !entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK))
		{
			if (name.Contains("tunguska") || name.Contains("2s6") || name.Contains("pantsir"))
				return AROR_EVehicleType.RADAR_AA_VEHICLE;

			return AROR_EVehicleType.AA_VEHICLE;
		}

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_APC))
		{
			// Tracked/armoured fighting vehicles carrying an autocannon are treated as IFVs by name
			if (name.Contains("bmp") || name.Contains("ifv"))
				return AROR_EVehicleType.IFV;

			return AROR_EVehicleType.APC;
		}

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_TRUCK))
		{
			// Armed trucks, e.g. the WCS ZU-23 carriers, are support vehicles, not troop/supply transports
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED))
			{
				if (IsAntiAirName(name))
					return AROR_EVehicleType.AA_VEHICLE;

				return AROR_EVehicleType.ARMED_TRUCK;
			}
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_REFUELING))
				return AROR_EVehicleType.FUEL_TRUCK;
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_MEDICAL))
				return AROR_EVehicleType.AMBULANCE;
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_SUPPLYSTORAGE_SMALL) || entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_SUPPLYSTORAGE_LARGE)
				|| entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_LOGISTICS))
				return AROR_EVehicleType.SUPPLY_TRUCK;

			return AROR_EVehicleType.MOTOR_TRUCK;
		}

		if (entry.HasEditableEntityLabel(EEditableEntityLabel.VEHICLE_CAR))
		{
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_MEDICAL))
				return AROR_EVehicleType.AMBULANCE;
			if (entry.HasEditableEntityLabel(EEditableEntityLabel.TRAIT_ARMED))
				return AROR_EVehicleType.ARMED_CAR;

			return AROR_EVehicleType.MOTOR_CAR;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsTankName(string name)
	{
		return (name.Contains("tank") && !name.Contains("tanker")) || name.Contains("m1a")
			|| name.Contains("t14") || name.Contains("t-14") || name.Contains("t_14")
			|| name.Contains("t72") || name.Contains("t-72") || name.Contains("t_72")
			|| name.Contains("t80") || name.Contains("t-80") || name.Contains("t_80")
			|| name.Contains("t90") || name.Contains("t-90") || name.Contains("t_90");
	}

	//------------------------------------------------------------------------------------------------
	//! Anti-air vehicles by name: ZU-23 carriers, Shilka, Tunguska and similar
	static bool IsAntiAirName(string name)
	{
		return name.Contains("zu23") || name.Contains("zu-23") || name.Contains("zu_23") || name.Contains("shilka") || name.Contains("zsu")
			|| name.Contains("tunguska") || name.Contains("2s6") || name.Contains("pantsir") || name.Contains("avenger");
	}

	//------------------------------------------------------------------------------------------------
	static int StaticWeaponType(SCR_EntityCatalogEntry entry)
	{
		string name = LowerName(entry.GetPrefab());
		if (name.Contains("zu23") || name.Contains("zu-23") || name.Contains("zu_23")
			|| name.Contains("stinger") || name.Contains("igla") || name.Contains("strela"))
			return AROR_EVehicleType.STATIC_AA;
		if (name.Contains("9m113") || name.Contains("9k111") || name.Contains("tow")
			|| name.Contains("spg9") || name.Contains("spg_9") || name.Contains("atgm") || name.Contains("metis"))
			return AROR_EVehicleType.STATIC_AT;
		if (name.Contains("mortar") || name.Contains("m252") || name.Contains("2b14"))
			return AROR_EVehicleType.STATIC_MORTAR;
		// As the base presets: the tall 6T7 tripods are high, other tripods low
		if (name.Contains("6t7"))
			return AROR_EVehicleType.STATIC_MG_HIGH;
		return AROR_EVehicleType.STATIC_MG_LOW;
	}

	//------------------------------------------------------------------------------------------------
	// Items: weapons, ammunition, accessories, gear and equipment, from the arsenal data
	//------------------------------------------------------------------------------------------------
	//! True for munitions loaded onto aircraft and vehicles as items (rocket pods, missiles, bombs). They share the
	//! ammunition types with hand-held rockets, so generated pack entries price them apart (see DefaultImportTerms).
	static bool IsVehicleMunition(SCR_EntityCatalogEntry entry)
	{
		SCR_ArsenalItem arsenal = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
		if (!arsenal)
			return false;

		SCR_EArsenalItemType itemType = arsenal.GetItemType();
		return arsenal.GetItemMode() == SCR_EArsenalItemMode.PYLON || itemType == SCR_EArsenalItemType.HELICOPTER || itemType == SCR_EArsenalItemType.VEHICLE;
	}

	//------------------------------------------------------------------------------------------------
	static bool ItemType(SCR_EntityCatalogEntry entry, out AROR_EFactionPresetCategory category, out int type)
	{
		SCR_ArsenalItem arsenal = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
		if (!arsenal)
			return false;

		string name = LowerName(entry.GetPrefab());
		SCR_EArsenalItemType itemType = arsenal.GetItemType();
		SCR_EArsenalItemMode itemMode = arsenal.GetItemMode();

		// Munitions loaded onto aircraft and vehicles as items (e.g. helicopter rocket pods): ammunition, priced apart
		if (IsVehicleMunition(entry))
		{
			category = AROR_EFactionPresetCategory.AMMUNITION;
			type = AROR_EAmmunitionType.ROCKET;
			if (name.Contains("missile") || name.Contains("atgm"))
				type = AROR_EAmmunitionType.MISSILE;
			else if (name.Contains("bomb"))
				type = AROR_EAmmunitionType.BOMB;

			return true;
		}

		if (itemMode == SCR_EArsenalItemMode.AMMUNITION)
		{
			category = AROR_EFactionPresetCategory.AMMUNITION;
			type = AmmunitionType(itemType, name);
			return true;
		}

		if (itemMode == SCR_EArsenalItemMode.ATTACHMENT || itemType == SCR_EArsenalItemType.WEAPON_ATTACHMENT)
		{
			category = AROR_EFactionPresetCategory.WEAPON_ACCESSORIES;
			type = AccessoryType(name);
			return type >= 0;
		}

		type = WeaponType(itemType, name);
		if (type >= 0)
		{
			category = AROR_EFactionPresetCategory.WEAPONS;
			return true;
		}

		type = GearType(itemType, name);
		if (type >= 0)
		{
			category = AROR_EFactionPresetCategory.GEAR;
			return true;
		}

		type = EquipmentType(itemType, name);
		if (type >= 0)
		{
			category = AROR_EFactionPresetCategory.EQUIPMENT;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static int WeaponType(SCR_EArsenalItemType itemType, string name = "")
	{
		switch (itemType)
		{
			case SCR_EArsenalItemType.PISTOL:
				return AROR_EWeaponType.PISTOL;
			case SCR_EArsenalItemType.RIFLE:
				return AROR_EWeaponType.ASSAULT_RIFLE;
			case SCR_EArsenalItemType.MACHINE_GUN:
				return AROR_EWeaponType.MACHINEGUN;
			case SCR_EArsenalItemType.SNIPER_RIFLE:
				return AROR_EWeaponType.SNIPER_RIFLE;
			case SCR_EArsenalItemType.ROCKET_LAUNCHER:
				if (name.Contains("stinger") || name.Contains("fim92") || name.Contains("igla") || name.Contains("9k38") || name.Contains("strela"))
					return AROR_EWeaponType.LAUNCHER_AA;
				return AROR_EWeaponType.LAUNCHER_AT;
			case SCR_EArsenalItemType.LETHAL_THROWABLE:
				return AROR_EWeaponType.EXPLOSIVE_GRENADE;
			case SCR_EArsenalItemType.NON_LETHAL_THROWABLE:
				return AROR_EWeaponType.SMOKE_GRENADE;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static int AmmunitionType(SCR_EArsenalItemType itemType, string name)
	{
		if (itemType == SCR_EArsenalItemType.ROCKET_LAUNCHER)
			return AROR_EAmmunitionType.ROCKET;
		if (itemType == SCR_EArsenalItemType.MORTARS)
			return AROR_EAmmunitionType.SHELL;
		if (name.Contains("40mm") || name.Contains("vog") || name.Contains("gp25") || name.Contains("m203") || name.Contains("grenade"))
			return AROR_EAmmunitionType.GRENADE_ROUND;
		if (itemType == SCR_EArsenalItemType.MACHINE_GUN || name.Contains("belt") || name.Contains("box"))
			return AROR_EAmmunitionType.BELT;

		return AROR_EAmmunitionType.MAGAZINE;
	}

	//------------------------------------------------------------------------------------------------
	static int AccessoryType(string name)
	{
		if (name.Contains("suppressor") || name.Contains("silencer") || name.Contains("pbs"))
			return AROR_EWeaponAccessoryType.SUPPRESSOR;
		if (name.Contains("bayonet"))
			return AROR_EWeaponAccessoryType.BAYONET;
		if (name.Contains("ugl") || name.Contains("m203") || name.Contains("gp25") || name.Contains("bipod"))
			return AROR_EWeaponAccessoryType.UNDERBARREL;
		if (name.Contains("flashlight") || name.Contains("light"))
			return AROR_EWeaponAccessoryType.LIGHT;
		if (name.Contains("optic") || name.Contains("scope") || name.Contains("sight") || name.Contains("artii") || name.Contains("pso") || name.Contains("kobra") || name.Contains("_pu"))
			return AROR_EWeaponAccessoryType.OPTIC;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static int GearType(SCR_EArsenalItemType itemType, string name)
	{
		if (itemType == SCR_EArsenalItemType.HEADWEAR)
		{
			if (name.Contains("helmet") || name.Contains("pasgt") || name.Contains("ssh") || name.Contains("6b"))
				return AROR_EGearType.HELMET;
			if (name.Contains("glasses") || name.Contains("goggles") || name.Contains("balaclava") || name.Contains("mask"))
				return AROR_EGearType.FACEWEAR;

			return AROR_EGearType.HEADGEAR;
		}

		// Reforger splits clothing into jacket, trousers, boots and gloves; all count as uniform
		if (itemType == SCR_EArsenalItemType.TORSO || itemType == SCR_EArsenalItemType.LEGS
			|| itemType == SCR_EArsenalItemType.FOOTWEAR || itemType == SCR_EArsenalItemType.HANDWEAR)
			return AROR_EGearType.UNIFORM;

		if (itemType == SCR_EArsenalItemType.VEST_AND_WAIST)
			return AROR_EGearType.VEST;

		if (itemType == SCR_EArsenalItemType.BACKPACK || itemType == SCR_EArsenalItemType.RADIO_BACKPACK)
			return AROR_EGearType.BACKPACK;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static int EquipmentType(SCR_EArsenalItemType itemType, string name)
	{
		if (itemType == SCR_EArsenalItemType.HEAL)
			return AROR_EEquipmentType.MEDICAL;
		if (itemType == SCR_EArsenalItemType.EXPLOSIVES)
			return AROR_EEquipmentType.EXPLOSIVE;
		if (itemType != SCR_EArsenalItemType.EQUIPMENT)
			return -1;

		if (name.Contains("binocular"))
			return AROR_EEquipmentType.BINOCULARS;
		if (name.Contains("radio"))
			return AROR_EEquipmentType.RADIO;
		if (name.Contains("map"))
			return AROR_EEquipmentType.MAP;
		if (name.Contains("compass"))
			return AROR_EEquipmentType.COMPASS;
		if (name.Contains("watch"))
			return AROR_EEquipmentType.WATCH;
		if (name.Contains("flashlight") || name.Contains("torch"))
			return AROR_EEquipmentType.FLASHLIGHT;

		return AROR_EEquipmentType.TOOL;
	}
}
