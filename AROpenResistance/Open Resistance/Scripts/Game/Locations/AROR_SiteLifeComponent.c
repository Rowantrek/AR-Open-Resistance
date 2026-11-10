[ComponentEditorProps(category: "OpenResistance/Locations", description: "Site life for a military site: builds the camp around its garrison (plated yard, camp props, dirt and tracks on the ground), keeps soldiers moving (fire teams walking the outer perimeter, pairs walking between the buildings, an optional truck) and parks helicopters on its helipads while an AI army holds it. Optionally pays its holder extra income. Put on a location next to its garrison.")]
class AROR_SiteLifeComponentClass : ScriptComponentClass {}

//! One piece of a site's authored dressing, placed in the site's own frame (x right, z forward of the location entity)
[BaseContainerProps()]
class AROR_SpecialSiteProp
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefab or composition to place", params: "et")]
	ResourceName m_sPrefab;
	[Attribute("0 0 0", desc: "Offset from the location entity in its own frame (x right, z forward), metres. Height is ignored: placed on the ground.")]
	vector m_vOffset;
	[Attribute("0", desc: "Turn relative to the location entity, degrees", params: "-360 360 1")]
	float m_fYaw;
	[Attribute("0", desc: "A helipad: while an AI army holds the site, one of its helicopters is parked on it")]
	bool m_bHelipad;
	[Attribute("1", desc: "Soldiers at the site walk over to it while they go about the base")]
	bool m_bWorkPoint;
}

//! Deterministic random numbers: every machine dresses a site identically from its location ID
class AROR_SeededRandom
{
	protected int m_iState;

	void AROR_SeededRandom(int seed)
	{
		m_iState = seed;
		if (m_iState == 0)
			m_iState = 0x2545F491;
	}

	//! 0 .. 2^31-1 (xorshift)
	int Next()
	{
		m_iState = m_iState ^ (m_iState << 13);
		m_iState = m_iState ^ ((m_iState >> 17) & 0x7FFF);
		m_iState = m_iState ^ (m_iState << 5);
		return m_iState & 0x7FFFFFFF;
	}

	//! 0 .. 1
	float NextFloat()
	{
		int value = Next();
		int span = 100000;
		value = Math.Mod(value, span);
		return value / 100000.0;
	}

	//! min .. max - 1
	int Range(int min, int max)
	{
		if (max <= min)
			return min;
		int value = Next();
		int span = max - min;
		value = Math.Mod(value, span);
		return min + value;
	}
}

//! Life at a military site (see ARCHITECTURE.md -> Locations).
//! Every machine, at play start, from fixed data or the location ID (so every player sees the same camp without
//! replicating hundreds of props): optionally removes the authored sandbags, lays steel plating over the yard (it
//! hides the grass), places the authored props list and, with auto dressing, rings the garrison with a varied camp
//! (tents, containers, nets, generators, fuel, stores) on dirt, oil stains and vehicle tracks.
//! Server: optional extra income for the holder; while an AI army holds the site and players are near, a parked
//! helicopter on each helipad, pairs of soldiers walking between the buildings, fire teams walking the outer
//! perimeter and an optional truck driving laps. Lost ones return after the respawn time. Nothing is saved.
class AROR_SiteLifeComponent : ScriptComponent
{
	[Attribute("0", desc: "Extra money paid to the holder every income payment (a special site)", params: "0 100000 50", category: "Income")]
	protected int m_iMoneyPerPayment;
	[Attribute("0", desc: "Extra Resistance manpower every income payment while the Resistance holds it", params: "0 50 1", category: "Income")]
	protected int m_iManpowerPerPayment;

	[Attribute("0", desc: "Remove the location's authored sandbag walls and positions (direct children with a sandbag model) at play start", category: "Layout")]
	protected bool m_bRemoveSandbags;
	[Attribute("{6AC9F0A1B2C3D410}Prefabs/Props/Locations/AROR_Prop_YardPlating.et", UIWidgets.ResourceNamePicker, desc: "Ground plating laid edge to edge over the yard (empty: none). Must occlude clutter, or the grass grows through it.", params: "et", category: "Layout")]
	protected ResourceName m_sFloorPrefab;
	[Attribute("1", desc: "Plate the garrison's compound (its defence square). Off: plate the rectangle below instead.", category: "Layout")]
	protected bool m_bFloorCompound;
	[Attribute("-38 0 -44", desc: "Plated rectangle when not plating the compound: corner in the site's frame, metres", category: "Layout")]
	protected vector m_vFloorMin;
	[Attribute("49 0 41", desc: "...opposite corner", category: "Layout")]
	protected vector m_vFloorMax;
	[Attribute("10", desc: "No plating within this distance of a helipad's centre (the pad has its own deck), metres", params: "0 30 0.5", category: "Layout")]
	protected float m_fPadPlatingGap;
	[Attribute(desc: "Buildings, props and helipads placed at play start", category: "Layout")]
	protected ref array<ref AROR_SpecialSiteProp> m_aProps;

	[Attribute("1", desc: "Ring the garrison's compound with a camp picked from the camp set (seeded by the location ID, so it is the same every time and on every machine)", category: "Auto dressing")]
	protected bool m_bAutoDressing;
	[Attribute("7", desc: "Camp ring distance outside the compound edge, metres (sites with a raised perimeter add 10)", params: "2 40 0.5", category: "Auto dressing")]
	protected float m_fAutoRingOffset;
	[Attribute("11", desc: "Spacing of camp slots along the ring, metres", params: "5 30 0.5", category: "Auto dressing")]
	protected float m_fAutoSpacing;
	[Attribute("0.7", desc: "Share of camp slots that get a prop (the rest only dirt)", params: "0 1 0.05", category: "Auto dressing")]
	protected float m_fAutoFill;
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Camp set to pick from (empty: the built-in US camp set)", params: "et", category: "Auto dressing")]
	protected ref array<ResourceName> m_aAutoPool;

	[Attribute("", UIWidgets.ComboBox, desc: "Helicopter parked on each helipad while an AI army holds the site, in pad order (empty: transport helicopters). Taken from the holder's faction preset.", enums: ParamEnumArray.FromEnum(AROR_EVehicleType), category: "Activity")]
	protected ref array<AROR_EVehicleType> m_aHelicopterTypes;
	[Attribute("2", desc: "Pairs of soldiers walking between the camp's buildings", params: "0 8 1", category: "Activity")]
	protected int m_iWorkerPairs;
	[Attribute("1", desc: "Fire teams walking the perimeter outside the compound", params: "0 6 1", category: "Activity")]
	protected int m_iFootPatrols;
	[Attribute("0", desc: "Radius of the perimeter loop round the compound, metres (0: just outside the camp ring)", params: "0 400 5", category: "Activity")]
	protected float m_fLoopRadius;
	[Attribute("0", desc: "A truck (or car) driving laps of the perimeter loop", category: "Activity")]
	protected bool m_bSupplyTruck;
	[Attribute("25", desc: "The truck's top speed, km/h", params: "5 80 1", category: "Activity")]
	protected float m_fTruckSpeed;

	[Attribute("1300", desc: "Helicopters and soldiers turn out while a player is this close, metres", params: "300 3000 50", category: "Activation")]
	protected float m_fActivationDistance;
	[Attribute("1600", desc: "...and are removed once no player has been this close for half a minute, metres", params: "400 4000 50", category: "Activation")]
	protected float m_fRetireDistance;
	[Attribute("1800", desc: "Seconds before a lost helicopter, team or truck is replaced", params: "120 14400 60", category: "Activation")]
	protected int m_iRespawnSeconds;
	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, desc: "Empty AI group prefab", params: "et", category: "Activation")]
	protected ResourceName m_sGroupPrefab;
	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, params: "et", category: "Activation")]
	protected ResourceName m_sMoveWaypointPrefab;

	protected static const int TICK_SECONDS = 2;
	protected static const int RETIRE_DELAY = 30;
	protected static const int LOOP_POINTS = 8;
	protected static const int MAX_FLOOR_TILES = 900;
	protected static const float PAD_MOVED = 30;		//!< A helicopter this far off its pad has been taken
	protected static const float SLOT_CLEARANCE = 6;	//!< Camp slots keep this far from the site's own pieces (towers, guns, positions)

	// Built-in camp set (US). Big pieces first in each group so the seeded picks spread them out.
	protected static const ResourceName CAMP_TENT_1 = "{D8B2D92435256291}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_US_01.et";
	protected static const ResourceName CAMP_TENT_5 = "{89D2C8FCFC8D6488}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_Barracks_US_05.et";
	protected static const ResourceName CAMP_COMMAND = "{F09296B9255F6704}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_US_01.et";
	protected static const ResourceName CAMP_CONTAINER = "{1B79BB823DC46F9D}Prefabs/Props/Industrial/CargoContainers/CargoContainer_01/CargoContainer_01_20ft_green.et";
	protected static const ResourceName CAMP_NET_LARGE = "{89230C660A90788F}Prefabs/Structures/Military/CamoNets/US/CamoNet_Large_Top_US.et";
	protected static const ResourceName CAMP_NET_MEDIUM = "{972F01488E5B23C2}Prefabs/Structures/Military/CamoNets/US/CamoNet_Medium_Top_US.et";
	protected static const ResourceName CAMP_COVER_LARGE = "{FA9066DE33E8E67D}Prefabs/Structures/Military/Camps/Canvas_Covers/US/CanvasCover_Large_US.et";
	protected static const ResourceName CAMP_COVER_MEDIUM = "{6204943E9EC70EC2}Prefabs/Structures/Military/Camps/Canvas_Covers/US/CanvasCover_Medium_US.et";
	protected static const ResourceName CAMP_GENERATOR = "{7F671B405CDBF0A2}Prefabs/Props/Airport/GeneratorAirfield_US_01/GeneratorAirfield_US_01.et";
	protected static const ResourceName CAMP_FLOODLIGHT = "{0E94F28FD722B1D8}Prefabs/Props/Military/Generators/GeneratorFloodlight_US_01.et";
	protected static const ResourceName CAMP_WATER = "{DFF391458A7AC773}Prefabs/Props/Military/WaterTanks/MobileWaterTank_US_01.et";
	protected static const ResourceName CAMP_FUEL = "{9F35A268E0DD98A4}Prefabs/Props/Military/Fuel/MobileWaterTank_US_01_fuel.et";
	protected static const ResourceName CAMP_KITCHEN = "{FB0F12529D2FD0DA}Prefabs/Props/Military/FieldKitchenTrailerUS_01.et";
	protected static const ResourceName CAMP_PALLET_FUEL = "{DB4F61D5F37432F7}Prefabs/Props/Military/Camps/PalletFuel_01.et";
	protected static const ResourceName CAMP_BARRELS = "{B350840088390760}Prefabs/Compositions/Misc/SubCompositions/Decorations/Barrel_Group_Stack_01.et";
	protected static const ResourceName CAMP_TIRES = "{A00729CC0E1EE305}Prefabs/Props/VehicleParts/Tires/Tire_M998_pile.et";
	protected static const ResourceName CAMP_TIRES_SMALL = "{CEA239D9E1899899}Prefabs/Props/Garbage/Tires/TirePile_01_small.et";
	protected static const ResourceName CAMP_CABLE = "{A419F4547954DA05}Prefabs/Props/Construction/CableReel/CableReelWooden_01.et";
	protected static const ResourceName CAMP_SCRAP = "{8751F77B1038FC72}Prefabs/Props/Industrial/Repair/VehicleGarbage_01_pile_large.et";
	protected static const ResourceName CAMP_DUCKBOARD = "{CC66836BF4E211F9}Prefabs/Props/Military/Camps/Duckboard_01.et";
	// Ground: dirt patches, tracks and stains (projected decals)
	protected static const ResourceName GROUND_DIRT_1 = "{E51E2E1D77B54223}Prefabs/Compositions/Misc/CustomEntities/CompositionDecals/CompositionDecal_Dirt_Patch_01.et";
	protected static const ResourceName GROUND_DIRT_2 = "{894A1AFDCB71CD0D}Prefabs/Compositions/Misc/CustomEntities/CompositionDecals/CompositionDecal_Dirt_Patch_02.et";
	protected static const ResourceName GROUND_DIRT_3 = "{6CD65604C7945A66}Prefabs/Compositions/Misc/CustomEntities/CompositionDecals/CompositionDecal_Dirt_Patch_03.et";
	protected static const ResourceName GROUND_TRACKS_1 = "{F2A6A3DA58AFD5D2}Prefabs/Compositions/Misc/CustomEntities/CompositionDecals/CompositionDecal_Dirt_Tracks_01.et";
	protected static const ResourceName GROUND_TRACKS_2 = "{9EF2973AE46B5AFC}Prefabs/Compositions/Misc/CustomEntities/CompositionDecals/CompositionDecal_Dirt_Tracks_02.et";
	protected static const ResourceName GROUND_OIL = "{C977A93D74F5356A}Prefabs/Compositions/Misc/CustomEntities/CompositionDecals/CompositionDecal_OilStain_01_medium.et";
	protected static const ResourceName GROUND_PETROL = "{9E9221B0DF3E1837}Prefabs/Compositions/Misc/CustomEntities/CompositionDecals/CompositionDecal_PetrolStain_01_V1.et";

	protected AROR_LocationComponent m_Location;
	protected bool m_bBuilt;
	protected bool m_bActive;
	protected int m_iClock;
	protected int m_iFarSince = -1;
	protected AROR_EFactionRole m_eRole;
	protected vector m_vCompound;		//!< World centre of the garrison's compound (its flag), or the location centre
	protected float m_fCompoundX = 20;	//!< Compound half sizes in the site's frame
	protected float m_fCompoundZ = 20;
	protected float m_fLoop;			//!< Perimeter loop radius in use
	protected ref array<IEntity> m_aDressing = {};		//!< Every machine: props and plating placed here (local entities)
	protected ref array<vector> m_aPads = {};
	protected ref array<float> m_aPadYaws = {};
	protected ref array<IEntity> m_aPadEntities = {};
	protected ref array<vector> m_aWorkPoints = {};
	protected ref array<IEntity> m_aHelicopters = {};	//!< Server, parallel to the pads; null when none stands there
	protected ref array<int> m_aHelicopterLostAt = {};	//!< Parallel: clock when its helicopter was lost, -1 none
	protected ref array<ref AROR_SpecialSiteGroup> m_aPatrols = {};
	protected ref array<ref AROR_SpecialSiteGroup> m_aWorkers = {};
	protected ref array<int> m_aPatrolLostAt = {};
	protected ref array<int> m_aWorkerLostAt = {};
	protected ref AROR_SpecialSiteTruck m_Truck;
	protected int m_iTruckLostAt = -1;
	protected ref array<ref AROR_SpecialSiteGroup> m_aRetiringGroups = {};
	protected ref array<IEntity> m_aRetiringHelicopters = {};
	protected static ref array<AROR_SiteLifeComponent> s_aSites = {};
	[Attribute("48", desc: "Shared personnel ceiling for site activity, including pending groups and former holders. Garrisons and operations retain their own budgets.", params: "8 144 1", category: "Activity")]
	protected int m_iActivityPersonnelBudget;

	//------------------------------------------------------------------------------------------------
	int GetMoneyPerPayment() { return m_iMoneyPerPayment; }
	int GetManpowerPerPayment() { return m_iManpowerPerPayment; }

	//------------------------------------------------------------------------------------------------
	//! Every machine, once: sandbags out, plating, the authored props and the camp ring in
	protected void Build()
	{
		if (m_bBuilt)
			return;
		m_bBuilt = true;

		IEntity owner = GetOwner();
		ReadCompound(owner);
		if (m_bRemoveSandbags)
			RemoveSandbags(owner);

		int props = PlaceProps(owner);
		int tiles = LayFloor(owner);
		int camp;
		if (m_bAutoDressing)
			camp = DressCamp(owner);
		Print(string.Format("[AROR SITE] %1 built: %2 plates, %3 props, %4 camp pieces, %5 helipads, perimeter loop %6 m", m_Location.GetId(), tiles, props, camp, m_aPads.Count(), m_fLoop), LogLevel.NORMAL);
	}

	//! The garrison's compound (centred on its flag) and the perimeter loop round it
	protected void ReadCompound(IEntity owner)
	{
		m_vCompound = m_Location.GetCenter();
		AROR_GarrisonComponent garrison = AROR_GarrisonComponent.Cast(owner.FindComponent(AROR_GarrisonComponent));
		if (garrison)
		{
			m_vCompound = garrison.GetDefenceCenter();
			m_fCompoundX = garrison.GetDefenceHalfExtent();
			m_fCompoundZ = garrison.GetDefenceHalfDepth();
		}

		m_fLoop = m_fLoopRadius;
		if (m_fLoop <= 0)
			m_fLoop = Math.Max(m_fCompoundX, m_fCompoundZ) * 1.42 + RingOffset(owner) + 8;
	}

	protected float RingOffset(IEntity owner)
	{
		// A raised perimeter (walls and wire) stands round the compound: the camp goes outside it
		if (owner.FindComponent(AROR_PerimeterComponent))
			return m_fAutoRingOffset + 10;
		return m_fAutoRingOffset;
	}

	//------------------------------------------------------------------------------------------------
	//! Direct children that are sandbags, told by their model (children inherited through a composition do not
	//! always report their own prefab name). Guard towers and other compositions have no model of their own and stay.
	protected void RemoveSandbags(IEntity owner)
	{
		array<IEntity> doomed = {};
		IEntity child = owner.GetChildren();
		while (child)
		{
			if (IsSandbag(child))
				doomed.Insert(child);
			child = child.GetSibling();
		}

		foreach (IEntity sandbag : doomed)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(sandbag);
		}

		Print(string.Format("[AROR SITE] %1 removed %2 sandbag pieces", m_Location.GetId(), doomed.Count()), LogLevel.NORMAL);
	}

	protected static bool IsSandbag(IEntity entity)
	{
		string name;
		VObject model = entity.GetVObject();
		if (model)
			name = model.GetResourceName();

		EntityPrefabData data = entity.GetPrefabData();
		if (data)
			name += " " + data.GetPrefabName();

		name.ToLower();
		return name.Contains("sandbag");
	}

	//------------------------------------------------------------------------------------------------
	//! Plates laid edge to edge over the compound (or the authored rectangle), measured from the prefab itself, each
	//! tilted to the ground under it so none floats or sinks on a slope
	protected int LayFloor(IEntity owner)
	{
		if (m_sFloorPrefab.IsEmpty())
			return 0;

		vector siteAngles = owner.GetYawPitchRoll();
		float yaw = siteAngles[0];
		IEntity probe = SpawnLocal(m_sFloorPrefab, owner.GetOrigin(), yaw);
		if (!probe)
			return 0;

		vector mins, maxs;
		probe.GetBounds(mins, maxs);
		SCR_EntityHelper.DeleteEntityAndChildren(probe);
		float sizeX = maxs[0] - mins[0];
		float sizeZ = maxs[2] - mins[2];
		if (sizeX < 0.5 || sizeZ < 0.5)
			return 0;

		vector centreOffset = (mins + maxs) * 0.5;
		centreOffset[1] = 0;

		float left, right, back, front;
		if (m_bFloorCompound)
		{
			vector compound = owner.CoordToLocal(m_vCompound);
			left = compound[0] - m_fCompoundX + 1;
			right = compound[0] + m_fCompoundX - 1;
			back = compound[2] - m_fCompoundZ + 1;
			front = compound[2] + m_fCompoundZ - 1;
		}
		else
		{
			left = Math.Min(m_vFloorMin[0], m_vFloorMax[0]);
			right = Math.Max(m_vFloorMin[0], m_vFloorMax[0]);
			back = Math.Min(m_vFloorMin[2], m_vFloorMax[2]);
			front = Math.Max(m_vFloorMin[2], m_vFloorMax[2]);
		}

		// Helipads keep their own deck: no plating under them
		array<vector> padOffsets = {};
		if (m_aProps)
		{
			foreach (AROR_SpecialSiteProp prop : m_aProps)
			{
				if (prop && prop.m_bHelipad)
					padOffsets.Insert(prop.m_vOffset);
			}
		}

		vector siteRight = owner.VectorToParent(Vector(1, 0, 0));
		vector siteForward = owner.VectorToParent(Vector(0, 0, 1));
		int columns = Math.Ceil((right - left) / sizeX);
		int rows = Math.Ceil((front - back) / sizeZ);
		BaseWorld world = GetGame().GetWorld();
		int laid;
		for (int column = 0; column < columns; column++)
		{
			for (int row = 0; row < rows; row++)
			{
				if (laid >= MAX_FLOOR_TILES)
					return laid;

				vector siteOffset = Vector(left + (column + 0.5) * sizeX, 0, back + (row + 0.5) * sizeZ);
				bool underPad;
				foreach (vector padOffset : padOffsets)
				{
					if (Math.AbsFloat(siteOffset[0] - padOffset[0]) < m_fPadPlatingGap && Math.AbsFloat(siteOffset[2] - padOffset[2]) < m_fPadPlatingGap)
						underPad = true;
				}
				if (underPad)
					continue;

				// The ground's slope across the plate, from its edge midpoints
				vector centre = owner.CoordToParent(siteOffset);
				float leftY = SurfaceAt(world, centre - siteRight * (sizeX * 0.5));
				float rightY = SurfaceAt(world, centre + siteRight * (sizeX * 0.5));
				float backY = SurfaceAt(world, centre - siteForward * (sizeZ * 0.5));
				float frontY = SurfaceAt(world, centre + siteForward * (sizeZ * 0.5));
				float centreY = Math.Max(SurfaceAt(world, centre), (leftY + rightY + backY + frontY) * 0.25);

				vector axisX = siteRight + Vector(0, (rightY - leftY) / sizeX, 0);
				vector axisZ = siteForward + Vector(0, (frontY - backY) / sizeZ, 0);
				axisX.Normalize();
				axisZ.Normalize();
				vector axisY = Cross(axisZ, axisX);
				axisY.Normalize();
				axisZ = Cross(axisX, axisY);
				axisZ.Normalize();

				centre[1] = centreY;
				vector origin = centre - axisX * centreOffset[0] - axisZ * centreOffset[2] - axisY * (mins[1] - 0.01);
				IEntity tile = SpawnLocalTransform(m_sFloorPrefab, axisX, axisY, axisZ, origin);
				if (!tile)
					return laid;

				m_aDressing.Insert(tile);
				laid++;
			}
		}

		return laid;
	}

	//! Cross product a x b
	protected static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}

	protected static float SurfaceAt(BaseWorld world, vector position)
	{
		return world.GetSurfaceY(position[0], position[2]);
	}

	//------------------------------------------------------------------------------------------------
	protected int PlaceProps(IEntity owner)
	{
		if (!m_aProps)
			return 0;

		vector siteAngles = owner.GetYawPitchRoll();
		float siteYaw = siteAngles[0];
		BaseWorld world = GetGame().GetWorld();
		int placed;
		foreach (AROR_SpecialSiteProp prop : m_aProps)
		{
			if (!prop || prop.m_sPrefab.IsEmpty())
				continue;

			vector siteOffset = prop.m_vOffset;
			siteOffset[1] = 0;
			vector position = owner.CoordToParent(siteOffset);
			position[1] = world.GetSurfaceY(position[0], position[2]);
			float yaw = siteYaw + prop.m_fYaw;
			IEntity entity = SpawnLocal(prop.m_sPrefab, position, yaw);
			if (!entity)
			{
				Print(string.Format("[AROR SITE] %1 could not place %2", m_Location.GetId(), prop.m_sPrefab), LogLevel.WARNING);
				continue;
			}

			m_aDressing.Insert(entity);
			placed++;
			if (prop.m_bHelipad)
			{
				m_aPads.Insert(position);
				m_aPadYaws.Insert(yaw);
				m_aPadEntities.Insert(entity);
				m_aHelicopters.Insert(null);
				m_aHelicopterLostAt.Insert(-1);
			}
			else if (prop.m_bWorkPoint)
				AddWorkPoint(position);
		}

		return placed;
	}

	//! Beside a building, on the side facing the compound
	protected void AddWorkPoint(vector position)
	{
		vector toCentre = m_vCompound - position;
		toCentre[1] = 0;
		float distance = toCentre.Length();
		if (distance > 1)
			position = position + toCentre * (Math.Min(5, distance) / distance);
		m_aWorkPoints.Insert(position);
	}

	//------------------------------------------------------------------------------------------------
	//! The camp ring: slots every few metres on a rectangle just outside the compound, each with dirt on the ground
	//! and most with a camp piece facing the compound. Slots near roads (the way in), water, steep ground or the
	//! site's own pieces are left clear. Same location ID, same camp.
	protected int DressCamp(IEntity owner)
	{
		array<ResourceName> pool = {};
		if (m_aAutoPool && !m_aAutoPool.IsEmpty())
			pool.InsertAll(m_aAutoPool);
		else
			GetCampSet(pool);

		array<ResourceName> ground = {GROUND_DIRT_1, GROUND_DIRT_2, GROUND_DIRT_3, GROUND_DIRT_1, GROUND_TRACKS_1, GROUND_TRACKS_2, GROUND_OIL, GROUND_PETROL};
		AROR_SeededRandom random = new AROR_SeededRandom(m_Location.GetId().Hash());
		BaseWorld world = GetGame().GetWorld();
		vector siteAngles = owner.GetYawPitchRoll();
		float siteYaw = siteAngles[0];
		vector compound = owner.CoordToLocal(m_vCompound);
		float ring = RingOffset(owner);
		float halfX = m_fCompoundX + ring;
		float halfZ = m_fCompoundZ + ring;

		// Things already standing at the site: towers, guns, defence positions, the flag
		array<vector> occupied = {};
		IEntity child = owner.GetChildren();
		while (child)
		{
			occupied.Insert(child.GetOrigin());
			child = child.GetSibling();
		}
		foreach (vector pad : m_aPads)
		{
			occupied.Insert(pad);
		}

		int placed;
		float perimeter = 4 * (halfX + halfZ);
		float start = random.NextFloat() * m_fAutoSpacing;
		for (float along = start; along < perimeter; along += m_fAutoSpacing)
		{
			// Walk the rectangle: back edge, right edge, front edge, left edge; face inwards
			vector siteOffset;
			float facing;
			float edge = along;
			if (edge < 2 * halfX)
			{
				siteOffset = Vector(-halfX + edge, 0, -halfZ);
				facing = 0;
			}
			else if (edge < 2 * halfX + 2 * halfZ)
			{
				siteOffset = Vector(halfX, 0, -halfZ + edge - 2 * halfX);
				facing = 270;
			}
			else if (edge < 4 * halfX + 2 * halfZ)
			{
				siteOffset = Vector(halfX - (edge - 2 * halfX - 2 * halfZ), 0, halfZ);
				facing = 180;
			}
			else
			{
				siteOffset = Vector(-halfX, 0, halfZ - (edge - 4 * halfX - 2 * halfZ));
				facing = 90;
			}

			// Never right on a corner, where two edges' pieces would meet
			if (Math.AbsFloat(Math.AbsFloat(siteOffset[0]) - halfX) < 3 && Math.AbsFloat(Math.AbsFloat(siteOffset[2]) - halfZ) < 3)
				continue;

			// Draw every slot's numbers even when it stays empty, so one blocked slot never reshuffles the rest
			int pick = random.Range(0, pool.Count());
			int groundPick = random.Range(0, ground.Count());
			bool filled = random.NextFloat() < m_fAutoFill;
			float jitter = random.Range(-8, 9);
			float yaw = siteYaw + facing + jitter;

			vector position = owner.CoordToParent(compound + siteOffset);
			position[1] = world.GetSurfaceY(position[0], position[2]);
			if (!IsCampSlot(world, position, occupied))
				continue;

			IEntity dirt = SpawnLocal(ground[groundPick], position, yaw);
			if (dirt)
				m_aDressing.Insert(dirt);

			if (!filled)
				continue;

			IEntity piece = SpawnLocal(pool[pick], position, yaw);
			if (!piece)
				continue;

			m_aDressing.Insert(piece);
			occupied.Insert(position);
			AddWorkPoint(position);
			placed++;

			// Duckboards in front of a tent's door
			if (pool[pick] == CAMP_TENT_1 || pool[pick] == CAMP_TENT_5 || pool[pick] == CAMP_COMMAND)
			{
				vector front = owner.VectorToParent(Vector(Math.Sin((facing) * Math.DEG2RAD), 0, Math.Cos((facing) * Math.DEG2RAD)));
				vector step = position + front * 5;
				step[1] = world.GetSurfaceY(step[0], step[2]);
				IEntity board = SpawnLocal(CAMP_DUCKBOARD, step, yaw);
				if (board)
					m_aDressing.Insert(board);
			}
		}

		return placed;
	}

	//! A camp slot: dry, nearly level, off the roads (the way in stays open) and clear of the site's own pieces
	protected bool IsCampSlot(BaseWorld world, vector position, notnull array<vector> occupied)
	{
		if (position[1] <= world.GetOceanHeight(position[0], position[2]) + 0.5)
			return false;

		float low = position[1];
		float high = position[1];
		for (int corner = 0; corner < 4; corner++)
		{
			float angle = corner * Math.PI * 0.5;
			float y = world.GetSurfaceY(position[0] + Math.Sin(angle) * 4, position[2] + Math.Cos(angle) * 4);
			low = Math.Min(low, y);
			high = Math.Max(high, y);
		}
		if (high - low > 1.6)
			return false;

		foreach (vector taken : occupied)
		{
			if (vector.DistanceXZ(position, taken) < SLOT_CLEARANCE)
				return false;
		}

		return !AROR_ForceSpawner.IsNearRoad(position, 5);
	}

	//! The built-in US camp set; repeats weight the picks towards the everyday pieces
	protected static void GetCampSet(notnull array<ResourceName> pool)
	{
		pool.Insert(CAMP_TENT_1);
		pool.Insert(CAMP_TENT_5);
		pool.Insert(CAMP_COMMAND);
		pool.Insert(CAMP_CONTAINER);
		pool.Insert(CAMP_CONTAINER);
		pool.Insert(CAMP_NET_LARGE);
		pool.Insert(CAMP_NET_MEDIUM);
		pool.Insert(CAMP_COVER_LARGE);
		pool.Insert(CAMP_COVER_MEDIUM);
		pool.Insert(CAMP_GENERATOR);
		pool.Insert(CAMP_FLOODLIGHT);
		pool.Insert(CAMP_FLOODLIGHT);
		pool.Insert(CAMP_WATER);
		pool.Insert(CAMP_FUEL);
		pool.Insert(CAMP_KITCHEN);
		pool.Insert(CAMP_PALLET_FUEL);
		pool.Insert(CAMP_BARRELS);
		pool.Insert(CAMP_BARRELS);
		pool.Insert(CAMP_TIRES);
		pool.Insert(CAMP_TIRES_SMALL);
		pool.Insert(CAMP_CABLE);
		pool.Insert(CAMP_SCRAP);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawned on this machine only (yaw in degrees, position used as given)
	protected static IEntity SpawnLocal(ResourceName prefab, vector position, float yaw)
	{
		vector rotation[3];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), rotation);
		return SpawnLocalTransform(prefab, rotation[0], rotation[1], rotation[2], position);
	}

	protected static IEntity SpawnLocalTransform(ResourceName prefab, vector right, vector up, vector forward, vector position)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = right;
		params.Transform[1] = up;
		params.Transform[2] = forward;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Server
	protected void Tick()
	{
		if (!Replication.IsServer() || !m_Location || !m_bBuilt)
			return;

		AROR_CommanderManagerComponent commanders = AROR_CommanderManagerComponent.GetInstance();
		if (!commanders || !commanders.IsCampaignActive())
			return;

		m_iClock += TICK_SECONDS;
		vector centre = m_Location.GetCenter();
		if (!m_bActive)
		{
			if (NearestPlayer(centre) <= m_fActivationDistance)
			{
				m_bActive = true;
				m_iFarSince = -1;
				Print(string.Format("[AROR SITE] %1 active (holder role %2)", m_Location.GetId(), m_Location.GetControllingRole()), LogLevel.NORMAL);
			}
			return;
		}

		if (NearestPlayer(centre) <= m_fRetireDistance)
			m_iFarSince = -1;
		else if (m_iFarSince < 0)
			m_iFarSince = m_iClock;
		else if (m_iClock - m_iFarSince >= RETIRE_DELAY)
		{
			Retire();
			return;
		}

		AROR_EFactionRole role = m_Location.GetControllingRole();
		if (role != m_eRole)
		{
			// A new holder: the old army's helicopters and soldiers are left where they are, no longer this site's
			Forget();
			m_eRole = role;
		}
		ReapFormerForces(false);

		// Former defenders retain ownership and count against capacity until removed or legitimately taken.
		if (!AROR_GarrisonPolicy.IsEnemyRole(role) || !m_aRetiringGroups.IsEmpty() || !m_aRetiringHelicopters.IsEmpty())
			return;

		UpdateHelicopters(role);
		UpdateGroups(role, m_aWorkers, m_aWorkerLostAt, m_iWorkerPairs, true);
		UpdateGroups(role, m_aPatrols, m_aPatrolLostAt, m_iFootPatrols, false);
		UpdateTruck(role);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHelicopters(AROR_EFactionRole role)
	{
		foreach (int index, vector pad : m_aPads)
		{
			IEntity helicopter = m_aHelicopters[index];
			if (helicopter)
			{
				bool gone = AROR_ForceSpawner.IsDestroyed(helicopter) || AROR_ResistanceFleet.IsClaimed(helicopter) || vector.DistanceXZ(helicopter.GetOrigin(), pad) > PAD_MOVED;
				if (!gone)
					continue;

				Print(string.Format("[AROR SITE] %1 helicopter on pad %2 lost or taken", m_Location.GetId(), index), LogLevel.NORMAL);
				m_aHelicopters[index] = null;
				m_aHelicopterLostAt[index] = m_iClock;
				continue;
			}

			int lost = m_aHelicopterLostAt[index];
			if (lost >= 0 && m_iClock - lost < m_iRespawnSeconds)
				continue;

			// Not in front of a player standing on the pad
			if (NearestPlayer(pad) < 15)
				continue;

			ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(role, HelicopterType(index));
			if (prefab.IsEmpty())
				prefab = AROR_ForceSpawner.VehiclePrefab(role, AROR_EVehicleType.HELI_TRANSPORT);
			if (prefab.IsEmpty())
			{
				Print(string.Format("[AROR SITE] %1 pad %2: the holder's preset has no helicopter", m_Location.GetId(), index), LogLevel.WARNING);
				m_aHelicopterLostAt[index] = m_iClock;
				continue;
			}

			IEntity spawned = AROR_ForceSpawner.SpawnVehicle(role, prefab, pad, m_aPadYaws[index]);
			if (!spawned)
			{
				m_aHelicopterLostAt[index] = m_iClock;
				continue;
			}

			RaiseOntoDeck(spawned, pad);
			m_aHelicopters[index] = spawned;
			m_aHelicopterLostAt[index] = -1;
			Print(string.Format("[AROR SITE] %1 pad %2: %3 parked", m_Location.GetId(), index, prefab), LogLevel.NORMAL);
		}
	}

	protected AROR_EVehicleType HelicopterType(int index)
	{
		if (m_aHelicopterTypes && index < m_aHelicopterTypes.Count())
			return m_aHelicopterTypes[index];

		return AROR_EVehicleType.HELI_TRANSPORT;
	}

	//! Spawned on the terrain; lifted onto whatever deck the pad has
	protected void RaiseOntoDeck(IEntity vehicle, vector pad)
	{
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(pad[0], pad[2]);
		TraceParam trace = new TraceParam();
		trace.Start = Vector(pad[0], ground + 3, pad[2]);
		trace.End = Vector(pad[0], ground - 0.5, pad[2]);
		trace.Flags = TraceFlags.ENTS;
		trace.Exclude = vehicle;
		float fraction = world.TraceMove(trace, null);
		if (fraction >= 1 || !m_aPadEntities.Contains(trace.TraceEnt))
			return;

		float deck = trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction;
		if (deck <= ground + 0.05)
			return;

		vector origin = vehicle.GetOrigin();
		origin[1] = origin[1] + (deck - ground);
		vehicle.SetOrigin(origin);
		vehicle.Update();
	}

	//------------------------------------------------------------------------------------------------
	//! Workers (pairs walking between the camp's buildings) or patrols (fire teams walking the outer perimeter)
	protected void UpdateGroups(AROR_EFactionRole role, array<ref AROR_SpecialSiteGroup> groups, array<int> lostAt, int wanted, bool workers)
	{
		if (workers && m_aWorkPoints.IsEmpty())
			return;

		for (int index = groups.Count() - 1; index >= 0; index--)
		{
			AROR_SpecialSiteGroup entry = groups[index];
			array<IEntity> living = {};
			AROR_ForceSpawner.CountLiving(entry.m_Group, living);
			int alive = AROR_ForceSpawner.CountStrength(entry.m_Group);
			if (!entry.m_Group || (alive == 0 && (entry.m_bSeen || m_iClock - entry.m_iRaisedAt > 60)))
			{
				// Also after a failed spawn, so a broken preset never retries every tick
				if (entry.Remove_S())
				{
					lostAt.Insert(m_iClock);
					groups.Remove(index);
				}
				continue;
			}

			if (alive > 0)
				entry.m_bSeen = true;
			AROR_ForceSpawner.RenewSimulation_S(entry.m_Group);

			if (living.IsEmpty())
			{
				if (alive > 0 && m_iClock >= entry.m_iNextWake)
				{
					entry.m_iNextWake = m_iClock + 30;
					entry.m_Group.RequestSpawn();
				}
				continue;
			}
			bool arrived = vector.DistanceXZ(living[0].GetOrigin(), entry.m_vGoal) < 8;
			if (m_iClock < entry.m_iNextOrderAt && !arrived)
				continue;

			if (workers)
			{
				// Stop a while at each building, then on to another
				if (arrived && entry.m_iLingerUntil == 0)
				{
					entry.m_iLingerUntil = m_iClock + Math.RandomInt(20, 50);
					continue;
				}
				if (entry.m_iLingerUntil > 0 && m_iClock < entry.m_iLingerUntil)
					continue;

				entry.m_iLingerUntil = 0;
				entry.m_vGoal = m_aWorkPoints.GetRandomElement();
				entry.m_iNextOrderAt = m_iClock + 90;
			}
			else
			{
				int points = LOOP_POINTS;
				int stepped = entry.m_iPoint + entry.m_iDirection + points;
				entry.m_iPoint = Math.Mod(stepped, points);
				entry.m_vGoal = LoopPoint(entry.m_iPoint);
				entry.m_iNextOrderAt = m_iClock + 120;
			}
			entry.Order_S(m_sMoveWaypointPrefab, entry.m_vGoal, 4);
		}

		for (int lost = lostAt.Count() - 1; lost >= 0; lost--)
		{
			if (m_iClock - lostAt[lost] >= m_iRespawnSeconds)
				lostAt.Remove(lost);
		}

		if (groups.Count() + lostAt.Count() >= wanted)
			return;

		// One new group per tick, at the spot furthest from any player
		array<vector> spots = {};
		if (workers)
			spots.InsertAll(m_aWorkPoints);
		else
		{
			for (int point = 0; point < LOOP_POINTS; point++)
			{
				spots.Insert(LoopPoint(point));
			}
		}

		int start = FurthestFromPlayers(spots);
		vector spawn = spots[start];
		vector clear;
		if (AROR_ForceSpawner.FindClearPosition(spawn, 10, 1, clear))
			spawn = clear;

		array<AROR_EUnitType> units = {};
		if (workers)
		{
			units.Insert(AROR_EUnitType.RIFLEMAN);
			units.Insert(AROR_EUnitType.RIFLEMAN);
		}
		else
			AROR_Formations.GetUnits(AROR_EFormation.RIFLE_FIRE_TEAM, units);
		if (!HasActivityCapacity(units.Count()))
			return;

		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, role, units, spawn);
		if (!group)
			return;

		AROR_SpecialSiteGroup added = new AROR_SpecialSiteGroup();
		added.m_Group = group;
		added.m_iSize = units.Count();
		added.m_iRaisedAt = m_iClock;
		added.m_iPoint = start;
		added.m_vGoal = spawn;
		added.m_iDirection = 1;
		int existing = groups.Count();
		if (Math.Mod(existing, 2) == 1)
			added.m_iDirection = -1;
		added.m_iNextOrderAt = m_iClock + 6;
		groups.Insert(added);
		string kind = "perimeter patrol";
		if (workers)
			kind = "work detail";
		Print(string.Format("[AROR SITE] %1 %2 raised for role %3", m_Location.GetId(), kind, role), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! A truck with a driver and a passenger driving laps of the perimeter loop
	protected void UpdateTruck(AROR_EFactionRole role)
	{
		if (m_Truck)
		{
			array<IEntity> crew = {};
			AROR_ForceSpawner.CountLiving(m_Truck.m_Group, crew);
			int alive = AROR_ForceSpawner.CountStrength(m_Truck.m_Group);
			AROR_ForceSpawner.RenewSimulation_S(m_Truck.m_Group);
			if (m_Truck.m_Group && m_Truck.m_Group.IsDormant() && alive > 0)
			{
				if (m_iClock >= m_Truck.m_iNextWake)
				{
					m_Truck.m_iNextWake = m_iClock + 30;
					m_Truck.m_Group.RequestSpawn();
				}
				return;
			}
			bool lost = !m_Truck.m_Vehicle || AROR_ForceSpawner.IsDestroyed(m_Truck.m_Vehicle) || AROR_ResistanceFleet.IsClaimed(m_Truck.m_Vehicle);
			if (m_Truck.m_bSeated && alive == 0)
				lost = true;
			if (!m_Truck.m_bSeated && m_iClock - m_Truck.m_iRaisedAt > 90)
			{
				Print(string.Format("[AROR SITE] %1 truck crew could not board; removed", m_Location.GetId()), LogLevel.WARNING);
				lost = true;
			}

			if (lost)
			{
				if (!m_Truck.Remove_S())
					m_aRetiringGroups.Insert(m_Truck);
				m_Truck = null;
				m_iTruckLostAt = m_iClock;
				return;
			}

			if (!m_Truck.m_bSeated)
			{
				int expected = 2;
				if (m_iClock - m_Truck.m_iRaisedAt > 30)
					expected = Math.Max(alive, 1);
				if (AROR_ForceSpawner.SeatGroup(m_Truck.m_Group, m_Truck.m_Vehicle, expected))
				{
					m_Truck.m_bSeated = true;
					m_Truck.m_iNextOrderAt = m_iClock;
				}
				return;
			}

			bool arrived = vector.DistanceXZ(m_Truck.m_Vehicle.GetOrigin(), m_Truck.m_vGoal) < 25;
			if (m_iClock >= m_Truck.m_iNextOrderAt || arrived)
			{
				int loopPoints = LOOP_POINTS;
				int nextPoint = m_Truck.m_iPoint + 1;
				m_Truck.m_iPoint = Math.Mod(nextPoint, loopPoints);
				m_Truck.m_vGoal = LoopPoint(m_Truck.m_iPoint);
				m_Truck.Order_S(m_sMoveWaypointPrefab, m_Truck.m_vGoal, 20);
				m_Truck.m_iNextOrderAt = m_iClock + 120;
				AROR_ForceSpawner.SetCruiseSpeed(m_Truck.m_Vehicle, m_Truck.m_Group, m_fTruckSpeed);
			}
			return;
		}

		if (!m_bSupplyTruck || !HasActivityCapacity(2) || (m_iTruckLostAt >= 0 && m_iClock - m_iTruckLostAt < m_iRespawnSeconds))
			return;

		ResourceName prefab = AROR_ForceSpawner.VehiclePrefab(role, AROR_EVehicleType.MOTOR_TRUCK);
		if (prefab.IsEmpty())
			prefab = AROR_ForceSpawner.VehiclePrefab(role, AROR_EVehicleType.MOTOR_CAR);
		if (prefab.IsEmpty())
		{
			m_iTruckLostAt = m_iClock;
			return;
		}

		array<vector> spots = {};
		for (int point = 0; point < LOOP_POINTS; point++)
		{
			spots.Insert(LoopPoint(point));
		}
		int start = FurthestFromPlayers(spots);
		vector position;
		float yaw;
		if (!AROR_ForceSpawner.FindRoadsidePosition(spots[start], 40, false, position, yaw))
		{
			if (!AROR_ForceSpawner.FindClearPosition(spots[start], 25, 5, position))
				return;
			yaw = Math.RandomFloat(0, 360);
		}

		IEntity vehicle = AROR_ForceSpawner.SpawnVehicle(role, prefab, position, yaw);
		if (!vehicle)
		{
			m_iTruckLostAt = m_iClock;
			return;
		}

		array<AROR_EUnitType> units = {AROR_EUnitType.CREW, AROR_EUnitType.RIFLEMAN};
		vector side = position;
		vector clear;
		if (AROR_ForceSpawner.FindClearPosition(position, 10, 1, clear))
			side = clear;
		SCR_AIGroup group = AROR_ForceSpawner.SpawnGroup(m_sGroupPrefab, role, units, side);
		if (!group)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			m_iTruckLostAt = m_iClock;
			return;
		}

		m_Truck = new AROR_SpecialSiteTruck();
		m_Truck.m_iSize = 2;
		m_Truck.m_Vehicle = vehicle;
		m_Truck.m_Group = group;
		m_Truck.m_iRaisedAt = m_iClock;
		m_Truck.m_iPoint = start;
		m_Truck.m_vGoal = position;
		Print(string.Format("[AROR SITE] %1 truck raised for role %2: %3", m_Location.GetId(), role, prefab), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! A point on the perimeter loop round the compound
	protected vector LoopPoint(int point)
	{
		float bearing = point * Math.PI2 / LOOP_POINTS;
		vector spot = m_vCompound + Vector(Math.Sin(bearing), 0, Math.Cos(bearing)) * m_fLoop;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		return spot;
	}

	//! Distance to the nearest player (a large number when there is none)
	protected float NearestPlayer(vector position)
	{
		float nearest = AROR_ResistancePresence.Nearest(position);
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (player)
				nearest = Math.Min(nearest, vector.DistanceXZ(player.GetOrigin(), position));
		}

		return nearest;
	}

	//! Index of the spot furthest from every player (players often stand inside the base, so never wait for a spot
	//! nobody can see)
	protected int FurthestFromPlayers(notnull array<vector> spots)
	{
		int best;
		float bestDistance = -1;
		foreach (int index, vector spot : spots)
		{
			float distance = NearestPlayer(spot);
			if (distance > bestDistance)
			{
				best = index;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Nobody near: soldiers and the truck go, and helicopters still on their pads (unclaimed) are put away
	protected void Retire()
	{
		Forget();
		ReapFormerForces(true);
		m_bActive = !m_aRetiringGroups.IsEmpty() || !m_aRetiringHelicopters.IsEmpty();

		Print(string.Format("[AROR SITE] %1 retired", m_Location.GetId()), LogLevel.NORMAL);
	}

	//! Control changed while players are here: stop managing the old holder's helicopters and soldiers
	protected void Forget()
	{
		// InsertAll expects an array of weak references; owned arrays must transfer elements individually.
		foreach (AROR_SpecialSiteGroup patrol : m_aPatrols)
			m_aRetiringGroups.Insert(patrol);
		foreach (AROR_SpecialSiteGroup worker : m_aWorkers)
			m_aRetiringGroups.Insert(worker);
		if (m_Truck)
			m_aRetiringGroups.Insert(m_Truck);
		m_aPatrols.Clear();
		m_aWorkers.Clear();
		m_aPatrolLostAt.Clear();
		m_aWorkerLostAt.Clear();
		m_Truck = null;
		m_iTruckLostAt = -1;
		for (int index = 0; index < m_aHelicopters.Count(); index++)
		{
			if (m_aHelicopters[index])
				m_aRetiringHelicopters.Insert(m_aHelicopters[index]);
			m_aHelicopters[index] = null;
			m_aHelicopterLostAt[index] = -1;
		}
	}

	//! Retirement owns former holders too. Keep live fighters near commanded units or active opposing forces.
	protected void ReapFormerForces(bool retiring)
	{
		for (int index = m_aRetiringGroups.Count() - 1; index >= 0; index--)
		{
			AROR_SpecialSiteGroup entry = m_aRetiringGroups[index];
			bool living = AROR_ForceSpawner.CountStrength(entry.m_Group) > 0;
			if (living && (!retiring || !CanRetireGroup(entry.m_Group)))
				continue;
			if (!living && entry.m_Group && !entry.m_bSeen && m_iClock - entry.m_iRaisedAt < 180)
				continue;
			if (entry.Remove_S())
				m_aRetiringGroups.Remove(index);
		}
		for (int air = m_aRetiringHelicopters.Count() - 1; air >= 0; air--)
		{
			IEntity helicopter = m_aRetiringHelicopters[air];
			if (helicopter && AROR_ResistanceFleet.IsClaimed(helicopter))
			{
				m_aRetiringHelicopters.Remove(air);
				continue;
			}
			if (helicopter && (!retiring || AROR_ForceSpawner.IsVehicleInUse(helicopter) || NearestPlayer(helicopter.GetOrigin()) <= m_fRetireDistance))
				continue;
			if (helicopter)
				SCR_EntityHelper.DeleteEntityAndChildren(helicopter);
			m_aRetiringHelicopters.Remove(air);
		}
	}

	protected bool CanRetireGroup(SCR_AIGroup group)
	{
		if (!group)
			return true;
		array<IEntity> members = {};
		AROR_ForceSpawner.CountLiving(group, members);
		foreach (IEntity member : members)
		{
			if (NearestPlayer(member.GetOrigin()) <= m_fRetireDistance)
				return false;
		}
		Faction faction = group.GetFaction();
		return !faction || !AROR_HostilesQuery.HasHostilesInRadius(faction, group.GetOrigin(), 300);
	}

	protected bool HasActivityCapacity(int requested)
	{
		int committed;
		foreach (AROR_SiteLifeComponent site : s_aSites)
		{
			foreach (AROR_SpecialSiteGroup worker : site.m_aWorkers)
				committed += worker.m_iSize;
			foreach (AROR_SpecialSiteGroup patrol : site.m_aPatrols)
				committed += patrol.m_iSize;
			foreach (AROR_SpecialSiteGroup former : site.m_aRetiringGroups)
				committed += former.m_iSize;
			if (site.m_Truck)
				committed += site.m_Truck.m_iSize;
		}
		return committed + requested <= m_iActivityPersonnelBudget && requested <= AROR_PopulationBudget.GetAvailable_S(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Normalize signed hashes without bitwise expressions inside native call arguments.
	protected static int BuildSlot(int seed)
	{
		int slot = Math.Mod(seed, 20);
		if (slot < 0)
			slot += 20;
		return slot;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Location = AROR_LocationComponent.Cast(owner.FindComponent(AROR_LocationComponent));
		if (!GetGame().InPlayMode())
			return;

		if (!m_Location)
		{
			Print("[AROR SITE] Site life needs a location component; disabled", LogLevel.ERROR);
			return;
		}

		// Every machine builds the camp once the world has settled
		int siteSeed = m_Location.GetId().Hash();
		int buildSlot = BuildSlot(siteSeed);
		GetGame().GetCallqueue().CallLater(Build, 1000 + buildSlot * 500);
		if (!Replication.IsServer())
			return;
		s_aSites.Insert(this);

		m_eRole = m_Location.GetControllingRole();
		GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aSites.RemoveItem(this);
		GetGame().GetCallqueue().Remove(Build);
		GetGame().GetCallqueue().Remove(Tick);
		if (Replication.IsServer() && m_bActive)
			Retire();
		foreach (IEntity dressing : m_aDressing)
		{
			if (dressing)
				SCR_EntityHelper.DeleteEntityAndChildren(dressing);
		}
		m_aDressing.Clear();
		super.OnDelete(owner);
	}
}

//! Soldiers at a site: a work detail walking between buildings or a fire team walking the perimeter
class AROR_SpecialSiteGroup
{
	SCR_AIGroup m_Group;
	int m_iSize;
	int m_iNextWake;
	AIWaypoint m_Waypoint;
	int m_iRaisedAt;
	int m_iPoint;
	int m_iDirection = 1;
	int m_iNextOrderAt;
	int m_iLingerUntil;
	bool m_bSeen;	//!< Its members have spawned; from then on none living means wiped out
	vector m_vGoal;

	void Order_S(ResourceName prefab, vector goal, float radius)
	{
		if (!m_Group)
			return;

		if (m_Waypoint)
		{
			m_Group.RemoveWaypoint(m_Waypoint);
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		}

		m_Waypoint = AROR_ForceSpawner.AddWaypoint(m_Group, prefab, goal, radius);
	}

	bool Remove_S()
	{
		if (!AROR_ForceSpawner.RemoveGroup_S(m_Group))
			return false;
		if (m_Waypoint)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Waypoint);
		m_Group = null;
		m_Waypoint = null;
		return true;
	}
}

//! The truck driving laps round a site
class AROR_SpecialSiteTruck : AROR_SpecialSiteGroup
{
	IEntity m_Vehicle;
	bool m_bSeated;

	override bool Remove_S()
	{
		if (!super.Remove_S())
			return false;
		if (m_Vehicle && !AROR_ForceSpawner.IsVehicleInUse(m_Vehicle))
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
		m_Vehicle = null;
		return true;
	}
}
