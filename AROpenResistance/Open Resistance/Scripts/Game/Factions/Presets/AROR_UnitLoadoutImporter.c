#ifdef WORKBENCH
//! One item as found, with everything inside it; identical siblings are merged into a count
class AROR_LoadoutImportItem
{
	ResourceName m_sPrefab;
	int m_iCount = 1;
	ref array<ref AROR_LoadoutImportItem> m_aContents = {};

	//------------------------------------------------------------------------------------------------
	//! Prefab and contents as one string, to tell identical items apart from merely same-prefab ones
	string GetSignature()
	{
		string signature = m_sPrefab;
		if (m_aContents.IsEmpty())
			return signature;

		signature += "[";
		foreach (AROR_LoadoutImportItem child : m_aContents)
		{
			signature += string.Format("%1x%2;", child.m_iCount, child.GetSignature());
		}

		return signature + "]";
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the item to the list, or counts it on an identical one already there
	static void AddTo(notnull array<ref AROR_LoadoutImportItem> items, AROR_LoadoutImportItem item)
	{
		if (!item || item.m_sPrefab.IsEmpty())
			return;

		string signature = item.GetSignature();
		foreach (AROR_LoadoutImportItem existing : items)
		{
			if (existing.GetSignature() == signature)
			{
				existing.m_iCount += item.m_iCount;
				return;
			}
		}

		items.Insert(item);
	}
}

//! One worn piece and what it holds
class AROR_LoadoutImportClothing
{
	ResourceName m_sPrefab;
	ref array<ref AROR_LoadoutImportItem> m_aContents = {};
}

//! One carried weapon as found on the character
class AROR_LoadoutImportWeapon
{
	ResourceName m_sPrefab;
	ref array<ResourceName> m_aAttachments = {};
	ResourceName m_sMagazine;
}

//! One preset unit being read
class AROR_LoadoutImportUnit
{
	string m_sPresetId;
	AROR_EUnitType m_eType;
	IEntity m_Entity;
	ref array<ref AROR_LoadoutImportClothing> m_aClothing = {};
	ref array<ref AROR_LoadoutImportWeapon> m_aWeapons = {};
	ref array<ref AROR_LoadoutImportItem> m_aItems = {};
}

//! What a weapon prefab carries on its own, so only differences are written
class AROR_LoadoutImportWeaponDefaults
{
	IEntity m_Entity;
	ref array<ResourceName> m_aAttachments = {};
	ResourceName m_sMagazine;
}

//! Workbench only: reads the kit of the base-game faction presets' infantry characters and writes it into the preset
//! files as each unit's readiness 1 loadout (see AROR_UnitLoadout). Runs on Play when "Import Base Unit Loadouts" is
//! ticked on the game mode's AROR_FactionPresetsComponent (Workbench plugins cannot reach the Play world). It spawns each unit's character out of the way, reads what it actually carries (clothing and
//! contents, weapons with attachments and magazine that differ from the weapon prefab's own, loose items), then
//! deletes everything. Units that already have loadouts are left untouched, so it never overwrites authored kits.
class AROR_UnitLoadoutImporter
{
	protected static const string OUTPUT_FOLDER = "$OpenResistance:Configs/FactionPresets/BaseGame/";
	protected static const string FALLBACK_FOLDER = "$profile:AROR_FactionPresets/";
	protected static const int SETTLE_DELAY_MS = 3000;

	protected static ref AROR_UnitLoadoutImporter s_Running;

	protected ref array<string> m_aPresetIds = {"FIA", "US_ARMY", "USSR_ARMY"};
	protected ref array<string> m_aPresetFiles = {"AROR_Preset_FIA.conf", "AROR_Preset_US_Army.conf", "AROR_Preset_USSR_Army.conf"};

	protected ref array<ref AROR_LoadoutImportUnit> m_aUnits = {};
	protected ref map<ResourceName, ref AROR_LoadoutImportWeaponDefaults> m_mWeaponDefaults = new map<ResourceName, ref AROR_LoadoutImportWeaponDefaults>();
	protected int m_iSpawned;

	//------------------------------------------------------------------------------------------------
	static void Start()
	{
		if (s_Running)
		{
			Print("[AROR] Import base unit loadouts: already running", LogLevel.WARNING);
			return;
		}

		if (!GetGame() || !GetGame().InPlayMode() || !Replication.IsServer())
		{
			Print("[AROR] Import base unit loadouts: only runs on the server in Play mode", LogLevel.ERROR);
			return;
		}

		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (!presets)
		{
			Print("[AROR] Import base unit loadouts: the world has no faction presets component", LogLevel.ERROR);
			return;
		}

		s_Running = new AROR_UnitLoadoutImporter();
		s_Running.SpawnUnits(presets);
	}

	//------------------------------------------------------------------------------------------------
	//! The unit types this importer handles: infantry
	protected static void GetInfantryTypes(notnull array<AROR_EUnitType> outTypes)
	{
		outTypes.Insert(AROR_EUnitType.RIFLEMAN);
		outTypes.Insert(AROR_EUnitType.AUTORIFLEMAN);
		outTypes.Insert(AROR_EUnitType.MACHINEGUNNER);
		outTypes.Insert(AROR_EUnitType.GRENADIER);
		outTypes.Insert(AROR_EUnitType.MEDIC);
		outTypes.Insert(AROR_EUnitType.MARKSMAN);
		outTypes.Insert(AROR_EUnitType.SNIPER);
		outTypes.Insert(AROR_EUnitType.AT);
		outTypes.Insert(AROR_EUnitType.AA);
		outTypes.Insert(AROR_EUnitType.ENGINEER);
		outTypes.Insert(AROR_EUnitType.RADIO_OPERATOR);
		outTypes.Insert(AROR_EUnitType.LEADER);
	}

	//------------------------------------------------------------------------------------------------
	// Step 1: spawn every preset's infantry characters
	//------------------------------------------------------------------------------------------------
	protected void SpawnUnits(AROR_FactionPresetsComponent presets)
	{
		array<AROR_EUnitType> types = {};
		GetInfantryTypes(types);

		foreach (string presetId : m_aPresetIds)
		{
			AROR_FactionPreset preset = presets.FindPreset(presetId);
			if (!preset)
			{
				Print(string.Format("[AROR] Import base unit loadouts: preset '%1' is not loaded; skipped", presetId), LogLevel.WARNING);
				continue;
			}

			foreach (AROR_EUnitType type : types)
			{
				AROR_FactionPresetUnit unit = preset.GetUnit(type);
				if (!unit || !unit.HasPrefab())
					continue;

				IEntity entity = SpawnOutOfTheWay(unit.GetListedPrefab());
				if (!entity)
				{
					Print(string.Format("[AROR] Import base unit loadouts: %1 %2 could not be spawned from %3", presetId, unit.GetTypeName(), unit.GetListedPrefab()), LogLevel.WARNING);
					continue;
				}

				AROR_LoadoutImportUnit importUnit = new AROR_LoadoutImportUnit();
				importUnit.m_sPresetId = presetId;
				importUnit.m_eType = type;
				importUnit.m_Entity = entity;
				m_aUnits.Insert(importUnit);
			}
		}

		Print(string.Format("[AROR] Import base unit loadouts: spawned %1 characters; reading their kit shortly", m_aUnits.Count()), LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(ReadUnits, SETTLE_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the prefab on the ground in a row at the map corner, away from players
	protected IEntity SpawnOutOfTheWay(ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		vector position = Vector(50 + 3 * m_iSpawned, 0, 50);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		m_iSpawned++;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	// Step 2: read each character, then spawn its weapons alone to learn their own defaults
	//------------------------------------------------------------------------------------------------
	protected void ReadUnits()
	{
		foreach (AROR_LoadoutImportUnit unit : m_aUnits)
		{
			if (unit.m_Entity)
				ReadCharacter(unit);
		}

		foreach (AROR_LoadoutImportUnit unit : m_aUnits)
		{
			foreach (AROR_LoadoutImportWeapon weapon : unit.m_aWeapons)
			{
				if (m_mWeaponDefaults.Contains(weapon.m_sPrefab))
					continue;

				AROR_LoadoutImportWeaponDefaults defaults = new AROR_LoadoutImportWeaponDefaults();
				defaults.m_Entity = SpawnOutOfTheWay(weapon.m_sPrefab);
				m_mWeaponDefaults.Insert(weapon.m_sPrefab, defaults);
			}
		}

		GetGame().GetCallqueue().CallLater(Finish, SETTLE_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadCharacter(AROR_LoadoutImportUnit unit)
	{
		set<IEntity> seen = new set<IEntity>();

		array<Managed> storages = {};
		unit.m_Entity.FindComponents(BaseInventoryStorageComponent, storages);
		foreach (Managed managed : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage)
				continue;

			bool isClothingStorage = storage.IsInherited(SCR_CharacterInventoryStorageComponent);

			array<IEntity> items = {};
			storage.GetAll(items, false);
			foreach (IEntity item : items)
			{
				if (!item || !seen.Insert(item))
					continue;

				ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
				if (prefab.IsEmpty())
					continue;

				BaseWeaponComponent weaponComponent = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
				if (weaponComponent)
				{
					AROR_LoadoutImportWeapon weapon = new AROR_LoadoutImportWeapon();
					weapon.m_sPrefab = prefab;
					weapon.m_sMagazine = ReadWeaponParts(item, weaponComponent, weapon.m_aAttachments);
					unit.m_aWeapons.Insert(weapon);
				}
				else if (isClothingStorage)
				{
					AROR_LoadoutImportClothing clothing = new AROR_LoadoutImportClothing();
					clothing.m_sPrefab = prefab;
					ReadContents(item, clothing.m_aContents);
					unit.m_aClothing.Insert(clothing);
				}
				else
				{
					AROR_LoadoutImportItem.AddTo(unit.m_aItems, ReadItem(item));
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The item and, recursively, everything inside it
	protected AROR_LoadoutImportItem ReadItem(IEntity entity)
	{
		AROR_LoadoutImportItem item = new AROR_LoadoutImportItem();
		item.m_sPrefab = SCR_ResourceNameUtils.GetPrefabName(entity);
		ReadContents(entity, item.m_aContents);
		return item;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every item inside the entity's storages, each with its own contents
	protected void ReadContents(IEntity owner, notnull array<ref AROR_LoadoutImportItem> outContents)
	{
		array<Managed> storages = {};
		owner.FindComponents(BaseInventoryStorageComponent, storages);
		foreach (Managed managed : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage)
				continue;

			array<IEntity> items = {};
			storage.GetAll(items, false);
			foreach (IEntity item : items)
			{
				if (item)
					AROR_LoadoutImportItem.AddTo(outContents, ReadItem(item));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the weapon's attachments (everything in its storages except the loaded magazine); returns the magazine prefab
	protected ResourceName ReadWeaponParts(IEntity weaponEntity, BaseWeaponComponent weapon, notnull array<ResourceName> outAttachments)
	{
		IEntity magazineEntity;
		BaseMagazineComponent magazine = weapon.GetCurrentMagazine();
		if (magazine)
			magazineEntity = magazine.GetOwner();

		array<Managed> storages = {};
		weaponEntity.FindComponents(BaseInventoryStorageComponent, storages);
		foreach (Managed managed : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage)
				continue;

			array<IEntity> items = {};
			storage.GetAll(items, false);
			foreach (IEntity item : items)
			{
				if (!item || item == magazineEntity)
					continue;

				ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(item);
				if (!prefab.IsEmpty())
					outAttachments.Insert(prefab);
			}
		}

		if (!magazineEntity)
			return string.Empty;

		return SCR_ResourceNameUtils.GetPrefabName(magazineEntity);
	}

	//------------------------------------------------------------------------------------------------
	// Step 3: subtract weapon defaults, write the files, clean up
	//------------------------------------------------------------------------------------------------
	protected void Finish()
	{
		foreach (ResourceName prefab, AROR_LoadoutImportWeaponDefaults defaults : m_mWeaponDefaults)
		{
			if (!defaults.m_Entity)
				continue;

			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(defaults.m_Entity.FindComponent(BaseWeaponComponent));
			if (weapon)
				defaults.m_sMagazine = ReadWeaponParts(defaults.m_Entity, weapon, defaults.m_aAttachments);
		}

		foreach (AROR_LoadoutImportUnit unit : m_aUnits)
		{
			foreach (AROR_LoadoutImportWeapon weapon : unit.m_aWeapons)
			{
				AROR_LoadoutImportWeaponDefaults defaults = m_mWeaponDefaults.Get(weapon.m_sPrefab);
				if (!defaults)
					continue;

				foreach (ResourceName attachment : defaults.m_aAttachments)
				{
					int index = weapon.m_aAttachments.Find(attachment);
					if (index >= 0)
						weapon.m_aAttachments.Remove(index);
				}

				if (weapon.m_sMagazine == defaults.m_sMagazine)
					weapon.m_sMagazine = string.Empty;
			}
		}

		foreach (int index, string presetId : m_aPresetIds)
		{
			WritePreset(presetId, m_aPresetFiles[index]);
		}

		foreach (AROR_LoadoutImportUnit unit : m_aUnits)
		{
			if (unit.m_Entity)
				SCR_EntityHelper.DeleteEntityAndChildren(unit.m_Entity);
		}

		foreach (ResourceName prefab, AROR_LoadoutImportWeaponDefaults defaults : m_mWeaponDefaults)
		{
			if (defaults.m_Entity)
				SCR_EntityHelper.DeleteEntityAndChildren(defaults.m_Entity);
		}

		s_Running = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Inserts a loadout into each read unit's block in the preset file, unless it already has loadouts
	protected void WritePreset(string presetId, string fileName)
	{
		string path = OUTPUT_FOLDER + fileName;
		array<string> lines = {};
		if (!ReadLines(path, lines))
		{
			Print(string.Format("[AROR] Import base unit loadouts: cannot read %1", path), LogLevel.ERROR);
			return;
		}

		array<string> output = {};
		int written, kept;
		bool inUnits, inUnit, hasLoadouts;
		AROR_EUnitType blockType;

		foreach (string line : lines)
		{
			if (!inUnits)
			{
				inUnits = line == " m_aUnits {";
				output.Insert(line);
				continue;
			}

			if (!inUnit)
			{
				if (line == " }")
					inUnits = false;

				// A unit block opens at two-space indent; Workbench omits m_eType when it is the default (RIFLEMAN)
				inUnit = line.IndexOf("  AROR_FactionPresetUnit") == 0;
				hasLoadouts = false;
				blockType = AROR_EUnitType.RIFLEMAN;
				output.Insert(line);
				continue;
			}

			if (line.IndexOf("   m_eType ") == 0)
			{
				string typeName = line.Substring(11, line.Length() - 11);
				typeName.TrimInPlace();
				blockType = typename.StringToEnum(AROR_EUnitType, typeName);
			}

			if (line.IndexOf("   m_aLoadouts") == 0)
				hasLoadouts = true;

			if (line == "  }")
			{
				inUnit = false;
				AROR_LoadoutImportUnit unit = FindUnit(presetId, blockType);
				if (unit && hasLoadouts)
				{
					kept++;
				}
				else if (unit)
				{
					AppendLoadout(unit, output);
					written++;
				}
			}

			output.Insert(line);
		}

		if (written == 0)
		{
			Print(string.Format("[AROR] Import base unit loadouts: %1: nothing to write (%2 units already had loadouts)", presetId, kept), LogLevel.NORMAL);
			return;
		}

		string writtenPath = WriteLines(path, fileName, output);
		if (writtenPath.IsEmpty())
			return;

		Print(string.Format("[AROR] Import base unit loadouts: %1: wrote %2 loadouts to %3 (%4 units already had loadouts and were kept)", presetId, written, writtenPath, kept), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected AROR_LoadoutImportUnit FindUnit(string presetId, AROR_EUnitType type)
	{
		foreach (AROR_LoadoutImportUnit unit : m_aUnits)
		{
			if (unit.m_sPresetId == presetId && unit.m_eType == type)
				return unit;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the unit's readiness 1 loadout as .conf text, indented to sit inside its unit block
	protected void AppendLoadout(AROR_LoadoutImportUnit unit, notnull array<string> output)
	{
		output.Insert("   m_aLoadouts {");
		output.Insert("    AROR_UnitLoadout {");

		if (!unit.m_aClothing.IsEmpty())
		{
			output.Insert("     m_aClothing {");
			foreach (AROR_LoadoutImportClothing clothing : unit.m_aClothing)
			{
				output.Insert("      AROR_LoadoutClothing {");
				output.Insert(string.Format("       m_sPrefab \"%1\"", clothing.m_sPrefab));
				AppendItems("       ", "m_aContents", clothing.m_aContents, output);
				output.Insert("      }");
			}
			output.Insert("     }");
		}

		if (!unit.m_aWeapons.IsEmpty())
		{
			output.Insert("     m_aWeapons {");
			foreach (AROR_LoadoutImportWeapon weapon : unit.m_aWeapons)
			{
				output.Insert("      AROR_LoadoutWeapon {");
				output.Insert(string.Format("       m_sPrefab \"%1\"", weapon.m_sPrefab));
				if (!weapon.m_aAttachments.IsEmpty())
				{
					output.Insert("       m_aAttachments {");
					foreach (ResourceName attachment : weapon.m_aAttachments)
					{
						output.Insert(string.Format("        \"%1\"", attachment));
					}
					output.Insert("       }");
				}

				if (!weapon.m_sMagazine.IsEmpty())
					output.Insert(string.Format("       m_sMagazine \"%1\"", weapon.m_sMagazine));

				output.Insert("      }");
			}
			output.Insert("     }");
		}

		AppendItems("     ", "m_aItems", unit.m_aItems, output);

		output.Insert("    }");
		output.Insert("   }");
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a list of AROR_LoadoutItem entries, recursing into contents (nothing when empty). indent is the list's own
	//! indent; count 1 is the default and left out.
	protected void AppendItems(string indent, string listName, array<ref AROR_LoadoutImportItem> items, notnull array<string> output)
	{
		if (items.IsEmpty())
			return;

		string itemIndent = indent + " ";
		output.Insert(string.Format("%1%2 {", indent, listName));
		foreach (AROR_LoadoutImportItem item : items)
		{
			output.Insert(itemIndent + "AROR_LoadoutItem {");
			output.Insert(string.Format("%1 m_sPrefab \"%2\"", itemIndent, item.m_sPrefab));
			if (item.m_iCount != 1)
				output.Insert(string.Format("%1 m_iCount %2", itemIndent, item.m_iCount));

			AppendItems(itemIndent + " ", "m_aContents", item.m_aContents, output);
			output.Insert(itemIndent + "}");
		}
		output.Insert(indent + "}");
	}

	//------------------------------------------------------------------------------------------------
	protected bool ReadLines(string path, notnull array<string> outLines)
	{
		if (!FileIO.FileExists(path))
			return false;

		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return false;

		string line;
		while (file.ReadLine(line) >= 0)
		{
			line.Replace("\r", string.Empty);
			outLines.Insert(line);
		}

		file.Close();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the lines to the addon file, or to the profile folder when the addon is read-only; returns the path used
	protected string WriteLines(string path, string fileName, notnull array<string> lines)
	{
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			FileIO.MakeDirectory(FALLBACK_FOLDER);
			path = FALLBACK_FOLDER + fileName;
			file = FileIO.OpenFile(path, FileMode.WRITE);
			if (!file)
			{
				Print(string.Format("[AROR] Import base unit loadouts: cannot write %1", fileName), LogLevel.ERROR);
				return string.Empty;
			}

			Print(string.Format("[AROR] Import base unit loadouts: the addon folder is read-only here, so %1 was written to your profile folder; copy it into Configs/FactionPresets/BaseGame/", fileName), LogLevel.WARNING);
		}

		foreach (string line : lines)
		{
			file.WriteLine(line);
		}

		file.Close();
		return path;
	}
}
#endif
