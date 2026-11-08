//! A radio that is currently sounding on this machine
class ORRadio_Sound
{
	int m_iVehicleId;
	int m_iStation;
	int m_iVolume;
	ORRadio_Track m_Track;
	AudioHandle m_Handle = AudioHandle.Invalid;
	IEntity m_Vehicle;
}

[ComponentEditorProps(category: "OpenResistance/Radio", description: "Vehicle radios and the stations they play. Server owns which radios are on; each client plays the nearest ones.")]
class ORRadio_ManagerComponentClass : SCR_BaseGameModeComponentClass {}

//! Vehicle radios (see ARCHITECTURE.md -> Radio). Stations are worked out from a replicated seed and start time (see
//! ORRadio_Schedule) and only the radios that are on are replicated, so vehicles need no component of their own.
//! The controls are actions added to every vehicle by the Vehicle_Base override (see ORRadio_VehicleAction).
class ORRadio_ManagerComponent : SCR_BaseGameModeComponent
{
	protected static const int SWEEP_MS = 5000;
	protected static const int CLIENT_TICK_MS = 100;
	protected static const int SELECT_EVERY_TICKS = 5;
	protected static const float FADE_STOP_SECONDS = 0.4;
	protected static const float FADE_RESTART_SECONDS = 0.12;
	//! A sound this close to its end is left out rather than started for a moment
	protected static const int END_GUARD_MS = 500;
	protected static const int BROADCAST_TYPE_MUSIC = 1;
	//! Entries per radio in m_aRadios
	protected static const int RADIO_STRIDE = 3;
	static const int VOLUME_MAX = 10;

	[Attribute("{6D1A7E2C40000010}Configs/Radio/AROR_RadioStations.conf", UIWidgets.ResourceNamePicker, desc: "Stations config", params: "conf class=ORRadio_Stations")]
	protected ResourceName m_sStationsConfig;

	[Attribute("120", desc: "Metres at which a client starts playing a radio. Match the outer range of the audio project's amplitude curve", params: "10 500 1")]
	protected float m_fAudibleRange;

	[Attribute("3", desc: "Radios a client plays at once, nearest first", params: "1 8 1")]
	protected int m_iMaxSounds;

	[Attribute("15", desc: "Metres another radio must be closer by to replace one already playing, so equal distances do not restart sounds", params: "0 100 1")]
	protected float m_fSwitchMargin;

	[Attribute("0", desc: "Seconds before the radio of an empty vehicle turns off. 0 keeps it on", params: "0 3600 1")]
	protected float m_fEmptyShutoffSeconds;

	[Attribute("7", desc: "Volume a radio turns on at", params: "0 10 1")]
	protected int m_iDefaultVolume;

	[Attribute("2", desc: "Volume response. 1 is linear; higher is gentler at the low end", params: "1 4 0.1")]
	protected float m_fVolumeCurve;

	[Attribute("{6D77C615F1EBB509}UI/Layouts/Radio/ORRadio_NowPlaying.layout", UIWidgets.ResourceNamePicker, desc: "Now playing card", params: "layout")]
	protected ResourceName m_sNowPlayingLayout;

	[Attribute("5", desc: "Seconds the now playing card stays up", params: "1 30 0.5")]
	protected float m_fNowPlayingSeconds;

	protected static ORRadio_ManagerComponent s_Instance;

	[RplProp(onRplName: "OnScheduleReplicated")]
	protected int m_iSeed;

	[RplProp()]
	protected WorldTimestamp m_Epoch;

	//! Radios that are on, as {vehicle RplId, station, volume, ...}
	[RplProp(onRplName: "OnRadiosReplicated")]
	protected ref array<int> m_aRadios = {};

	protected ref ORRadio_Stations m_Stations;
	protected ref array<ref ORRadio_Station> m_aNoStations = {};
	protected ref array<ref ORRadio_Schedule> m_aSchedules = {};
	protected ChimeraWorld m_World;
	protected bool m_bAuthority;

	// Server: when each radio's vehicle was found empty, by radio index; 0 while occupied
	protected ref array<float> m_aEmptySinceMs = {};

	// Client
	protected ref array<ref ORRadio_Sound> m_aSounds = {};
	protected bool m_bSelectionDue;
	protected int m_iTicksToSelect;
	protected ref array<string> m_aReportedFailures = {};
	protected bool m_bReportedFirstSound;
	protected ref ORRadio_NowPlayingCard m_NowPlayingCard;
	protected int m_iCardStation = -1;
	protected bool m_bCardTitlePending;

	//------------------------------------------------------------------------------------------------
	static ORRadio_ManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (s_Instance && s_Instance != this)
		{
			Print("[AROR Radio] A second radio manager is attached. Radio state lives on one instance, so machines can read different ones and radios fall silent. Remove the duplicate game mode component.", LogLevel.ERROR);
			return;
		}

		s_Instance = this;
		m_World = ChimeraWorld.CastFrom(owner.GetWorld());
		LoadStations();

		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		m_bAuthority = !rpl || rpl.IsMaster();
		// The station clock starts with the first radio (StartClock_S), not here
		if (m_bAuthority && m_World)
			GetGame().GetCallqueue().CallLater(Sweep_S, SWEEP_MS, true);

		BuildSchedules();
		Print(string.Format("[AROR Radio] Ready: %1 stations, authority %2, %3", GetStations().Count(), m_bAuthority, typename.EnumToString(RplMode, RplSession.Mode())), LogLevel.NORMAL);

		if (RplSession.Mode() != RplMode.Dedicated)
		{
			foreach (ORRadio_Station station : GetStations())
			{
				if (station && !station.m_sAudioProject.IsEmpty())
					AudioSystem.PlayEventInitialize(station.m_sAudioProject);
			}

			GetGame().GetCallqueue().CallLater(ClientTick, CLIENT_TICK_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Sweep_S);
		GetGame().GetCallqueue().Remove(ClientTick);
		foreach (ORRadio_Sound sound : m_aSounds)
		{
			if (sound.m_Handle != AudioHandle.Invalid)
				AudioSystem.TerminateSound(sound.m_Handle);
		}

		m_aSounds.Clear();
		m_NowPlayingCard = null;
		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void LoadStations()
	{
		Resource resource = Resource.Load(m_sStationsConfig);
		if (!resource || !resource.IsValid())
		{
			Print("[AROR Radio] Stations config not found: " + m_sStationsConfig, LogLevel.WARNING);
			return;
		}

		m_Stations = ORRadio_Stations.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildSchedules()
	{
		m_aSchedules.Clear();
		array<ref ORRadio_Station> stations = GetStations();
		for (int i = 0; i < stations.Count(); i++)
		{
			m_aSchedules.Insert(new ORRadio_Schedule(stations[i], (m_iSeed + i * 7919) & 0x7FFFFFFF));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnScheduleReplicated()
	{
		BuildSchedules();
		m_bSelectionDue = true;
		Print(string.Format("[AROR Radio] Station clock received (seed %1)", m_iSeed), LogLevel.NORMAL);
		RefreshNowPlayingTitle();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadiosReplicated()
	{
		m_bSelectionDue = true;
	}

	//------------------------------------------------------------------------------------------------
	// Queries, every machine
	//------------------------------------------------------------------------------------------------
	array<ref ORRadio_Station> GetStations()
	{
		if (m_Stations && m_Stations.m_aStations)
			return m_Stations.m_aStations;

		return m_aNoStations;
	}

	//------------------------------------------------------------------------------------------------
	//! The station a vehicle's radio is playing, -1 when it is off
	int GetVehicleStation(RplId vehicleId)
	{
		int id = vehicleId;
		int index = FindRadio(id);
		if (index < 0)
			return -1;

		return m_aRadios[index + 1];
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle's radio volume, 0 to VOLUME_MAX; 0 when it is off
	int GetVehicleVolume(RplId vehicleId)
	{
		int id = vehicleId;
		int index = FindRadio(id);
		if (index < 0)
			return 0;

		return m_aRadios[index + 2];
	}

	//------------------------------------------------------------------------------------------------
	//! What a station is playing now; false when it has nothing to play or has not replicated yet
	bool GetNowPlaying(int station, out ORRadio_Track current, out int offsetMs, out ORRadio_Track upcoming)
	{
		if (!m_World || m_iSeed == 0 || !m_aSchedules.IsIndexValid(station))
			return false;

		int elapsedMs = m_World.GetServerTimestamp().DiffMilliseconds(m_Epoch);
		return m_aSchedules[station].Resolve(elapsedMs, current, offsetMs, upcoming);
	}

	//------------------------------------------------------------------------------------------------
	protected int FindRadio(int vehicleId)
	{
		for (int i = 0; i < m_aRadios.Count(); i += RADIO_STRIDE)
		{
			if (m_aRadios[i] == vehicleId)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Null when the vehicle no longer exists, or is not streamed in on this client
	protected IEntity FindVehicle(int vehicleId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicleId));
		if (!rpl)
			return null;

		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	// Server
	//------------------------------------------------------------------------------------------------
	//! A player sets the radio of the vehicle they are sitting in: a station, or off with -1, and a volume, or -1 to
	//! keep the current one (the default for a radio being turned on)
	void SetRadio_S(int playerId, RplId vehicleId, int station, int volume)
	{
		if (!m_bAuthority)
		{
			RejectRequest_S(playerId, "it reached a machine that is not the server");
			return;
		}

		if (station >= GetStations().Count())
		{
			RejectRequest_S(playerId, string.Format("station %1 is not in the stations config", station));
			return;
		}

		int id = vehicleId;
		IEntity vehicle = FindVehicle(id);
		if (!vehicle)
		{
			RejectRequest_S(playerId, "the vehicle was not found");
			return;
		}

		if (!IsSittingIn(playerId, vehicle))
		{
			RejectRequest_S(playerId, "the player is not sitting in the vehicle");
			return;
		}

		if (IsDestroyed(vehicle))
		{
			RejectRequest_S(playerId, "the vehicle is destroyed");
			return;
		}

		if (station >= 0)
			StartClock_S();

		int index = FindRadio(id);
		if (station < 0)
		{
			if (index >= 0)
				RemoveRadio_S(index);
		}
		else if (index >= 0)
		{
			m_aRadios[index + 1] = station;
			if (volume >= 0)
				m_aRadios[index + 2] = Math.ClampInt(volume, 0, VOLUME_MAX);
		}
		else
		{
			if (volume < 0)
				volume = m_iDefaultVolume;

			m_aRadios.Insert(id);
			m_aRadios.Insert(station);
			m_aRadios.Insert(Math.ClampInt(volume, 0, VOLUME_MAX));
			m_aEmptySinceMs.Insert(0);
		}

		Replication.BumpMe();
		m_bSelectionDue = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the station clock (seed and start time) the first time a radio is turned on; it then runs for the rest of
	//! the game. Not set in EOnInit: values the game mode set while the world loaded did not reach clients in multiplayer
	//! (Workbench, being server and client at once, hid it), whereas a change made now goes out in the same update as
	//! the radio that needs it.
	protected void StartClock_S()
	{
		if (m_iSeed != 0 || !m_World)
			return;

		// Mixed with the date and time in case the engine's generator starts from the same state each server run (the
		// first multiplayer test received seed 65535), which would give every game the same order and starting song
		m_iSeed = (Math.RandomInt(1, 0x3FFFFFFF) ^ (System.GetUnixTime() * 1103515245)) & 0x3FFFFFFF;
		if (m_iSeed == 0)
			m_iSeed = 1;

		m_Epoch = m_World.GetServerTimestamp();
		BuildSchedules();
		Print(string.Format("[AROR Radio] Station clock started (seed %1)", m_iSeed), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void RejectRequest_S(int playerId, string reason)
	{
		Print(string.Format("[AROR Radio] Radio request from player %1 rejected: %2", playerId, reason), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsSittingIn(int playerId, IEntity vehicle)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return false;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		return access && access.GetCompartment() && access.GetCompartment().GetVehicle() == vehicle;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsDestroyed(IEntity vehicle)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveRadio_S(int index)
	{
		for (int i = RADIO_STRIDE - 1; i >= 0; i--)
		{
			m_aRadios.RemoveOrdered(index + i);
		}

		m_aEmptySinceMs.RemoveOrdered(index / RADIO_STRIDE);
	}

	//------------------------------------------------------------------------------------------------
	//! Turns off radios whose vehicle is gone, destroyed or has been left empty for long enough
	protected void Sweep_S()
	{
		if (m_aRadios.IsEmpty())
			return;

		float now = m_World.GetServerTimestamp().DiffMilliseconds(m_Epoch);
		bool changed;
		for (int i = m_aRadios.Count() - RADIO_STRIDE; i >= 0; i -= RADIO_STRIDE)
		{
			IEntity vehicle = FindVehicle(m_aRadios[i]);
			bool remove = !vehicle || IsDestroyed(vehicle);
			if (!remove && m_fEmptyShutoffSeconds > 0)
			{
				int radioNumber = i / RADIO_STRIDE;
				if (AROR_GarageSnapshotHelper.HasLivingOccupant(vehicle))
					m_aEmptySinceMs[radioNumber] = 0;
				else if (m_aEmptySinceMs[radioNumber] <= 0)
					m_aEmptySinceMs[radioNumber] = Math.Max(now, 1);
				else
					remove = now - m_aEmptySinceMs[radioNumber] >= m_fEmptyShutoffSeconds * 1000;
			}

			if (remove)
			{
				RemoveRadio_S(i);
				changed = true;
			}
		}

		if (changed)
		{
			Replication.BumpMe();
			m_bSelectionDue = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	// Client: playing the nearest radios
	//------------------------------------------------------------------------------------------------
	protected void ClientTick()
	{
		if (m_aSounds.IsEmpty() && m_aRadios.IsEmpty())
			return;

		m_iTicksToSelect--;
		if (m_bSelectionDue || m_iTicksToSelect <= 0)
		{
			m_bSelectionDue = false;
			m_iTicksToSelect = SELECT_EVERY_TICKS;
			UpdateSounds();
		}

		for (int i = m_aSounds.Count() - 1; i >= 0; i--)
		{
			ORRadio_Sound sound = m_aSounds[i];
			if (!sound.m_Vehicle)
			{
				StopSound(i, FADE_STOP_SECONDS);
				continue;
			}

			vector transform[4];
			sound.m_Vehicle.GetTransform(transform);
			AudioSystem.SetSoundTransformation(sound.m_Handle, transform);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stops what should no longer play, then starts the nearest radios up to the limit
	protected void UpdateSounds()
	{
		IEntity listener = SCR_PlayerController.GetLocalControlledEntity();
		if (!listener)
		{
			StopAllSounds();
			return;
		}

		IEntity listenerRoot = listener.GetRootParent();
		if (listenerRoot)
			listener = listenerRoot;

		vector origin = listener.GetOrigin();
		array<int> nearest = {};
		array<float> distances = {};
		for (int i = 0; i < m_aRadios.Count(); i += RADIO_STRIDE)
		{
			IEntity vehicle = FindVehicle(m_aRadios[i]);
			if (!vehicle)
				continue;

			float distance = vector.Distance(vehicle.GetOrigin(), origin);
			if (distance > m_fAudibleRange)
				continue;

			if (FindSound(m_aRadios[i]) >= 0)
				distance -= m_fSwitchMargin;

			int insertAt = distances.Count();
			for (int d = 0; d < distances.Count(); d++)
			{
				if (distance < distances[d])
				{
					insertAt = d;
					break;
				}
			}

			distances.InsertAt(distance, insertAt);
			nearest.InsertAt(i, insertAt);
		}

		int wanted = Math.Min(m_iMaxSounds, nearest.Count());
		for (int s = m_aSounds.Count() - 1; s >= 0; s--)
		{
			ORRadio_Sound sound = m_aSounds[s];
			bool keep;
			for (int k = 0; k < wanted; k++)
			{
				if (m_aRadios[nearest[k]] == sound.m_iVehicleId)
					keep = true;
			}

			if (!keep)
			{
				StopSound(s, FADE_STOP_SECONDS);
				continue;
			}

			ORRadio_Track current, upcoming;
			int offsetMs;
			int radioIndex = FindRadio(sound.m_iVehicleId);
			// The engine cannot change a playing sound's signals, so a new volume restarts it in place
			if (radioIndex < 0 || m_aRadios[radioIndex + 1] != sound.m_iStation || m_aRadios[radioIndex + 2] != sound.m_iVolume || !GetNowPlaying(sound.m_iStation, current, offsetMs, upcoming) || current != sound.m_Track)
				StopSound(s, FADE_RESTART_SECONDS);
		}

		for (int n = 0; n < wanted; n++)
		{
			int radio = nearest[n];
			if (FindSound(m_aRadios[radio]) < 0)
				StartSound(m_aRadios[radio], m_aRadios[radio + 1], m_aRadios[radio + 2]);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StartSound(int vehicleId, int station, int volume)
	{
		array<ref ORRadio_Station> stations = GetStations();
		IEntity vehicle = FindVehicle(vehicleId);
		ORRadio_Track current, upcoming;
		int offsetMs;
		if (!vehicle)
			return;

		if (!stations.IsIndexValid(station))
		{
			ReportStartFailure(string.Format("station %1 is not in the stations config", station));
			return;
		}

		if (!GetNowPlaying(station, current, offsetMs, upcoming))
		{
			if (!m_World)
				ReportStartFailure("the manager has no world");
			else if (m_iSeed == 0)
				ReportStartFailure("the station clock has not reached this machine (seed 0)");
			else
				ReportStartFailure(string.Format("station %1 has no playable tracks; check m_iDurationMs in the stations config", station));

			return;
		}

		if (offsetMs > current.m_iDurationMs - END_GUARD_MS)
			return;

		ORRadio_Station stationData = stations[station];
		vector transform[4];
		vehicle.GetTransform(transform);
		float loudness = Math.Pow(volume / (float)VOLUME_MAX, m_fVolumeCurve);
		array<string> names = {"Offset", "MusicTrackID", "BroadcastType", "Volume"};
		array<float> values = {offsetMs, current.m_iBankIndex, BROADCAST_TYPE_MUSIC, loudness};
		AudioHandle handle = AudioSystem.PlayEvent(stationData.m_sAudioProject, stationData.m_sEvent, transform, names, values);
		if (handle == AudioHandle.Invalid)
		{
			float distance = -1;
			IEntity listener = SCR_PlayerController.GetLocalControlledEntity();
			if (listener)
				distance = vector.Distance(vehicle.GetOrigin(), listener.GetOrigin());

			string details = string.Format(" ('%1', bank index %2, %3 ms of %4 in, %5 m away)", current.m_sTitle, current.m_iBankIndex, offsetMs, current.m_iDurationMs, distance);
			ReportStartFailure(string.Format("could not play event '%1' of '%2'", stationData.m_sEvent, stationData.m_sAudioProject), details);
			return;
		}

		if (!m_bReportedFirstSound)
		{
			m_bReportedFirstSound = true;
			Print(string.Format("[AROR Radio] Playing '%1' on %2 (bank index %3, %4 ms in, loudness %5)", current.m_sTitle, stationData.m_sName, current.m_iBankIndex, offsetMs, loudness), LogLevel.NORMAL);
		}

		ORRadio_Sound sound = new ORRadio_Sound();
		sound.m_iVehicleId = vehicleId;
		sound.m_iStation = station;
		sound.m_iVolume = volume;
		sound.m_Track = current;
		sound.m_Handle = handle;
		sound.m_Vehicle = vehicle;
		m_aSounds.Insert(sound);
	}

	//------------------------------------------------------------------------------------------------
	//! Logs each reason a radio could not start once, as starting is retried every selection; details are logged with
	//! the first occurrence but do not make a reason count as new
	protected void ReportStartFailure(string reason, string details = "")
	{
		if (m_aReportedFailures.Contains(reason))
			return;

		m_aReportedFailures.Insert(reason);
		Print("[AROR Radio] Radio did not start: " + reason + details, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected int FindSound(int vehicleId)
	{
		for (int i = 0; i < m_aSounds.Count(); i++)
		{
			if (m_aSounds[i].m_iVehicleId == vehicleId)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopSound(int index, float fadeSeconds)
	{
		ORRadio_Sound sound = m_aSounds[index];
		if (sound.m_Handle != AudioHandle.Invalid)
			AudioSystem.TerminateSoundFadeOut(sound.m_Handle, true, fadeSeconds);

		m_aSounds.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopAllSounds()
	{
		for (int i = m_aSounds.Count() - 1; i >= 0; i--)
		{
			StopSound(i, FADE_STOP_SECONDS);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a station's art and current track to the player who tuned it
	void ShowNowPlaying(int station)
	{
		array<ref ORRadio_Station> stations = GetStations();
		if (!stations.IsIndexValid(station))
			return;

		if (!m_NowPlayingCard)
			m_NowPlayingCard = new ORRadio_NowPlayingCard(m_sNowPlayingLayout, m_fNowPlayingSeconds);

		string title;
		ORRadio_Track current, upcoming;
		int offsetMs;
		if (GetNowPlaying(station, current, offsetMs, upcoming))
			title = current.m_sTitle;

		// The first radio of a game starts the station clock, which arrives after the card is up (RefreshNowPlayingTitle)
		m_iCardStation = station;
		m_bCardTitlePending = title.IsEmpty();

		ORRadio_Station stationData = stations[station];
		m_NowPlayingCard.Show(stationData.m_sName, title, stationData.m_sAlbumArt);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshNowPlayingTitle()
	{
		if (!m_bCardTitlePending || !m_NowPlayingCard || !m_NowPlayingCard.IsShowing())
			return;

		ORRadio_Track current, upcoming;
		int offsetMs;
		if (!GetNowPlaying(m_iCardStation, current, offsetMs, upcoming))
			return;

		m_bCardTitlePending = false;
		m_NowPlayingCard.SetTitle(current.m_sTitle);
	}
}
