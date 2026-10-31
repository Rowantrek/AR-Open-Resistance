[ComponentEditorProps(category: "OpenResistance/Garage", description: "Resistance shared vehicle garage (unique snapshots). Attach to the game mode entity.")]
class AROR_ResistanceGarageComponentClass : SCR_BaseGameModeComponentClass
{
}

void AROR_OnResistanceGarageChanged();
typedef func AROR_OnResistanceGarageChanged;

//! Resistance Garage (see ARCHITECTURE.md -> Resistance Garage): a campaign ledger of unique vehicle
//! snapshots for the Resistance. Capacity comes from the HQ plus Resistance-controlled military
//! objectives. Server owns full entry data; compact summaries replicate. Retrieve restores condition
//! within a placement radius of a garage anchor. Saved by AROR_ResistanceGarageComponentSerializer.
class AROR_ResistanceGarageComponent : SCR_BaseGameModeComponent
{
	[Attribute("10", desc: "Garage slots granted by the Resistance HQ", params: "0 100", category: "Capacity")]
	protected int m_iHqSlots;

	[Attribute("2", desc: "Garage slots per Resistance-controlled military objective (excl. towns/cities)", params: "0 20", category: "Capacity")]
	protected int m_iObjectiveSlots;

	[Attribute("50", desc: "Max distance (m) from the garage anchor to place a retrieved vehicle", params: "5 200", category: "Retrieve")]
	protected float m_fPlaceRadius;

	[Attribute("2.5", desc: "Half-extent (m) of the clearance TraceBox when placing a retrieved vehicle", params: "0.5 10", category: "Retrieve")]
	protected float m_fPlaceClearanceHalfExtent;

	[Attribute("3", desc: "Height (m) of the clearance TraceBox when placing a retrieved vehicle", params: "1 10", category: "Retrieve")]
	protected float m_fPlaceClearanceHeight;

	[Attribute("5", desc: "Max aim distance (m) when storing the vehicle under the player's view", params: "1 25", category: "Store")]
	protected float m_fStoreLookDistance;

	[Attribute("50", desc: "Player must be within this distance (m) of a Resistance HQ or capturable site to store", params: "5 200", category: "Store")]
	protected float m_fStoreSiteRadius;

	protected static AROR_ResistanceGarageComponent s_Instance;

	//! Server: full unique entries (cargo trees included)
	protected ref array<ref AROR_GarageVehicleEntry> m_aEntries = {};

	protected int m_iNextId = 1;

	//! Server: playerId -> reserved entry id (session only)
	protected ref map<int, int> m_mReservationsByPlayer = new map<int, int>();

	//! Replicated summaries for clients / future UI
	[RplProp(onRplName: "OnGarageReplicated")]
	protected ref array<int> m_aEntryIds = {};

	[RplProp(onRplName: "OnGarageReplicated")]
	protected ref array<ResourceName> m_aPrefabs = {};

	[RplProp(onRplName: "OnGarageReplicated")]
	protected ref array<float> m_aHealths = {};

	[RplProp(onRplName: "OnGarageReplicated")]
	protected ref array<float> m_aFuelFractions = {};

	protected ref ScriptInvokerBase<AROR_OnResistanceGarageChanged> m_OnGarageChanged = new ScriptInvokerBase<AROR_OnResistanceGarageChanged>();

	protected bool m_bListeningToLocations;

	//------------------------------------------------------------------------------------------------
	static AROR_ResistanceGarageComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine; fires after a mutate, capacity-affecting location change, or client replicate
	ScriptInvokerBase<AROR_OnResistanceGarageChanged> GetOnGarageChanged()
	{
		return m_OnGarageChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: HQ slots + objective slots for Resistance-held capturable locations
	int GetCapacity()
	{
		int capacity;
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return capacity;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		foreach (AROR_LocationComponent location : all)
		{
			if (!location || location.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
				continue;

			AROR_ELocationType type = location.GetLocationType();
			if (type == AROR_ELocationType.HQ)
				capacity += m_iHqSlots;
			else if (AROR_CaptureLocationAction.IsCapturableType(type))
				capacity += m_iObjectiveSlots;
		}

		return capacity;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine
	int GetCount()
	{
		if (Replication.IsServer())
			return m_aEntries.Count();

		return m_aEntryIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine
	bool IsFull()
	{
		return GetCount() >= GetCapacity();
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: retrieve placement radius around the garage anchor
	float GetPlaceRadius()
	{
		return m_fPlaceRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: max view distance for store-looked-at
	float GetStoreLookDistance()
	{
		return m_fStoreLookDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: max distance from Resistance HQ / capturable site to store
	float GetStoreSiteRadius()
	{
		return m_fStoreSiteRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: true when position is within store-site radius of a Resistance HQ or capturable location
	bool IsNearFriendlyGarageSite(vector position)
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return false;

		array<AROR_LocationComponent> all = {};
		locations.GetLocations(all);
		foreach (AROR_LocationComponent location : all)
		{
			if (!location || location.GetControllingRole() != AROR_EFactionRole.RESISTANCE)
				continue;

			AROR_ELocationType type = location.GetLocationType();
			if (type != AROR_ELocationType.HQ && !AROR_CaptureLocationAction.IsCapturableType(type))
				continue;

			if (vector.DistanceXZ(position, location.GetCenter()) <= m_fStoreSiteRadius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True while nothing is garaged (nothing to save)
	bool IsEmpty()
	{
		return GetCount() == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: compact summaries (id, prefab, health, fuel fraction)
	void GetEntries(notnull array<int> ids, notnull array<ResourceName> prefabs, notnull array<float> healths, notnull array<float> fuelFractions)
	{
		ids.Clear();
		prefabs.Clear();
		healths.Clear();
		fuelFractions.Clear();

		if (Replication.IsServer())
		{
			foreach (AROR_GarageVehicleEntry entry : m_aEntries)
			{
				if (!entry)
					continue;

				ids.Insert(entry.m_iId);
				prefabs.Insert(entry.m_sPrefab);
				healths.Insert(entry.m_fHealthScaled);
				fuelFractions.Insert(entry.m_fFuelNormalized);
			}

			return;
		}

		int count = Math.Min(Math.Min(m_aEntryIds.Count(), m_aPrefabs.Count()), Math.Min(m_aHealths.Count(), m_aFuelFractions.Count()));
		for (int i = 0; i < count; i++)
		{
			ids.Insert(m_aEntryIds[i]);
			prefabs.Insert(m_aPrefabs[i]);
			healths.Insert(m_aHealths[i]);
			fuelFractions.Insert(m_aFuelFractions[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: full entry by id, or null
	AROR_GarageVehicleEntry GetEntry_S(int id)
	{
		if (!Replication.IsServer())
			return null;

		foreach (AROR_GarageVehicleEntry entry : m_aEntries)
		{
			if (entry && entry.m_iId == id)
				return entry;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Snapshots the vehicle, appends a unique entry, deletes the world entity.
	//! Rejects non-vehicles, destroyed hulls, occupied vehicles, and stores while at/over capacity.
	//! Any faction/prefab is allowed. Returns the new entry id, or -1 on failure.
	int StoreVehicle_S(IEntity vehicle)
	{
		if (!Replication.IsServer() || !vehicle)
			return -1;

		if (!AROR_GarageSnapshotHelper.IsVehicle(vehicle))
			return -1;

		if (AROR_GarageSnapshotHelper.HasLivingOccupant(vehicle))
			return -1;

		if (GetCount() >= GetCapacity())
			return -1;

		AROR_GarageVehicleEntry entry = AROR_GarageSnapshotHelper.Capture(vehicle);
		if (!entry)
			return -1;

		entry.m_iId = m_iNextId;
		m_iNextId++;
		m_aEntries.Insert(entry);

		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		Publish_S();
		return entry.m_iId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Adds a brand-new vehicle (bought through Imports) without a world entity. It is retrieved in
	//! factory condition. Rejects an empty prefab or a full garage. Returns the new entry id, or -1 on failure.
	int AddVehicleByPrefab_S(ResourceName prefab)
	{
		if (!Replication.IsServer() || prefab.IsEmpty())
			return -1;

		if (GetCount() >= GetCapacity())
			return -1;

		AROR_GarageVehicleEntry entry = new AROR_GarageVehicleEntry();
		entry.m_iId = m_iNextId;
		entry.m_sPrefab = prefab;
		entry.m_bFactoryNew = true;
		entry.m_fHealthScaled = 1;
		entry.m_fFuelNormalized = 1;
		m_iNextId++;
		m_aEntries.Insert(entry);

		Publish_S();
		return entry.m_iId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Player-initiated store: near friendly garage site, within look distance of the vehicle.
	//! outReason is a short hint for the client. Returns entry id or -1.
	int RequestStoreVehicle_S(int playerId, IEntity vehicle, out string outReason)
	{
		outReason = string.Empty;
		if (!Replication.IsServer() || playerId <= 0 || !vehicle)
		{
			outReason = "Store failed.";
			return -1;
		}

		IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!controlled)
		{
			outReason = "No controlled character.";
			return -1;
		}

		if (!IsNearFriendlyGarageSite(controlled.GetOrigin()))
		{
			outReason = "Must be near a Resistance HQ or military site.";
			return -1;
		}

		float maxDist = m_fStoreLookDistance + 1.5;
		if (vector.Distance(controlled.GetOrigin(), vehicle.GetOrigin()) > maxDist)
		{
			outReason = "Vehicle is too far away.";
			return -1;
		}

		if (!AROR_GarageSnapshotHelper.IsVehicle(vehicle))
		{
			outReason = "Not a vehicle.";
			return -1;
		}

		if (AROR_GarageSnapshotHelper.HasLivingOccupant(vehicle))
		{
			outReason = "Vehicle is occupied.";
			return -1;
		}

		if (GetCount() >= GetCapacity())
		{
			outReason = "Garage is full.";
			return -1;
		}

		int id = StoreVehicle_S(vehicle);
		if (id < 0)
		{
			outReason = "Could not store vehicle.";
			return -1;
		}

		outReason = "Vehicle stored.";
		return id;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Removes an entry without spawning. Returns false if the id is unknown.
	bool DiscardVehicle_S(int id)
	{
		if (!Replication.IsServer() || id <= 0)
			return false;

		for (int i = m_aEntries.Count() - 1; i >= 0; i--)
		{
			AROR_GarageVehicleEntry entry = m_aEntries[i];
			if (!entry || entry.m_iId != id)
				continue;

			ClearReservationsForEntry_S(id);
			m_aEntries.Remove(i);
			Publish_S();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Soft-locks an entry for one player while they place. Fails if missing or held by another.
	bool ReserveEntry_S(int playerId, int entryId)
	{
		if (!Replication.IsServer() || playerId <= 0 || entryId <= 0)
			return false;

		if (!GetEntry_S(entryId))
			return false;

		int holder;
		if (FindReservationHolder_S(entryId, holder) && holder != playerId)
			return false;

		ReleaseReservation_S(playerId);
		m_mReservationsByPlayer.Set(playerId, entryId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Clears this player's reservation (cancel place / disconnect).
	void ReleaseReservation_S(int playerId)
	{
		if (!Replication.IsServer() || playerId <= 0)
			return;

		m_mReservationsByPlayer.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Flat cargo labels for the inventory dialog.
	bool GetEntryCargoLabels_S(int entryId, notnull array<string> outLabels)
	{
		outLabels.Clear();
		if (!Replication.IsServer())
			return false;

		AROR_GarageVehicleEntry entry = GetEntry_S(entryId);
		if (!entry)
			return false;

		AROR_GarageSnapshotHelper.FlattenCargoLabels(entry, outLabels);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Spawns the entry at position (yaw degrees) within maxRadius of the anchor, restores
	//! snapshot, affiliates Resistance, removes the entry. Returns the vehicle or null (entry kept on fail).
	IEntity RetrieveVehicle_S(int playerId, int entryId, vector position, float yaw, vector anchorOrigin, float maxRadius = -1)
	{
		if (!Replication.IsServer() || entryId <= 0)
			return null;

		AROR_GarageVehicleEntry entry = GetEntry_S(entryId);
		if (!entry || entry.m_sPrefab.IsEmpty())
			return null;

		int holder;
		if (FindReservationHolder_S(entryId, holder) && holder != playerId)
			return null;

		float radius = maxRadius;
		if (radius < 0)
			radius = m_fPlaceRadius;

		if (vector.DistanceXZ(position, anchorOrigin) > radius)
			return null;

		string reason;
		if (!IsPlacementClear(position, reason))
			return null;

		IEntity vehicle = SpawnVehiclePrefab_S(entry.m_sPrefab, position, yaw);
		if (!vehicle)
			return null;

		// A factory-new import has no snapshot to restore: it keeps the prefab's own fuel, cargo and condition
		if (!entry.m_bFactoryNew)
			AROR_GarageSnapshotHelper.Apply(vehicle, entry);

		RemoveEntryById_S(entryId);
		ReleaseReservation_S(playerId);
		return vehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! Client + server: TraceBox clearance at surface-grounded position.
	//! exclude: optional entity (and its hierarchy) ignored — needed so the client ghost does not block itself.
	bool IsPlacementClear(vector position, out string reason, IEntity exclude = null)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			reason = "World missing";
			return false;
		}

		float surfaceY = world.GetSurfaceY(position[0], position[2]);
		if (surfaceY <= world.GetOceanHeight(position[0], position[2]) + 0.25)
		{
			reason = "Cannot place in water";
			return false;
		}

		vector grounded = Vector(position[0], surfaceY, position[2]);
		TraceBox trace = new TraceBox();
		trace.Start = grounded + Vector(0, 0.25, 0);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-m_fPlaceClearanceHalfExtent, 0, -m_fPlaceClearanceHalfExtent);
		trace.Maxs = Vector(m_fPlaceClearanceHalfExtent, m_fPlaceClearanceHeight, m_fPlaceClearanceHalfExtent);
		trace.Flags = TraceFlags.ENTS;
		if (exclude)
		{
			array<IEntity> excludes = {};
			CollectEntityHierarchy(exclude, excludes);
			trace.Exclude = exclude;
			trace.ExcludeArray = excludes;
		}

		if (world.TraceMove(trace, null) != 1)
		{
			reason = "Placement blocked";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectEntityHierarchy(IEntity root, notnull array<IEntity> outEntities)
	{
		if (!root)
			return;

		outEntities.Insert(root);
		IEntity child = root.GetChildren();
		while (child)
		{
			CollectEntityHierarchy(child, outEntities);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnVehiclePrefab_S(ResourceName prefab, vector position, float yaw)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		BaseWorld world = GetGame().GetWorld();
		position[1] = world.GetSurfaceY(position[0], position[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector rotation[3];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), rotation);
		params.Transform[0] = rotation[0];
		params.Transform[1] = rotation[1];
		params.Transform[2] = rotation[2];
		params.Transform[3] = position;

		IEntity vehicle = GetGame().SpawnEntityPrefab(resource, world, params);
		if (!vehicle)
			return null;

		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (affiliation && roles)
		{
			string factionKey = roles.GetFactionKey(AROR_EFactionRole.RESISTANCE);
			if (!factionKey.IsEmpty())
				affiliation.SetAffiliatedFactionByKey(factionKey);
		}

		return vehicle;
	}

	//------------------------------------------------------------------------------------------------
	protected bool RemoveEntryById_S(int id)
	{
		for (int i = m_aEntries.Count() - 1; i >= 0; i--)
		{
			AROR_GarageVehicleEntry entry = m_aEntries[i];
			if (!entry || entry.m_iId != id)
				continue;

			ClearReservationsForEntry_S(id);
			m_aEntries.Remove(i);
			Publish_S();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FindReservationHolder_S(int entryId, out int playerId)
	{
		playerId = 0;
		foreach (int pid, int reservedId : m_mReservationsByPlayer)
		{
			if (reservedId == entryId)
			{
				playerId = pid;
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearReservationsForEntry_S(int entryId)
	{
		array<int> toClear = {};
		foreach (int pid, int reservedId : m_mReservationsByPlayer)
		{
			if (reservedId == entryId)
				toClear.Insert(pid);
		}

		foreach (int pid : toClear)
		{
			m_mReservationsByPlayer.Remove(pid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: replace garage contents from a save (used by the serializer)
	void RestoreEntries_S(notnull array<ref AROR_GarageVehicleEntry> entries, int nextId)
	{
		if (!Replication.IsServer())
			return;

		m_aEntries.Clear();
		int maxId;
		foreach (AROR_GarageVehicleEntry entry : entries)
		{
			if (!entry || entry.m_sPrefab.IsEmpty() || entry.m_iId <= 0)
				continue;

			m_aEntries.Insert(entry);
			if (entry.m_iId > maxId)
				maxId = entry.m_iId;
		}

		if (nextId > maxId)
			m_iNextId = nextId;
		else
			m_iNextId = maxId + 1;

		Publish_S();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: copy of full entries for the serializer
	void GetFullEntries_S(notnull array<ref AROR_GarageVehicleEntry> outEntries)
	{
		outEntries.Clear();
		if (!Replication.IsServer())
			return;

		foreach (AROR_GarageVehicleEntry entry : m_aEntries)
		{
			if (entry)
				outEntries.Insert(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server only
	int GetNextId_S()
	{
		return m_iNextId;
	}

	//------------------------------------------------------------------------------------------------
	protected void Publish_S()
	{
		if (!Replication.IsServer())
			return;

		m_aEntryIds.Clear();
		m_aPrefabs.Clear();
		m_aHealths.Clear();
		m_aFuelFractions.Clear();

		foreach (AROR_GarageVehicleEntry entry : m_aEntries)
		{
			if (!entry)
				continue;

			m_aEntryIds.Insert(entry.m_iId);
			m_aPrefabs.Insert(entry.m_sPrefab);
			m_aHealths.Insert(entry.m_fHealthScaled);
			m_aFuelFractions.Insert(entry.m_fFuelNormalized);
		}

		Replication.BumpMe();
		m_OnGarageChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnGarageReplicated()
	{
		m_OnGarageChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnLocationChanged(AROR_LocationComponent location)
	{
		m_OnGarageChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void BindLocationListeners()
	{
		if (m_bListeningToLocations)
			return;

		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
			return;

		locations.GetOnLocationRegistered().Insert(OnLocationChanged);
		locations.GetOnLocationUnregistered().Insert(OnLocationChanged);
		locations.GetOnLocationControlChanged().Insert(OnLocationChanged);
		m_bListeningToLocations = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UnbindLocationListeners()
	{
		if (!m_bListeningToLocations)
			return;

		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (locations)
		{
			locations.GetOnLocationRegistered().Remove(OnLocationChanged);
			locations.GetOnLocationUnregistered().Remove(OnLocationChanged);
			locations.GetOnLocationControlChanged().Remove(OnLocationChanged);
		}

		m_bListeningToLocations = false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);
		ReleaseReservation_S(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;
		// Location manager may init in the same frame; defer the bind slightly
		GetGame().GetCallqueue().CallLater(BindLocationListeners, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(BindLocationListeners);
		UnbindLocationListeners();

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
