//! Server-only bounded visual reports shared only with living Resistance players.
//! Does not expose unobserved operation entities through replication or a client query RPC.
class AROR_QRFSpotting
{
	protected ref array<ref AROR_QRFSighting> m_aSightings = {};
	protected int m_iPairCursor;
	protected int m_iNextId;
	protected int m_iNextBroadcast;
	protected bool m_bWasDebug;
	protected ref array<int> m_aMapObserversBlocked = {};
	//! The owned controller can suppress its player's observations while a map obscures the view.
	void SetMapOpen_S(int playerId, bool open)
	{
		if (!Replication.IsServer() || playerId <= 0)
			return;
		if (open && !m_aMapObserversBlocked.Contains(playerId))
			m_aMapObserversBlocked.Insert(playerId);
		else if (!open)
			m_aMapObserversBlocked.RemoveItem(playerId);
	}

	static bool IsResistance(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().IsDead())
			return false;
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		return roles && affiliation && affiliation.GetAffiliatedFactionKey() == roles.GetFactionKey(AROR_EFactionRole.RESISTANCE);
	}
	//! Pure geometry gate; callers still require an unobstructed native visibility trace.
	static bool InView(float distance, float alignment, float range, float minimumDot)
	{
		return distance > 1 && distance <= range && alignment >= minimumDot;
	}
	protected AROR_QRFSighting Find(IEntity entity)
	{
		foreach (AROR_QRFSighting report : m_aSightings)
		{
			if (report.m_Entity == entity)
				return report;
		}
		return null;
	}
	protected void RecordLook(IEntity entity, int kind, int observerId, int now, int lifetime, int side)
	{
		AROR_QRFSighting report = Find(entity);
		if (!report)
		{
			if (m_aSightings.Count() >= 32)
				return;
			report = new AROR_QRFSighting();
			report.m_Entity = entity;
			m_iNextId++;
			report.m_sId = string.Format("qrf_intel_%1", m_iNextId);
			report.m_iFirstLook = now;
			report.m_iObserver = observerId;
			m_aSightings.Insert(report);
		}
		// Require a second observation by the same player, with a finite continuity window.
		if (!report.m_bConfirmed && report.m_iObserver != observerId && now - report.m_iLastLook <= 15)
			return;
		if (!report.m_bConfirmed && (report.m_iObserver != observerId || now - report.m_iLastLook > 15))
		{
			report.m_iObserver = observerId;
			report.m_iFirstLook = now;
		}
		if (report.m_bConfirmed || (now > report.m_iFirstLook && now - report.m_iLastLook <= 15))
		{
			if (!report.m_bConfirmed)
				Print(string.Format("[AROR INTEL] FIA spotted QRF asset %1 kind=%2 observer=%3", report.m_sId, kind, observerId), LogLevel.NORMAL);
			report.m_bConfirmed = true;
			report.m_iLastSeen = now;
			report.m_vPosition = entity.GetOrigin();
			report.m_iKind = kind;
			report.m_iSide = side;
		}
		report.m_iLastLook = now;
	}
	void Tick_S(AROR_OperationExecutorComponent executor, int now, float groundRange, float airRange, float minimumDot, int traceLimit, int lifetime, bool debugMarkers)
	{
		if (!Replication.IsServer() || !executor)
			return;
		if (debugMarkers || m_bWasDebug)
			m_aSightings.Clear();
		m_bWasDebug = debugMarkers;
		for (int index = m_aSightings.Count() - 1; index >= 0; index--)
		{
			AROR_QRFSighting old = m_aSightings[index];
			int last = old.m_iLastLook;
			if (old.m_bConfirmed)
				last = old.m_iLastSeen;
			if (now - last > lifetime)
				m_aSightings.Remove(index);
		}
		array<IEntity> vehicles = {};
		array<int> kinds = {};
		array<int> sides = {};
		executor.GetQRFSpottingAssets_S(vehicles, kinds, sides);
		if (debugMarkers)
		{
			// Rebuild from owned physical vehicles, including retained hulls; no visibility traces.
			// Removed entities vanish from the next snapshot instead of leaving debug ghosts.
			for (int asset = 0; asset < Math.Min(32, vehicles.Count()); asset++)
			{
				if (!vehicles[asset])
					continue;
				AROR_QRFSighting debugReport = new AROR_QRFSighting();
				debugReport.m_sId = string.Format("qrf_debug_%1", asset);
				debugReport.m_bConfirmed = true;
				debugReport.m_vPosition = vehicles[asset].GetOrigin();
				debugReport.m_iKind = kinds[asset];
				debugReport.m_iSide = sides[asset];
				debugReport.m_iLastSeen = now;
				m_aSightings.Insert(debugReport);
			}
		}
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		for (int blocked = m_aMapObserversBlocked.Count() - 1; blocked >= 0; blocked--)
		{
			if (!players.Contains(m_aMapObserversBlocked[blocked]))
				m_aMapObserversBlocked.Remove(blocked);
		}
		int pairs = vehicles.Count() * players.Count();
		if (debugMarkers)
			pairs = 0;
		int traces;
		// A maximum of 64 cheap pair checks and traceLimit native traces per second.
		for (int checked = 0; checked < Math.Min(64, pairs) && traces < traceLimit; checked++)
		{
			if (m_iPairCursor >= pairs)
				m_iPairCursor = 0;
			int vehicleIndex = m_iPairCursor / players.Count();
			int playerIndex = m_iPairCursor - vehicleIndex * players.Count();
			m_iPairCursor++;
			int playerId = players[playerIndex];
			if (m_aMapObserversBlocked.Contains(playerId))
				continue;
			ChimeraCharacter observer = ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
			IEntity vehicle = vehicles[vehicleIndex];
			if (!IsResistance(observer) || !vehicle)
				continue;
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
			if (affiliation && affiliation.GetAffiliatedFaction())
			{
				AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
				Faction resistance = roles.GetFaction(AROR_EFactionRole.RESISTANCE);
				if (!resistance || !resistance.IsFactionEnemy(affiliation.GetAffiliatedFaction()))
					continue;
			}
			DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
			if (damage && damage.IsDestroyed())
				continue;
			CharacterHeadAimingComponent head = observer.GetCharacterController().GetHeadAimingComponent();
			if (!head)
				continue;
			vector eye = observer.EyePosition();
			vector target = vehicle.GetOrigin() + Vector(0, 1.5, 0);
			vector delta = target - eye;
			float distance = delta.Length();
			vector direction = head.GetAimingDirectionWorld();
			direction.Normalize();
			delta.Normalize();
			float range = groundRange;
			if (kinds[vehicleIndex] >= 3)
				range = airRange;
			if (!InView(distance, vector.Dot(direction, delta), range, minimumDot))
				continue;
			TraceParam trace = new TraceParam();
			trace.Start = eye;
			trace.End = target;
			trace.Exclude = observer;
			trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS | TraceFlags.VISIBILITY;
			traces++;
			float fraction = GetGame().GetWorld().TraceMove(trace, null);
			bool visible = fraction >= 0.999;
			IEntity hit = trace.TraceEnt;
			if (hit && (hit == vehicle || hit.GetRootParent() == vehicle))
				visible = true;
			if (visible)
				RecordLook(vehicle, kinds[vehicleIndex], playerId, now, lifetime, sides[vehicleIndex]);
		}
		if (now < m_iNextBroadcast)
			return;
		m_iNextBroadcast = now + 5;
		array<string> ids = {};
		array<vector> positions = {};
		array<int> types = {};
		array<int> ages = {};
		array<int> reportSides = {};
		foreach (AROR_QRFSighting report : m_aSightings)
		{
			if (!report.m_bConfirmed || now - report.m_iLastSeen >= lifetime)
				continue;
			ids.Insert(report.m_sId);
			positions.Insert(report.m_vPosition);
			types.Insert(report.m_iKind);
			ages.Insert(now - report.m_iLastSeen);
			reportSides.Insert(report.m_iSide);
		}
		foreach (int recipientId : players)
		{
			IEntity recipient = GetGame().GetPlayerManager().GetPlayerControlledEntity(recipientId);
			if (!IsResistance(recipient))
				continue;
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(recipientId));
			if (controller)
				controller.AROR_SendQRFIntel_S(ids, positions, types, ages, lifetime, reportSides);
		}
	}
}
