[ComponentEditorProps(category: "OpenResistance/Field", description: "Personal, squad and Resistance map annotations; server-owned, owner-filtered snapshots.")]
class AROR_PlayerMarkersComponentClass : SCR_BaseGameModeComponentClass {}

class AROR_PlayerMarkersComponent : SCR_BaseGameModeComponent
{
	[Attribute("32", desc: "Maximum markers per player", params: "1 64 1")]
	protected int m_iPerPlayerLimit;
	[Attribute("256", desc: "Maximum markers in the campaign", params: "1 512 1")]
	protected int m_iCampaignLimit;
	protected static AROR_PlayerMarkersComponent s_Instance;
	protected ref array<ref AROR_PlayerMarker> m_aMarkers = {};
	protected int m_iNextId = 1;

	static AROR_PlayerMarkersComponent GetInstance() { return s_Instance; }

	//! id=0 creates; positive updates; negative deletes. Only the author can edit or delete.
	void Request_S(int playerId, int id, vector position, string caption, int scope, int kind, int lifetime)
	{
		if (!Replication.IsServer() || !AROR_FieldAccess.Player(playerId))
			return;
		string key = AROR_FieldAccess.PlayerKey(playerId);
		AROR_PlayerMarker marker;
		foreach (AROR_PlayerMarker existing : m_aMarkers)
		{
			if (existing.m_iId == id || existing.m_iId == -id)
				marker = existing;
		}
		if (id != 0 && (!marker || marker.m_sOwnerKey != key))
			return;
		if (id < 0)
		{
			m_aMarkers.RemoveItem(marker);
			Publish_S();
			return;
		}
		if (scope < 0 || scope > 2 || kind < 0 || kind > 4 || lifetime < 0 || lifetime > 86400 || !AROR_FieldAccess.MapPosition(position))
			return;
		SCR_AIGroup squad;
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (groups)
			squad = groups.GetPlayerGroup(playerId);
		if (scope == AROR_EMarkerScope.SQUAD && !squad)
		{
			AROR_NotificationManagerComponent.SendHint_S(playerId, "Join a player squad before sharing squad markers.");
			return;
		}
		caption = AROR_FieldAccess.Caption(caption);
		if (caption.IsEmpty())
			caption = "Note";
		if (!marker)
		{
			int count;
			foreach (AROR_PlayerMarker existing : m_aMarkers)
			{
				if (existing.m_sOwnerKey == key)
					count++;
			}
			if (count >= m_iPerPlayerLimit || m_aMarkers.Count() >= m_iCampaignLimit)
			{
				AROR_NotificationManagerComponent.SendHint_S(playerId, "Marker limit reached. Remove an old marker first.");
				return;
			}
			marker = new AROR_PlayerMarker();
			marker.m_iId = m_iNextId++;
			marker.m_sOwnerKey = key;
			m_aMarkers.Insert(marker);
		}
		marker.m_iOwnerId = playerId;
		marker.m_Squad = squad;
		marker.m_iScope = scope;
		marker.m_iKind = kind;
		marker.m_sCaption = caption;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		marker.m_vPosition = position;
		marker.m_iExpiresAt = 0;
		if (lifetime > 0)
			marker.m_iExpiresAt = GetGame().GetWorld().GetWorldTime() + lifetime * 1000;
		Publish_S();
	}

	protected string Snapshot_S(int playerId)
	{
		string result;
		string key = AROR_FieldAccess.PlayerKey(playerId);
		SCR_AIGroup squad;
		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (groups)
			squad = groups.GetPlayerGroup(playerId);
		foreach (AROR_PlayerMarker marker : m_aMarkers)
		{
			bool own = marker.m_sOwnerKey == key;
			if (!own && marker.m_iScope != AROR_EMarkerScope.FACTION
				&& !(marker.m_iScope == AROR_EMarkerScope.SQUAD && squad && squad == marker.m_Squad))
				continue;
			int editable;
			if (own)
				editable = 1;
			result += string.Format("%1|%2|%3|%4|%5|%6|%7;", marker.m_iId, editable, marker.m_iScope,
				marker.m_iKind, marker.m_vPosition[0], marker.m_vPosition[2], marker.m_sCaption);
		}
		return result;
	}

	protected void Publish_S()
	{
		if (!Replication.IsServer())
			return;
		int now = GetGame().GetWorld().GetWorldTime();
		for (int i = m_aMarkers.Count() - 1; i >= 0; i--)
		{
			if (m_aMarkers[i].m_iExpiresAt > 0 && now >= m_aMarkers[i].m_iExpiresAt)
				m_aMarkers.Remove(i);
		}
		array<int> players = {};
		PlayerManager manager = GetGame().GetPlayerManager();
		manager.GetPlayers(players);
		AROR_RecruitmentComponent recruitment = AROR_RecruitmentComponent.GetInstance();
		foreach (int playerId : players)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(manager.GetPlayerController(playerId));
			if (!controller)
				continue;
			string markers, recruits;
			// Dead Resistance players retain their map; staging and non-Resistance players get empty data.
			IEntity player = manager.GetPlayerControlledEntity(playerId);
			FactionAffiliationComponent affiliation;
			if (player)
				affiliation = FactionAffiliationComponent.Cast(player.FindComponent(FactionAffiliationComponent));
			AROR_GameFlowComponent flow = AROR_GameFlowComponent.GetInstance();
			if (flow && flow.IsPlaythroughReady() && affiliation && affiliation.GetAffiliatedFactionKey() == AROR_ForceSpawner.FactionKey(AROR_EFactionRole.RESISTANCE))
			{
				markers = Snapshot_S(playerId);
				if (recruitment)
					recruits = recruitment.Snapshot_S(playerId);
			}
			controller.AROR_SendFieldState_S(markers, recruits);
		}
	}

	//! Squad annotations belong to the current native group and are session-only. Other scopes survive saves.
	void GetSavedRows(notnull array<string> rows)
	{
		rows.Clear();
		int now = GetGame().GetWorld().GetWorldTime();
		foreach (AROR_PlayerMarker marker : m_aMarkers)
		{
			if (marker.m_iScope == AROR_EMarkerScope.SQUAD || (marker.m_iExpiresAt > 0 && marker.m_iExpiresAt <= now))
				continue;
			int remaining;
			if (marker.m_iExpiresAt > 0)
				remaining = Math.Max(1, (marker.m_iExpiresAt - now) / 1000);
			rows.Insert(string.Format("%1|%2|%3|%4|%5|%6|%7|%8", marker.m_iId, marker.m_sOwnerKey,
				marker.m_iScope, marker.m_iKind, marker.m_vPosition[0], marker.m_vPosition[2], marker.m_sCaption, remaining));
		}
	}

	void RestoreRows_S(array<string> rows)
	{
		if (!Replication.IsServer())
			return;
		m_aMarkers.Clear();
		m_iNextId = 1;
		foreach (string row : rows)
		{
			if (m_aMarkers.Count() >= m_iCampaignLimit)
				break;
			array<string> fields = {};
			row.Split("|", fields, false);
			if (fields.Count() != 8)
				continue;
			AROR_PlayerMarker marker = new AROR_PlayerMarker();
			marker.m_iId = fields[0].ToInt();
			marker.m_sOwnerKey = fields[1];
			marker.m_iScope = fields[2].ToInt();
			marker.m_iKind = fields[3].ToInt();
			marker.m_vPosition = Vector(fields[4].ToFloat(), 0, fields[5].ToFloat());
			marker.m_sCaption = AROR_FieldAccess.Caption(fields[6]);
			int remaining = fields[7].ToInt();
			if (marker.m_iId < 1 || marker.m_sOwnerKey.IsEmpty() || (marker.m_iScope != 0 && marker.m_iScope != 2)
				|| marker.m_iKind < 0 || marker.m_iKind > 4 || remaining < 0 || remaining > 86400 || !AROR_FieldAccess.MapPosition(marker.m_vPosition))
				continue;
			int ownerCount;
			bool duplicate;
			foreach (AROR_PlayerMarker restored : m_aMarkers)
			{
				if (restored.m_iId == marker.m_iId) duplicate = true;
				if (restored.m_sOwnerKey == marker.m_sOwnerKey) ownerCount++;
			}
			if (duplicate || ownerCount >= m_iPerPlayerLimit)
				continue;
			if (remaining > 0)
				marker.m_iExpiresAt = GetGame().GetWorld().GetWorldTime() + remaining * 1000;
			m_aMarkers.Insert(marker);
			m_iNextId = Math.Max(m_iNextId, marker.m_iId + 1);
		}
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		s_Instance = this;
		AROR_FieldClientState.Clear();
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(Publish_S, 2000, true);
	}

	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Publish_S);
		if (s_Instance == this)
		{
			s_Instance = null;
			AROR_FieldClientState.Clear();
		}
		super.OnDelete(owner);
	}
}
