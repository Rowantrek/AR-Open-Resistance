[ComponentEditorProps(category: "OpenResistance/Leaderboard", description: "Campaign kill leaderboard of Resistance players. Attach to the game mode entity.")]
class AROR_LeaderboardComponentClass : SCR_BaseGameModeComponentClass
{
}

//! Leaderboard (see ARCHITECTURE.md -> Leaderboard): campaign kills of enemy (Occupier and Invader) soldiers and
//! vehicles by Resistance players, split into AROR_ELeaderboardCategory, plus assists: kills made by AI the player
//! commands (their HICOMM squads and the AI in their native squad). Scored like Antistasi's player score: each kill
//! is worth points by what it took out, an assist a share of that.
//! Server: every character and vehicle carries an AROR_KillCredit (modded damage managers) that reports its hits here.
//! The last hostile hit decides the credit; a watch list checks those victims for death once a second, so a soldier
//! who bleeds out or a vehicle that burns out still counts. Records are keyed by identity and saved
//! (AROR_LeaderboardComponentSerializer). A sorted, compact board replicates to every client for the phone app.
class AROR_LeaderboardComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", desc: "Points for killing an enemy soldier", params: "0 100 1", category: "Points")]
	protected int m_iInfantryPoints;

	[Attribute("3", desc: "Points for destroying a light vehicle (cars, trucks, boats, armed cars such as an M2 Humvee)", params: "0 100 1", category: "Points")]
	protected int m_iLightVehiclePoints;

	[Attribute("6", desc: "Points for destroying an APC, IFV or armoured car (BTR, BMP, Bradley, JLTV)", params: "0 100 1", category: "Points")]
	protected int m_iApcIfvPoints;

	[Attribute("10", desc: "Points for destroying a tank", params: "0 100 1", category: "Points")]
	protected int m_iArmourPoints;

	[Attribute("8", desc: "Points for destroying a helicopter or plane", params: "0 100 1", category: "Points")]
	protected int m_iAircraftPoints;

	[Attribute("0.5", desc: "Share of a kill's points an assist earns (a kill by AI the player commands)", params: "0 1 0.05", category: "Points")]
	protected float m_fAssistShare;

	[Attribute("90", desc: "A death counts for whoever hit last within this many seconds (bleeding out, burning out)", params: "5 600 1")]
	protected float m_fCreditWindow;

	[Attribute("", desc: "Vehicle prefab rules checked before the preset type (built-in: JLTV, M-ATV, MRAP, Cougar, MaxxPro, BRDM count as APC / IFV)")]
	protected ref array<ref AROR_LeaderboardVehicleRule> m_aVehicleRules;

	[Attribute("40", desc: "Players replicated to clients (best scores first; online players are always included)", params: "5 200 1")]
	protected int m_iMaxRows;

	[Attribute("1", desc: "Hint the player when one of their own kills destroys a vehicle")]
	protected bool m_bVehicleKillHints;

	protected static const string DEFAULT_APC_MATCHES = "jltv,m-atv,matv,mrap,cougar,maxxpro,brdm";
	protected static const float CHECK_INTERVAL_MS = 1000;
	protected static const float PUBLISH_DELAY_MS = 2000;
	protected static const float COMMANDER_CACHE_MS = 2000;

	//! Server: the commander last resolved for an AI group (see GetCommander)
	protected SCR_AIGroup m_CachedGroup;
	protected int m_iCachedCommander;
	protected float m_fCachedAt;

	protected static AROR_LeaderboardComponent s_Instance;

	//! Server: player key (identity ID) -> record
	protected ref map<string, ref AROR_LeaderboardEntry> m_mEntries = new map<string, ref AROR_LeaderboardEntry>();

	//! Server: credits with a recent hostile hit, waiting to see whether the victim dies
	protected ref array<AROR_KillCredit> m_aPending = {};
	protected bool m_bPublishQueued;

	//! Lower-cased rule matches (designer rules first, then the built-in ones) and their categories
	protected ref array<string> m_aRuleMatches = {};
	protected ref array<int> m_aRuleCategories = {};

	//! "playerId|name|score|k0,k1,k2,k3,k4|assists|assistPoints;" per row, best score first
	[RplProp(onRplName: "OnBoardReplicated")]
	protected string m_sBoard;
	protected ref array<ref AROR_LeaderboardRow> m_aRows = {};

	//! Client: the drop-down scoreboard, created the first time its key is pressed
	protected static const string INPUT_CONTEXT = "AROR_Context";
	protected static const string INPUT_ACTION = "AROR_ToggleLeaderboard";
	protected static const string INPUT_ACTION_ALT = "AROR_ToggleLeaderboardAlt";
	protected ref AROR_LeaderboardOverlay m_Overlay;
	protected bool m_bToggleDown;
	protected bool m_bToggleAltDown;
	protected int m_iLastFrameMs;

	//------------------------------------------------------------------------------------------------
	static AROR_LeaderboardComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// Queries (every machine)
	//------------------------------------------------------------------------------------------------
	//! The replicated board, best score first
	array<ref AROR_LeaderboardRow> GetRows()
	{
		return m_aRows;
	}

	//------------------------------------------------------------------------------------------------
	//! Points for one kill of the AROR_ELeaderboardCategory
	int GetPoints(int category)
	{
		switch (category)
		{
			case AROR_ELeaderboardCategory.INFANTRY:
				return m_iInfantryPoints;
			case AROR_ELeaderboardCategory.LIGHT_VEHICLE:
				return m_iLightVehiclePoints;
			case AROR_ELeaderboardCategory.APC_IFV:
				return m_iApcIfvPoints;
			case AROR_ELeaderboardCategory.ARMOUR:
				return m_iArmourPoints;
			case AROR_ELeaderboardCategory.AIRCRAFT:
				return m_iAircraftPoints;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	float GetAssistShare()
	{
		return m_fAssistShare;
	}

	//------------------------------------------------------------------------------------------------
	static string GetCategoryName(int category)
	{
		switch (category)
		{
			case AROR_ELeaderboardCategory.INFANTRY:
				return "Infantry";
			case AROR_ELeaderboardCategory.LIGHT_VEHICLE:
				return "Light vehicles";
			case AROR_ELeaderboardCategory.APC_IFV:
				return "APC / IFV";
			case AROR_ELeaderboardCategory.ARMOUR:
				return "Armour";
			case AROR_ELeaderboardCategory.AIRCRAFT:
				return "Aircraft";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static int GetCategoryCount()
	{
		return 5;
	}

	//------------------------------------------------------------------------------------------------
	// Kill credit (server)
	//------------------------------------------------------------------------------------------------
	//! A character or vehicle carrying the credit took damage
	void OnHit_S(notnull AROR_KillCredit credit, BaseDamageContext context)
	{
		if (!Replication.IsServer() || !context || context.damageValue <= 0 || !context.instigator)
			return;

		if (context.damageType == EDamageType.HEALING || context.damageType == EDamageType.REGENERATION)
			return;

		Instigator instigator = context.instigator;
		if (instigator.GetInstigatorType() == InstigatorType.INSTIGATOR_GM)
			return;

		// Judged at hit time: a body or an abandoned wreck can lose its faction afterwards
		IEntity victim = credit.GetOwner();
		if (!victim || !IsEnemy(victim))
			return;

		int playerId = instigator.GetInstigatorPlayerID();
		IEntity attacker = instigator.GetInstigatorEntity();
		if (playerId <= 0 && attacker)
			playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(attacker);

		// The last creditable hit earns the death. Hits nobody can be credited for (enemy AI, Resistance AI nobody
		// commands, Occupiers against Invaders, fire, bleeding, crashes) leave an earlier player's credit in place;
		// the credit window still bounds how old that hit may be.
		if (playerId > 0 || (attacker && attacker != victim))
		{
			bool assist;
			int credited = ResolveCredit(playerId, attacker, assist);
			if (credited > 0)
			{
				credit.m_iPlayerId = credited;
				credit.m_bAssist = assist;
				credit.m_fHitTime = GetNow();
			}
		}

		// Nothing to watch until someone creditable has hit it
		if (credit.m_iPlayerId <= 0)
			return;

		if (!credit.m_bPending)
		{
			credit.m_bPending = true;
			m_aPending.Insert(credit);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The player credited for a hit, or 0: the shooting player, else the player commanding the AI that hit
	int ResolveInstigator_S(Instigator instigator)
	{
		if (!Replication.IsServer() || !instigator)
			return 0;
		bool assist;
		return ResolveCredit(instigator.GetInstigatorPlayerID(), instigator.GetInstigatorEntity(), assist);
	}

	protected int ResolveCredit(int playerId, IEntity attacker, out bool assist)
	{
		assist = false;
		if (playerId > 0)
			return playerId;

		ChimeraCharacter character = ChimeraCharacter.Cast(attacker);
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		if (!character || !roles || SCR_Faction.GetEntityFaction(character) != roles.GetFaction(AROR_EFactionRole.RESISTANCE))
			return 0;

		int commander = GetCommander(GetCharacterGroup(character));
		assist = commander > 0;
		return commander;
	}

	//------------------------------------------------------------------------------------------------
	//! The player commanding an AI group: a HICOMM squad's leader, or the leader of the native squad the AI is in.
	//! A firefight sends many hits from the same group in a row, so the last answer is reused for a moment.
	protected int GetCommander(AIGroup group)
	{
		SCR_AIGroup aiGroup = SCR_AIGroup.Cast(group);
		if (!aiGroup)
			return 0;

		float now = GetNow();
		if (aiGroup == m_CachedGroup && now - m_fCachedAt < COMMANDER_CACHE_MS)
			return m_iCachedCommander;

		m_CachedGroup = aiGroup;
		m_fCachedAt = now;
		m_iCachedCommander = FindCommander(aiGroup);
		return m_iCachedCommander;
	}

	//------------------------------------------------------------------------------------------------
	protected int FindCommander(notnull SCR_AIGroup aiGroup)
	{
		AROR_HighCommandComponent highCommand = AROR_HighCommandComponent.GetInstance();
		if (highCommand)
		{
			int owner = highCommand.GetSquadCommander_S(aiGroup);
			if (owner > 0)
				return owner;
		}

		SCR_GroupsManagerComponent groups = SCR_GroupsManagerComponent.GetInstance();
		if (!groups)
			return 0;

		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int player : players)
		{
			SCR_AIGroup playerGroup = groups.GetPlayerGroup(player);
			if (playerGroup && (playerGroup == aiGroup || playerGroup.GetSlave() == aiGroup))
				return playerGroup.GetLeaderID();
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static AIGroup GetCharacterGroup(IEntity character)
	{
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (!control)
			return null;

		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
			return null;

		return agent.GetParentGroup();
	}

	//------------------------------------------------------------------------------------------------
	//! Affiliated with any faction hostile to the Resistance: the Occupiers and Invaders, but also base-game factions
	//! (e.g. US soldiers placed with Game Master). Civilians and the Resistance's own never count.
	protected static bool IsEnemy(IEntity entity)
	{
		AROR_FactionRolesComponent roles = AROR_FactionRolesComponent.GetInstance();
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (!roles || !affiliation)
			return false;

		Faction faction = affiliation.GetAffiliatedFaction();
		Faction resistance = roles.GetFaction(AROR_EFactionRole.RESISTANCE);
		return faction && resistance && faction != resistance && resistance.IsFactionEnemy(faction);
	}

	//------------------------------------------------------------------------------------------------
	protected static float GetNow()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Once a second: count the watched victims that died, drop the ones whose last hit is too old
	protected void CheckPending_S()
	{
		if (m_aPending.IsEmpty())
			return;

		float now = GetNow();
		float window = m_fCreditWindow * 1000;
		for (int i = m_aPending.Count() - 1; i >= 0; i--)
		{
			AROR_KillCredit credit = m_aPending[i];
			if (!credit || !credit.GetOwner() || credit.m_bResolved)
			{
				m_aPending.Remove(i);
				continue;
			}

			if (credit.IsDead())
			{
				m_aPending.Remove(i);
				ResolveDeath_S(credit, now);
				continue;
			}

			if (now - credit.m_fHitTime > window)
			{
				credit.m_bPending = false;
				m_aPending.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A watched victim is being deleted (Game Master, cleanup, despawn): settle its death now, before the sweep
	//! would find it gone. The entity still exists during the delete. A victim deleted alive counts for nobody.
	void OnVictimDeleted_S(notnull AROR_KillCredit credit)
	{
		if (!Replication.IsServer() || !credit.m_bPending || credit.m_bResolved || !credit.IsDead())
			return;

		ResolveDeath_S(credit, GetNow());
	}

	//------------------------------------------------------------------------------------------------
	//! The victim died: counts it once for the credited player when their hit is recent enough
	protected void ResolveDeath_S(notnull AROR_KillCredit credit, float now)
	{
		credit.m_bResolved = true;
		credit.m_bPending = false;
		if (credit.m_iPlayerId > 0 && now - credit.m_fHitTime <= m_fCreditWindow * 1000)
			CountKill_S(credit);
	}

	//------------------------------------------------------------------------------------------------
	protected void CountKill_S(notnull AROR_KillCredit credit)
	{
		int category = Classify(credit.GetOwner());
		if (category < 0)
			return;

		AROR_LeaderboardEntry entry = FindOrCreateEntry(credit.m_iPlayerId);
		if (!entry)
			return;

		if (credit.m_bAssist)
			entry.m_aAssists[category] = entry.m_aAssists[category] + 1;
		else
			entry.m_aKills[category] = entry.m_aKills[category] + 1;

		QueuePublish();

		string kind = "kill";
		if (credit.m_bAssist)
			kind = "assist";

		Print(string.Format("[AROR LEADERBOARD] %1: %2 %3 (%4)", entry.m_sName, GetCategoryName(category), kind, SCR_ResourceNameUtils.GetPrefabName(credit.GetOwner())), LogLevel.NORMAL);

		if (m_bVehicleKillHints && !credit.m_bAssist && category != AROR_ELeaderboardCategory.INFANTRY)
			AROR_NotificationManagerComponent.SendHint_S(credit.m_iPlayerId, string.Format("%1 destroyed: +%2 points", GetCategoryName(category), GetPoints(category)));
	}

	//------------------------------------------------------------------------------------------------
	//! Leaderboard category of a dead character or destroyed vehicle; -1 = not counted (static weapons, props)
	int Classify(IEntity victim)
	{
		if (!victim)
			return -1;

		if (ChimeraCharacter.Cast(victim))
			return AROR_ELeaderboardCategory.INFANTRY;

		if (!Vehicle.Cast(victim))
			return -1;

		ResourceName prefab = SCR_ResourceNameUtils.GetPrefabName(victim);
		int ruled = GetRuleCategory(prefab);
		if (ruled >= 0)
			return ruled;

		int type = -1;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets)
			type = presets.GetVehicleType(prefab);

		if (type < 0)
		{
			AROR_ContentPacksComponent packs = AROR_ContentPacksComponent.GetInstance();
			AROR_ContentPackVehicle vehicle;
			if (packs)
				vehicle = AROR_ContentPackVehicle.Cast(packs.FindEntry(prefab));

			if (vehicle)
				type = vehicle.GetTypeValue();
		}

		if (type >= 0)
			return GetCategoryOfType(type);

		if (victim.FindComponent(HelicopterControllerComponent))
			return AROR_ELeaderboardCategory.AIRCRAFT;

		string path = prefab;
		path.ToLower();
		if (path.IndexOf("tracked") >= 0)
			return AROR_ELeaderboardCategory.APC_IFV;

		return AROR_ELeaderboardCategory.LIGHT_VEHICLE;
	}

	//------------------------------------------------------------------------------------------------
	//! The category a vehicle prefab rule gives this prefab, or -1 when no rule matches
	int GetRuleCategory(ResourceName prefab)
	{
		string path = prefab;
		path.ToLower();
		foreach (int r, string ruleMatch : m_aRuleMatches)
		{
			if (path.IndexOf(ruleMatch) >= 0)
				return m_aRuleCategories[r];
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Leaderboard category of a faction preset vehicle type; -1 for static weapons
	static int GetCategoryOfType(int type)
	{
		switch (type)
		{
			case AROR_EVehicleType.TANK:
				return AROR_ELeaderboardCategory.ARMOUR;
			case AROR_EVehicleType.APC:
			case AROR_EVehicleType.IFV:
			case AROR_EVehicleType.AA_VEHICLE:
			case AROR_EVehicleType.RADAR_AA_VEHICLE:
				return AROR_ELeaderboardCategory.APC_IFV;
			case AROR_EVehicleType.HELI_TRANSPORT:
			case AROR_EVehicleType.HELI_ATTACK:
				return AROR_ELeaderboardCategory.AIRCRAFT;
			case AROR_EVehicleType.STATIC_MG_LOW:
			case AROR_EVehicleType.STATIC_MG_HIGH:
			case AROR_EVehicleType.STATIC_AT:
			case AROR_EVehicleType.STATIC_AA:
			case AROR_EVehicleType.STATIC_MORTAR:
			case AROR_EVehicleType.STATIC_ARTILLERY:
				return -1;
		}

		return AROR_ELeaderboardCategory.LIGHT_VEHICLE;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildRules()
	{
		m_aRuleMatches.Clear();
		m_aRuleCategories.Clear();
		if (m_aVehicleRules)
		{
			foreach (AROR_LeaderboardVehicleRule rule : m_aVehicleRules)
			{
				if (!rule || rule.m_sMatch.IsEmpty())
					continue;

				string ruleMatch = rule.m_sMatch;
				ruleMatch.ToLower();
				m_aRuleMatches.Insert(ruleMatch);
				m_aRuleCategories.Insert(rule.m_eCategory);
			}
		}

		array<string> defaults = {};
		DEFAULT_APC_MATCHES.Split(",", defaults, true);
		foreach (string builtIn : defaults)
		{
			m_aRuleMatches.Insert(builtIn);
			m_aRuleCategories.Insert(AROR_ELeaderboardCategory.APC_IFV);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Records (server)
	//------------------------------------------------------------------------------------------------
	protected AROR_LeaderboardEntry FindOrCreateEntry(int playerId)
	{
		string key = GetPlayerKey(playerId);
		if (key.IsEmpty())
			return null;

		AROR_LeaderboardEntry entry;
		if (!m_mEntries.Find(key, entry))
		{
			entry = new AROR_LeaderboardEntry();
			entry.m_sKey = key;
			m_mEntries.Set(key, entry);
		}

		// A kill is never dropped for a missing name (some local sessions); a known name is kept until a real one arrives
		string name = GetDisplayName(playerId);
		if (!name.IsEmpty())
			entry.m_sName = name;
		else if (entry.m_sName.IsEmpty())
			entry.m_sName = string.Format("Player %1", playerId);

		return entry;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetDisplayName(int playerId)
	{
		return GetGame().GetPlayerManager().GetPlayerName(playerId);
	}

	//------------------------------------------------------------------------------------------------
	int GetScore(notnull AROR_LeaderboardEntry entry)
	{
		return GetKillPoints(entry) + GetAssistPoints(entry);
	}

	//------------------------------------------------------------------------------------------------
	protected int GetKillPoints(notnull AROR_LeaderboardEntry entry)
	{
		int points;
		foreach (int category, int count : entry.m_aKills)
		{
			points += count * GetPoints(category);
		}

		return points;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetAssistPoints(notnull AROR_LeaderboardEntry entry)
	{
		float points;
		foreach (int category, int count : entry.m_aAssists)
		{
			points += count * GetPoints(category) * m_fAssistShare;
		}

		return Math.Floor(points);
	}

	//------------------------------------------------------------------------------------------------
	//! Same key as personal money: the Bohemia identity ID, else the player ID (Workbench and local sessions)
	protected static string GetPlayerKey(int playerId)
	{
		if (playerId <= 0)
			return string.Empty;

		BackendApi backend = GetGame().GetBackendApi();
		if (backend)
		{
			string identity = backend.GetPlayerIdentityId(playerId);
			if (!identity.IsEmpty())
				return identity;
		}

		return "player_" + playerId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsEmpty()
	{
		return m_mEntries.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! For the serializer: counts holds every category's kills, then every category's assists, per player
	void GetEntries(notnull array<string> outKeys, notnull array<string> outNames, notnull array<int> outCounts)
	{
		foreach (string key, AROR_LeaderboardEntry entry : m_mEntries)
		{
			outKeys.Insert(key);
			outNames.Insert(entry.m_sName);
			foreach (int kills : entry.m_aKills)
			{
				outCounts.Insert(kills);
			}

			foreach (int assists : entry.m_aAssists)
			{
				outCounts.Insert(assists);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! From a save. countsPerEntry lets a save written with fewer categories load (missing ones start at 0).
	void RestoreEntries_S(notnull array<string> keys, notnull array<string> names, notnull array<int> counts, int countsPerEntry)
	{
		if (!Replication.IsServer() || countsPerEntry < 2)
			return;

		m_mEntries.Clear();
		int categories = countsPerEntry / 2;
		int shared = Math.Min(categories, GetCategoryCount());
		foreach (int i, string key : keys)
		{
			int offset = i * countsPerEntry;
			if (key.IsEmpty() || offset + countsPerEntry > counts.Count())
				continue;

			AROR_LeaderboardEntry entry = new AROR_LeaderboardEntry();
			entry.m_sKey = key;
			if (names.IsIndexValid(i))
				entry.m_sName = names[i];

			for (int c = 0; c < shared; c++)
			{
				entry.m_aKills[c] = counts[offset + c];
				entry.m_aAssists[c] = counts[offset + categories + c];
			}

			m_mEntries.Set(key, entry);
		}

		QueuePublish();
	}

	//------------------------------------------------------------------------------------------------
	// Replication
	//------------------------------------------------------------------------------------------------
	protected void QueuePublish()
	{
		if (m_bPublishQueued || !Replication.IsServer())
			return;

		m_bPublishQueued = true;
		GetGame().GetCallqueue().CallLater(Publish_S, PUBLISH_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void Publish_S()
	{
		m_bPublishQueued = false;

		// Online players by key, so clients can find themselves without seeing identity IDs
		map<string, int> online = new map<string, int>();
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int player : players)
		{
			string playerKey = GetPlayerKey(player);
			if (!playerKey.IsEmpty())
				online.Set(playerKey, player);
		}

		array<AROR_LeaderboardEntry> sorted = {};
		array<int> scores = {};
		foreach (string entryKey, AROR_LeaderboardEntry candidate : m_mEntries)
		{
			int score = GetScore(candidate);
			int at = sorted.Count();
			while (at > 0 && scores[at - 1] < score)
			{
				at--;
			}

			sorted.InsertAt(candidate, at);
			scores.InsertAt(score, at);
		}

		string board;
		foreach (int rank, AROR_LeaderboardEntry entry : sorted)
		{
			// Reset per row: a key that is not online must never keep the previous row's player ID
			int playerId = 0;
			if (!online.Find(entry.m_sKey, playerId))
				playerId = 0;

			if (rank >= m_iMaxRows && playerId <= 0)
				continue;

			string kills = string.Format("%1,%2,%3,%4,%5", entry.m_aKills[0], entry.m_aKills[1], entry.m_aKills[2], entry.m_aKills[3], entry.m_aKills[4]);
			board += string.Format("%1|%2|%3|%4|%5|%6;", playerId, SanitizeName(entry.m_sName), scores[rank], kills, entry.GetAssistTotal(), GetAssistPoints(entry));
		}

		// Everyone online is listed, Arma 3 style: players without a record yet follow with nothing scored
		foreach (int onlineId : players)
		{
			string onlineKey = GetPlayerKey(onlineId);
			if (onlineKey.IsEmpty() || m_mEntries.Contains(onlineKey))
				continue;

			string onlineName = GetDisplayName(onlineId);
			if (onlineName.IsEmpty())
				onlineName = string.Format("Player %1", onlineId);

			board += string.Format("%1|%2|0|0,0,0,0,0|0|0;", onlineId, SanitizeName(onlineName));
		}

		if (board == m_sBoard)
			return;

		m_sBoard = board;
		Replication.BumpMe();
		OnBoardReplicated();
	}

	//------------------------------------------------------------------------------------------------
	//! The board's field and row separators cannot appear in a name (commas are harmless: only the kills field has them)
	protected static string SanitizeName(string name)
	{
		name.Replace("|", "/");
		name.Replace(";", ",");
		return name;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBoardReplicated()
	{
		m_aRows.Clear();
		array<string> lines = {};
		m_sBoard.Split(";", lines, true);
		foreach (string line : lines)
		{
			array<string> fields = {};
			line.Split("|", fields, false);
			if (fields.Count() != 6)
				continue;

			AROR_LeaderboardRow row = new AROR_LeaderboardRow();
			row.m_iPlayerId = fields[0].ToInt();
			row.m_sName = fields[1];
			row.m_iScore = fields[2].ToInt();
			array<string> kills = {};
			fields[3].Split(",", kills, false);
			foreach (int c, string count : kills)
			{
				if (row.m_aKills.IsIndexValid(c))
					row.m_aKills[c] = count.ToInt();
			}

			row.m_iAssists = fields[4].ToInt();
			row.m_iAssistPoints = fields[5].ToInt();
			m_aRows.Insert(row);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Drop-down scoreboard (client)
	//------------------------------------------------------------------------------------------------
	//! Every frame: the keybind (P, or F8) toggles the scoreboard; the overlay slides and redraws itself
	protected void UpdateClient()
	{
		InputManager inputManager = GetGame().GetInputManager();
		inputManager.ActivateContext(INPUT_CONTEXT);

		bool toggleDown = inputManager.GetActionValue(INPUT_ACTION) > 0;
		if (toggleDown != m_bToggleDown)
		{
			m_bToggleDown = toggleDown;
			if (toggleDown)
				ToggleOverlay();
		}

		bool altDown = inputManager.GetActionValue(INPUT_ACTION_ALT) > 0;
		if (altDown != m_bToggleAltDown)
		{
			m_bToggleAltDown = altDown;
			if (altDown)
				ToggleOverlay();
		}

		int now = System.GetTickCount();
		float timeSlice = Math.Clamp((now - m_iLastFrameMs) / 1000.0, 0, 0.1);
		m_iLastFrameMs = now;

		// Closed and fully up out of sight: nothing to slide or draw (it hid itself on the frame it got there)
		if (m_Overlay && !m_Overlay.IsIdle())
			m_Overlay.Update(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	protected void ToggleOverlay()
	{
		if (!m_Overlay)
		{
			m_Overlay = new AROR_LeaderboardOverlay();
			if (!m_Overlay.Init())
			{
				m_Overlay.Shutdown();
				m_Overlay = null;
				Print("[AROR] Leaderboard overlay layout unavailable", LogLevel.WARNING);
				return;
			}
		}

		m_Overlay.Toggle();
	}

	//------------------------------------------------------------------------------------------------
	// Game mode events
	//------------------------------------------------------------------------------------------------
	override void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);
		if (!Replication.IsServer())
			return;

		// Refresh a returning player's name and mark them online
		AROR_LeaderboardEntry entry;
		if (m_mEntries.Find(GetPlayerKey(playerId), entry))
		{
			string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
			if (!name.IsEmpty())
				entry.m_sName = name;
		}

		QueuePublish();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);
		QueuePublish();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;

		s_Instance = this;
		BuildRules();
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(CheckPending_S, CHECK_INTERVAL_MS, true);

		if (RplSession.Mode() != RplMode.Dedicated)
		{
			m_iLastFrameMs = System.GetTickCount();
			GetGame().GetCallqueue().CallLater(UpdateClient, 0, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(CheckPending_S);
		GetGame().GetCallqueue().Remove(Publish_S);
		GetGame().GetCallqueue().Remove(UpdateClient);
		if (m_Overlay)
			m_Overlay.Shutdown();

		m_Overlay = null;
		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}
}
