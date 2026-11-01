//! Resistance vehicles out in the world (see ARCHITECTURE.md -> Resistance Garage). Server only.
//! Any vehicle a living Resistance player sits in is claimed for the Resistance: it is affiliated Resistance, no other
//! system (Operations, Missions, Garrisons) deletes it or reports it as an enemy asset any more,
//! and it is saved with its condition and position and put back on load. Owned by AROR_ResistanceGarageComponent and
//! saved through its serializer. A claimed vehicle leaves the fleet when it is destroyed, deleted or stored.
class AROR_ResistanceFleet
{
	//! Saved vehicles are put back this long after loading, once the world and the save have settled
	protected static const int RESTORE_DELAY_MS = 10000;
	//! A vehicle of the same prefab already this close to a saved one is taken as that vehicle (no duplicate)
	protected static const float RESTORE_MATCH_RADIUS = 3;

	protected ref array<IEntity> m_aVehicles = {};

	// Loaded from a save, waiting to be spawned
	protected ref array<ref AROR_GarageVehicleEntry> m_aPending = {};
	protected ref array<vector> m_aPendingPositions = {};
	protected ref array<vector> m_aPendingAngles = {};

	// Restore duplicate check (QueryEntitiesBySphere callback state)
	protected ResourceName m_sQueryPrefab;
	protected IEntity m_QueryFound;

	//------------------------------------------------------------------------------------------------
	//! True when the vehicle (or the hull it is part of) belongs to the Resistance fleet. Server only.
	static bool IsClaimed(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		AROR_ResistanceGarageComponent garage = AROR_ResistanceGarageComponent.GetInstance();
		if (!garage || !garage.GetFleet())
			return false;

		IEntity root = vehicle.GetRootParent();
		if (root)
			vehicle = root;

		return garage.GetFleet().Contains(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	bool Contains(IEntity vehicle)
	{
		return vehicle && m_aVehicles.Contains(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! Periodic: drop lost vehicles, then claim every vehicle a living Resistance player is seated in
	void Scan_S()
	{
		if (!Replication.IsServer())
			return;

		for (int i = m_aVehicles.Count() - 1; i >= 0; i--)
		{
			if (!m_aVehicles[i] || IsDestroyed(m_aVehicles[i]))
				m_aVehicles.Remove(i);
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> players = {};
		playerManager.GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity vehicle = SeatedVehicle(playerManager.GetPlayerControlledEntity(playerId));
			if (vehicle)
				Claim_S(vehicle, playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: add a vehicle to the fleet and affiliate it with the Resistance
	void Claim_S(IEntity vehicle, int playerId = 0)
	{
		if (!Replication.IsServer() || !vehicle || m_aVehicles.Contains(vehicle) || IsDestroyed(vehicle))
			return;

		m_aVehicles.Insert(vehicle);
		AffiliateResistance(vehicle);
		Print(string.Format("[AROR] %1 claimed for the Resistance (player %2)", AROR_GarageSnapshotHelper.PrefabDisplayName(SCR_ResourceNameUtils.GetPrefabName(vehicle)), playerId), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, on save: a snapshot, position and yaw/pitch/roll per intact fleet vehicle (ids are left to the caller)
	void CaptureForSave_S(notnull array<ref AROR_GarageVehicleEntry> outEntries, notnull array<vector> outPositions, notnull array<vector> outAngles)
	{
		foreach (IEntity vehicle : m_aVehicles)
		{
			if (!vehicle)
				continue;

			AROR_GarageVehicleEntry entry = AROR_GarageSnapshotHelper.Capture(vehicle);
			if (!entry)
				continue;

			outEntries.Insert(entry);
			outPositions.Insert(vehicle.GetOrigin());
			outAngles.Insert(vehicle.GetYawPitchRoll());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server, on load: remember the saved vehicles and put them back shortly
	void RestoreFromSave_S(notnull array<ref AROR_GarageVehicleEntry> entries, notnull array<vector> positions, notnull array<vector> angles)
	{
		int count = Math.Min(entries.Count(), Math.Min(positions.Count(), angles.Count()));
		for (int i = 0; i < count; i++)
		{
			if (!entries[i] || entries[i].m_sPrefab.IsEmpty())
				continue;

			m_aPending.Insert(entries[i]);
			m_aPendingPositions.Insert(positions[i]);
			m_aPendingAngles.Insert(angles[i]);
		}

		if (m_aPending.IsEmpty())
			return;

		GetGame().GetCallqueue().Remove(SpawnPending_S);
		GetGame().GetCallqueue().CallLater(SpawnPending_S, RESTORE_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		GetGame().GetCallqueue().Remove(SpawnPending_S);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnPending_S()
	{
		if (!Replication.IsServer())
			return;

		int restored;
		foreach (int i, AROR_GarageVehicleEntry entry : m_aPending)
		{
			vector position = m_aPendingPositions[i];

			// The base game may have brought the vehicle back with the save already: adopt it instead
			IEntity existing = FindNear(entry.m_sPrefab, position, RESTORE_MATCH_RADIUS);
			if (existing)
			{
				Claim_S(existing);
				restored++;
				continue;
			}

			IEntity vehicle = Spawn(entry.m_sPrefab, position, m_aPendingAngles[i]);
			if (!vehicle)
			{
				Print(string.Format("[AROR] Resistance vehicle %1 could not be restored at %2", entry.m_sPrefab, position), LogLevel.WARNING);
				continue;
			}

			AROR_GarageSnapshotHelper.Apply(vehicle, entry);
			Claim_S(vehicle);
			restored++;
		}

		Print(string.Format("[AROR] Restored %1 of %2 Resistance vehicles", restored, m_aPending.Count()), LogLevel.NORMAL);
		m_aPending.Clear();
		m_aPendingPositions.Clear();
		m_aPendingAngles.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Saved position, kept above the terrain so a vehicle never comes back sunk into the ground
	protected static IEntity Spawn(ResourceName prefab, vector position, vector angles)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		BaseWorld world = GetGame().GetWorld();
		position[1] = Math.Max(position[1], world.GetSurfaceY(position[0], position[2])) + 0.2;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector rotation[3];
		Math3D.AnglesToMatrix(angles, rotation);
		for (int axis = 0; axis < 3; axis++)
			params.Transform[axis] = rotation[axis];
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(resource, world, params);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FindNear(ResourceName prefab, vector position, float radius)
	{
		m_sQueryPrefab = prefab;
		m_QueryFound = null;
		GetGame().GetWorld().QueryEntitiesBySphere(position, radius, OnQueryEntity, null, EQueryEntitiesFlags.DYNAMIC);
		return m_QueryFound;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnQueryEntity(IEntity entity)
	{
		if (!Vehicle.Cast(entity) || IsDestroyed(entity) || SCR_ResourceNameUtils.GetPrefabName(entity) != m_sQueryPrefab)
			return true;

		m_QueryFound = entity;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle hull the character is seated in, if the character is a living Resistance member
	protected static IEntity SeatedVehicle(IEntity character)
	{
		if (!AROR_QRFSpotting.IsResistance(character))
			return null;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access || !access.IsInCompartment())
			return null;

		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return null;

		IEntity vehicle = slot.GetVehicle();
		if (!vehicle)
			return null;

		// Turret seats can live on child entities of the hull
		if (vehicle.GetRootParent())
			vehicle = vehicle.GetRootParent();

		// Static weapons are not vehicles and stay with their site
		if (!Vehicle.Cast(vehicle))
			return null;

		return vehicle;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDestroyed(IEntity vehicle)
	{
		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		return damage && damage.IsDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	protected static void AffiliateResistance(IEntity vehicle)
	{
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!affiliation || !roles)
			return;

		string factionKey = roles.GetFactionKey(AROR_EFactionRole.RESISTANCE);
		if (!factionKey.IsEmpty())
			affiliation.SetAffiliatedFactionByKey(factionKey);
	}
}
