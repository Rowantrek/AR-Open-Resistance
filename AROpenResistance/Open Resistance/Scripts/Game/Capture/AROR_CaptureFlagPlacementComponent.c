[ComponentEditorProps(category: "OpenResistance/Locations", description: "Server places the capture pole on clear nearby walkable terrain, then replicates its world position. At a town or city with a garrison post it first finds room for the post's building, raises the holding army's building there and stands the flag out front. The flag cloth shows the holding faction on every machine.")]
class AROR_CaptureFlagPlacementComponentClass : ScriptComponentClass {}

class AROR_CaptureFlagPlacementComponent : ScriptComponent
{
	[Attribute("1", desc: "Auto Place: during Play the server moves this capture pole to a clear, flat, walkable spot near the location centre (within Search Radius). Turn off for hand-placed flags to keep their editor position.")]
	protected bool m_bAutoPlace;
	[Attribute("60", desc: "Maximum search distance from the location centre; also limited to half its radius", params: "5 100 5")]
	protected float m_fSearchRadius;

	[Attribute("1", desc: "At a town or city with a garrison post: raise the holding army's post building and stand the flag in front of it", category: "Garrison post")]
	protected bool m_bGarrisonBuilding;
	[Attribute("{D8B2D92435256291}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_US_01.et", UIWidgets.ResourceNamePicker, desc: "Post building while the Occupiers hold the town", params: "et", category: "Garrison post")]
	protected ResourceName m_sOccupierBuilding;
	[Attribute("{607E00F1C367D129}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_USSR_01.et", UIWidgets.ResourceNamePicker, desc: "Post building while the Invaders hold the town", params: "et", category: "Garrison post")]
	protected ResourceName m_sInvaderBuilding;
	[Attribute("{795D20A26B62F82F}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_FIA_01.et", UIWidgets.ResourceNamePicker, desc: "Post building while the Resistance holds the town", params: "et", category: "Garrison post")]
	protected ResourceName m_sResistanceBuilding;
	[Attribute("6.5", desc: "Clear radius the building needs around its centre, metres", params: "3 15 0.5", category: "Garrison post")]
	protected float m_fBuildingRadius;
	[Attribute("1.5", desc: "Largest height difference allowed across the building's footprint, metres", params: "0.3 4 0.1", category: "Garrison post")]
	protected float m_fBuildingMaxRelief;
	[Attribute("10", desc: "Flag distance in front of the building's centre, metres", params: "4 25 0.5", category: "Garrison post")]
	protected float m_fFlagFront;
	[Attribute("0", desc: "Turns the building relative to the flag direction, degrees. Use 180 if its door ends up facing away from the flag.", params: "-180 180 90", category: "Garrison post")]
	protected float m_fBuildingYawOffset;

	[RplProp()]
	protected vector m_vPlacedPosition;
	[RplProp()]
	protected float m_fPlacedYaw;
	[RplProp(onRplName: "ApplyPlacement")]
	protected bool m_bPlaced;
	protected int m_iSiteAttempt;
	protected int m_iTileWaits;
	protected int m_iAttempt;
	protected bool m_bHasClearCandidate;
	protected vector m_vClearCandidate;
	protected bool m_bHasSite;
	protected vector m_vSite;
	protected IEntity m_Building;
	protected AROR_EFactionRole m_eBuildingRole;
	protected bool m_bHooked;

	protected static const int SITE_ATTEMPTS = 33;	//!< Location centre, then four rings of eight

	static vector Candidate(vector center, float radius, int ordinal)
	{
		if (ordinal <= 0)
			return center;
		int index = ordinal - 1;
		int ring = index / 8 + 1;
		int sector = index - (ring - 1) * 8;
		float angle = sector * 0.78539816;
		return center + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * (radius * ring / 4);
	}

	protected AROR_LocationComponent FindLocation()
	{
		IEntity parent = GetOwner().GetParent();
		while (parent)
		{
			AROR_LocationComponent location = AROR_LocationComponent.Cast(parent.FindComponent(AROR_LocationComponent));
			if (location)
				return location;
			parent = parent.GetParent();
		}
		return null;
	}

	//! A garrison post building belongs to towns and cities whose garrison is a settlement post
	protected bool WantsBuilding(AROR_LocationComponent location)
	{
		if (!m_bGarrisonBuilding || !location)
			return false;
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(location.GetOwner().FindComponent(AROR_GarrisonComponent));
		return garrison && garrison.IsSettlementPost();
	}

	protected bool IsClear(vector point)
	{
		BaseWorld world = GetGame().GetWorld();
		float height = world.GetSurfaceY(point[0], point[2]);
		if (height <= world.GetOceanHeight(point[0], point[2]) + 0.25)
			return false;
		for (int side = 0; side < 4; side++)
		{
			float angle = side * 1.57079633;
			float sample = world.GetSurfaceY(point[0] + Math.Sin(angle) * 1.5, point[2] + Math.Cos(angle) * 1.5);
			if (Math.AbsFloat(sample - height) > 0.6)
				return false;
		}
		TraceBox trace = new TraceBox();
		trace.Start = Vector(point[0], height + 0.15, point[2]);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-1.5, 0, -1.5);
		trace.Maxs = Vector(1.5, 7, 1.5);
		trace.Exclude = GetOwner();
		trace.Flags = TraceFlags.ENTS;
		return world.TraceMove(trace, null) >= 1;
	}

	//! Room for the building: dry, nearly flat across its footprint and nothing standing in it
	protected bool IsSiteClear(vector site)
	{
		BaseWorld world = GetGame().GetWorld();
		float height = world.GetSurfaceY(site[0], site[2]);
		if (height <= world.GetOceanHeight(site[0], site[2]) + 0.5)
			return false;
		float lowest = height;
		float highest = height;
		for (int sample = 0; sample < 8; sample++)
		{
			float angle = sample * 0.78539816;
			float x = site[0] + Math.Sin(angle) * m_fBuildingRadius;
			float z = site[2] + Math.Cos(angle) * m_fBuildingRadius;
			float y = world.GetSurfaceY(x, z);
			if (y <= world.GetOceanHeight(x, z) + 0.5)
				return false;
			lowest = Math.Min(lowest, y);
			highest = Math.Max(highest, y);
		}
		if (highest - lowest > m_fBuildingMaxRelief)
			return false;
		TraceBox trace = new TraceBox();
		trace.Start = Vector(site[0], highest + 0.3, site[2]);
		trace.End = trace.Start + Vector(0, 0.05, 0);
		trace.Mins = Vector(-m_fBuildingRadius, 0, -m_fBuildingRadius);
		trace.Maxs = Vector(m_fBuildingRadius, 5, m_fBuildingRadius);
		trace.Exclude = GetOwner();
		trace.Flags = TraceFlags.ENTS;
		return world.TraceMove(trace, null) >= 1;
	}

	//! 1 walkable, 0 not, -1 navmesh tile still loading (asked to load; try again later)
	protected int Walkable(vector point)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return 0;
		NavmeshWorldComponent soldiers = aiWorld.GetNavmeshWorldComponent("Soldiers");
		if (!soldiers)
			return 0;
		if (!soldiers.IsTileLoaded(point))
		{
			soldiers.LoadTileIn(point);
			return -1;
		}
		vector nearby;
		if (!soldiers.GetReachablePoint(point, 2, nearby))
			return 0;
		if (vector.DistanceXZ(point, nearby) <= 3 && Math.AbsFloat(point[1] - nearby[1]) <= 1)
			return 1;
		return 0;
	}

	protected void ApplyPlacement()
	{
		if (!m_bPlaced || !GetOwner())
			return;
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(m_fPlacedYaw, 0, 0), transform);
		transform[3] = m_vPlacedPosition;
		GetOwner().SetWorldTransform(transform);
		// Flags have static collision: commit the child transform, not just its generic parent.
		GetOwner().Update();
		ApplyFlagMaterial();
	}

	//! Server: settle the placement and tell clients
	protected void Settle(vector point, float yaw)
	{
		m_vPlacedPosition = point;
		m_fPlacedYaw = yaw;
		m_bPlaced = true;
		ApplyPlacement();
		Replication.BumpMe();
	}

	protected void TryPlace()
	{
		if (!Replication.IsServer() || m_bPlaced)
			return;
		AROR_LocationComponent location = FindLocation();
		if (!location)
			return;
		if (m_iSiteAttempt < SITE_ATTEMPTS && WantsBuilding(location))
		{
			if (TryPlaceSite(location))
				return;
			m_iSiteAttempt++;
			if (m_iSiteAttempt >= SITE_ATTEMPTS)
				Print(string.Format("[AROR] Garrison post at %1 found no room for its building within %2 m; flag only", location.GetId(), Math.Min(m_fSearchRadius, location.GetRadius() * 0.5)), LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(TryPlace, 100, false);
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		vector center = location.GetCenter();
		float radius = Math.Min(m_fSearchRadius, location.GetRadius() * 0.5);
		vector point = Candidate(center, radius, m_iAttempt);
		point[1] = world.GetSurfaceY(point[0], point[2]) + 0.1;
		bool walkable = Walkable(point) == 1;
		bool clear = IsClear(point);
		if (clear && !m_bHasClearCandidate)
		{
			m_bHasClearCandidate = true;
			m_vClearCandidate = point;
		}
		vector ownerAngles = GetOwner().GetYawPitchRoll();
		if (walkable && clear)
		{
			Settle(point, ownerAngles[0]);
			Print(string.Format("[AROR] Capture flag placed: location=%1 position=%2 offset=%3 attempts=%4", location.GetId(), point, vector.DistanceXZ(point, center), m_iAttempt + 1), LogLevel.NORMAL);
			return;
		}
		m_iAttempt++;
		if (m_iAttempt < 33)
			GetGame().GetCallqueue().CallLater(TryPlace, 250, false);
		else
		{
			// Make a failed search visible instead of leaving the static pole below ground.
			center[1] = world.GetSurfaceY(center[0], center[2]) + 0.1;
			if (m_bHasClearCandidate)
				center = m_vClearCandidate;
			Settle(center, ownerAngles[0]);
			Print(string.Format("[AROR] Capture flag needs placement verification: location=%1 position=%2 clear=%3; no navmesh-verified candidate", location.GetId(), center, m_bHasClearCandidate), LogLevel.WARNING);
		}
	}

	//! One candidate building site, facing the location centre first, then the other three ways. The flag goes in
	//! front of the building and must be clear and walkable.
	protected bool TryPlaceSite(AROR_LocationComponent location)
	{
		BaseWorld world = GetGame().GetWorld();
		vector center = location.GetCenter();
		float radius = Math.Min(m_fSearchRadius, location.GetRadius() * 0.5);
		vector site = Candidate(center, radius, m_iSiteAttempt);
		site[1] = world.GetSurfaceY(site[0], site[2]);
		if (!IsSiteClear(site))
			return false;
		vector towardCentre = center - site;
		towardCentre[1] = 0;
		float baseYaw = 0;
		if (vector.DistanceXZ(center, site) > 1)
			baseYaw = towardCentre.Normalized().ToYaw();
		for (int turn = 0; turn < 4; turn++)
		{
			float yaw = baseYaw + turn * 90;
			vector facing[4];
			Math3D.AnglesToMatrix(Vector(yaw, 0, 0), facing);
			vector flag = site + facing[2] * m_fFlagFront;
			flag[1] = world.GetSurfaceY(flag[0], flag[2]) + 0.1;
			if (!IsClear(flag))
				continue;
			int walkable = Walkable(flag);
			if (walkable == -1 && m_iTileWaits < 40)
			{
				// The tile is loading: look at this site again on the next try
				m_iTileWaits++;
				m_iSiteAttempt--;
				return false;
			}
			if (walkable != 1)
				continue;
			m_bHasSite = true;
			m_vSite = site;
			Settle(flag, yaw);
			RaiseBuilding();
			Print(string.Format("[AROR] Garrison post site: location=%1 building=%2 flag=%3 yaw=%4 offset=%5 attempts=%6", location.GetId(), site, flag, yaw, vector.DistanceXZ(site, center), m_iSiteAttempt + 1), LogLevel.NORMAL);
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName BuildingFor(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.OCCUPIERS)
			return m_sOccupierBuilding;
		if (role == AROR_EFactionRole.INVADERS)
			return m_sInvaderBuilding;
		if (role == AROR_EFactionRole.RESISTANCE)
			return m_sResistanceBuilding;
		return string.Empty;
	}

	//! Server: the holding army's building on the site (replicated by its own RplComponent); swapped when the town
	//! changes hands
	protected void RaiseBuilding()
	{
		if (!Replication.IsServer() || !m_bHasSite)
			return;
		AROR_LocationComponent location = FindLocation();
		if (!location)
			return;
		AROR_EFactionRole role = location.GetControllingRole();
		if (m_Building && role == m_eBuildingRole)
			return;
		RemoveBuilding();
		m_eBuildingRole = role;
		ResourceName prefab = BuildingFor(role);
		if (prefab.IsEmpty())
			return;
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			Print(string.Format("[AROR] Garrison post building %1 failed to load for %2", prefab, location.GetId()), LogLevel.ERROR);
			return;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(m_fPlacedYaw + m_fBuildingYawOffset, 0, 0), params.Transform);
		vector site = m_vSite;
		site[1] = GetGame().GetWorld().GetSurfaceY(site[0], site[2]);
		params.Transform[3] = site;
		m_Building = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!m_Building)
		{
			Print(string.Format("[AROR] Garrison post building %1 did not spawn for %2", prefab, location.GetId()), LogLevel.ERROR);
			return;
		}
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
			aiWorld.RequestNavmeshRebuildEntity(m_Building);
	}

	protected void RemoveBuilding()
	{
		if (m_Building)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Building);
		m_Building = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine with a screen: the flag cloth shows the holding faction's flag
	protected void ApplyFlagMaterial()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		SCR_FlagComponent flag = SCR_FlagComponent.Cast(GetOwner().FindComponent(SCR_FlagComponent));
		AROR_LocationComponent location = FindLocation();
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!flag || !location || !roles)
			return;
		SCR_Faction faction = SCR_Faction.Cast(roles.GetFaction(location.GetControllingRole()));
		if (!faction)
			return;
		ResourceName material = faction.GetFactionFlagMaterial();
		if (!material.IsEmpty())
			flag.ChangeMaterial(material);
	}

	protected void OnLocationControlChanged(AROR_LocationComponent location)
	{
		if (!location || location != FindLocation())
			return;
		ApplyFlagMaterial();
		if (Replication.IsServer())
			RaiseBuilding();
	}

	//! The location manager may register after this flag, so hook up a moment later
	protected void Hook()
	{
		AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
		if (!locations)
		{
			GetGame().GetCallqueue().CallLater(Hook, 1000, false);
			return;
		}
		m_bHooked = true;
		locations.GetOnLocationControlChanged().Insert(OnLocationControlChanged);
		ApplyFlagMaterial();
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		GetGame().GetCallqueue().CallLater(Hook, 500, false);
		if (!m_bAutoPlace)
			return;
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(TryPlace, 250, false);
		else
			ApplyPlacement();
	}

	override void OnDelete(IEntity owner)
	{
		if (GetGame())
		{
			GetGame().GetCallqueue().Remove(TryPlace);
			GetGame().GetCallqueue().Remove(Hook);
		}
		if (m_bHooked)
		{
			AROR_LocationManagerComponent locations = AROR_LocationManagerComponent.GetInstance();
			if (locations)
				locations.GetOnLocationControlChanged().Remove(OnLocationControlChanged);
		}
		if (GetGame() && Replication.IsServer())
			RemoveBuilding();
		super.OnDelete(owner);
	}
}
