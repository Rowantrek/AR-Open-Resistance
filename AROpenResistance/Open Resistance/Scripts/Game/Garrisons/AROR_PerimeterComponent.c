[ComponentEditorProps(category: "OpenResistance/Garrisons", description: "Hardens a military compound at play start: a square of high sandbag walls with a gate front and back, barbed wire outside, hedgehogs across the gates and guard towers on two corners. Each piece is snapped to the terrain and skipped where trees, buildings, props, roads or water are in the way. Server-spawned; the props replicate themselves. Put on a military location (its centre and heading set the square).")]
class AROR_PerimeterComponentClass : ScriptComponentClass {}

//! Server only. Built once per session a few seconds after play starts and left standing whoever holds the site
//! (a captured outpost keeps its walls). Nothing is saved: the same pieces are raised again on the next start.
class AROR_PerimeterComponent : ScriptComponent
{
	[Attribute("22", desc: "Half size of the wall square around the location's centre, metres; keep it outside the compound's own defences", params: "8 60 0.5")]
	protected float m_fHalfExtent;
	[Attribute("10", desc: "Gate width in the middle of the front and back walls (the location's heading), metres; 0 = no gates", params: "0 20 0.5")]
	protected float m_fGateWidth;
	[Attribute("{BE16EE8FAA315FE2}Prefabs/Props/Military/Sandbags/Sandbag_01_long_high_burlap.et", UIWidgets.ResourceNamePicker, desc: "Wall piece, long along its own X axis", params: "et")]
	protected ResourceName m_sWallPrefab;
	[Attribute("2.5", desc: "Distance between wall pieces along a wall, metres", params: "1 6 0.1")]
	protected float m_fWallSpacing;
	[Attribute("{BE7B5BAFE1A16A50}Prefabs/Props/Military/Fortification/BarbedTape_Coil.et", UIWidgets.ResourceNamePicker, desc: "Wire piece, long along its own X axis; empty for no wire", params: "et")]
	protected ResourceName m_sWirePrefab;
	[Attribute("7", desc: "Wire distance outside the walls, metres", params: "2 20 0.5")]
	protected float m_fWireOffset;
	[Attribute("5", desc: "Distance between wire pieces, metres", params: "2 10 0.5")]
	protected float m_fWireSpacing;
	[Attribute("{AE28D47CF26E91EA}Prefabs/Props/Military/Fortification/CzechHedgehog_01_rusty.et", UIWidgets.ResourceNamePicker, desc: "Vehicle obstacle placed across each gate (infantry still pass); empty for none", params: "et")]
	protected ResourceName m_sObstaclePrefab;
	[Attribute("{AB87B392A989CA95}Prefabs/Compositions/Slotted/SlotFlatSmall/GuardTower_S_US_01.et", UIWidgets.ResourceNamePicker, desc: "Corner tower while the Occupiers (or the Resistance) hold the site; empty for none", params: "et")]
	protected ResourceName m_sOccupierTower;
	[Attribute("{DFBF655559915333}Prefabs/Compositions/Slotted/SlotFlatSmall/GuardTower_S_USSR_01.et", UIWidgets.ResourceNamePicker, desc: "Corner tower while the Invaders hold the site; empty for none", params: "et")]
	protected ResourceName m_sInvaderTower;
	[Attribute("1.2", desc: "Largest height difference allowed along one wall or wire piece, metres", params: "0.2 4 0.1")]
	protected float m_fMaxRelief;

	protected static const float WALL_CLEARANCE = 0.9;	//!< Half size of the space a wall piece needs free of entities
	protected static const float TOWER_CLEARANCE = 2.2;
	protected static const float OBSTACLE_CLEARANCE = 1;
	protected static const float ROAD_MARGIN = 0.5;
	protected static const float TOWER_INSET = 3.5;	//!< Towers stand this far inside the corner

	protected ref array<IEntity> m_aPieces = {};
	protected int m_iSkipped;
	protected bool m_bRaised;

	//------------------------------------------------------------------------------------------------
	protected void Raise()
	{
		if (!Replication.IsServer() || m_bRaised)
			return;
		m_bRaised = true;
		IEntity owner = GetOwner();
		vector center = owner.GetOrigin();
		vector angles = owner.GetYawPitchRoll();
		float yaw = angles[0];
		vector heading[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), heading);
		vector right = heading[0];
		vector forward = heading[2];

		// Four walls, gates in the front and back walls; the wire follows the same square further out
		float wire = m_fHalfExtent + m_fWireOffset;
		for (int side = 0; side < 4; side++)
		{
			vector normal = forward;
			vector along = right;
			float sideYaw = yaw;
			bool gated = true;
			if (side == 1)
				normal = -forward;
			else if (side == 2)
			{
				normal = right;
				along = forward;
				sideYaw = yaw + 90;
				gated = false;
			}
			else if (side == 3)
			{
				normal = -right;
				along = forward;
				sideYaw = yaw + 90;
				gated = false;
			}
			BuildLine(m_sWallPrefab, center + normal * m_fHalfExtent, along, m_fHalfExtent, m_fWallSpacing, sideYaw, gated, WALL_CLEARANCE);
			if (!m_sWirePrefab.IsEmpty())
				BuildLine(m_sWirePrefab, center + normal * wire, along, wire, m_fWireSpacing, sideYaw, gated, WALL_CLEARANCE);
			// Hedgehogs across each gate, just outside it: vehicles stop, infantry walk through
			if (gated && m_fGateWidth > 0 && !m_sObstaclePrefab.IsEmpty())
			{
				for (int obstacle = -1; obstacle <= 1; obstacle++)
					Place(m_sObstaclePrefab, center + normal * (m_fHalfExtent + 3) + along * (obstacle * m_fGateWidth * 0.3), sideYaw + 45, OBSTACLE_CLEARANCE, 0);
			}
		}

		// Guard towers on two opposite corners, inside the walls
		ResourceName tower = m_sOccupierTower;
		AROR_LocationComponent location = AROR_LocationComponent.Cast(owner.FindComponent(AROR_LocationComponent));
		if (location && location.GetControllingRole() == AROR_EFactionRole.INVADERS)
			tower = m_sInvaderTower;
		if (!tower.IsEmpty())
		{
			float corner = m_fHalfExtent - TOWER_INSET;
			Place(tower, center + forward * corner + right * corner, yaw + 45, TOWER_CLEARANCE, 0);
			Place(tower, center - forward * corner - right * corner, yaw + 225, TOWER_CLEARANCE, 0);
		}

		string id = "?";
		if (location)
			id = location.GetId();
		Print(string.Format("[AROR GARRISON] %1 perimeter raised: %2 pieces, %3 skipped (trees, buildings, props, roads, water or slope)", id, m_aPieces.Count(), m_iSkipped), LogLevel.NORMAL);
	}

	//! Pieces along one side, from -half to +half of the along axis, leaving the gate gap in the middle
	protected void BuildLine(ResourceName prefab, vector middle, vector along, float half, float spacing, float yaw, bool gated, float clearance)
	{
		if (prefab.IsEmpty() || spacing <= 0)
			return;
		int count = Math.Floor(half * 2 / spacing);
		for (int index = 0; index <= count; index++)
		{
			float offset = -half + index * spacing;
			if (gated && m_fGateWidth > 0 && Math.AbsFloat(offset) < m_fGateWidth * 0.5)
				continue;
			Place(prefab, middle + along * offset, yaw, clearance, spacing * 0.5);
		}
	}

	//! One piece on the terrain; skipped where it would sit in water, on a road, on a steep step or in anything
	//! already standing there. reach: half length for the slope check along the piece (0 = none).
	protected void Place(ResourceName prefab, vector point, float yaw, float clearance, float reach)
	{
		BaseWorld world = GetGame().GetWorld();
		float height = world.GetSurfaceY(point[0], point[2]);
		if (height <= world.GetOceanHeight(point[0], point[2]) + 0.3 || AROR_ForceSpawner.IsNearRoad(point, ROAD_MARGIN))
		{
			m_iSkipped++;
			return;
		}
		if (reach > 0)
		{
			vector heading[4];
			Math3D.AnglesToMatrix(Vector(yaw, 0, 0), heading);
			vector a = point + heading[0] * reach;
			vector b = point - heading[0] * reach;
			if (Math.AbsFloat(world.GetSurfaceY(a[0], a[2]) - world.GetSurfaceY(b[0], b[2])) > m_fMaxRelief)
			{
				m_iSkipped++;
				return;
			}
		}
		point[1] = height;
		if (!AROR_ForceSpawner.IsSpotClear(point, clearance, 2))
		{
			m_iSkipped++;
			return;
		}
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			m_iSkipped++;
			return;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), params.Transform);
		params.Transform[3] = point;
		IEntity piece = GetGame().SpawnEntityPrefab(resource, world, params);
		if (!piece)
		{
			m_iSkipped++;
			return;
		}
		m_aPieces.Insert(piece);
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
			aiWorld.RequestNavmeshRebuildEntity(piece);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;
		// After the location, its compound and the AI world are up
		GetGame().GetCallqueue().CallLater(Raise, 3000, false);
	}

	override void OnDelete(IEntity owner)
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(Raise);
		if (GetGame() && Replication.IsServer())
		{
			foreach (IEntity piece : m_aPieces)
			{
				if (piece)
					SCR_EntityHelper.DeleteEntityAndChildren(piece);
			}
		}
		m_aPieces.Clear();
		super.OnDelete(owner);
	}
}
