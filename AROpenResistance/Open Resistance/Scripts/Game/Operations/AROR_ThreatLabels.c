//! Display labels for one army's Readiness and Crackdown levels (1-5). Logic only ever uses the integers.
class AROR_ThreatLabelSet : Managed
{
	string m_sReadinessHeading;			//!< Card heading and HUD key, e.g. "ГОТОВНОСТЬ" or "THREATCON"
	string m_sReadinessHeadingLine;		//!< English line under the heading
	string m_sCrackdownHeading;
	string m_sCrackdownHeadingLine;
	string m_sReadinessRaised;			//!< Alert headline when Readiness goes up
	string m_sReadinessLowered;
	string m_sReadinessUpIntercept = "INTERCEPT · COMMAND NET";
	string m_sReadinessDownIntercept = "INTERCEPT · COMMAND NET";
	string m_sCrackdownUpIntercept = "INTERCEPT · GARRISON ORDERS";
	string m_sCrackdownDownIntercept = "INTERCEPT · GARRISON ORDERS";
	ref array<string> m_aReadiness = {};
	ref array<string> m_aReadinessLines = {};
	ref array<string> m_aCrackdown = {};
	ref array<string> m_aCrackdownLines = {};

	//------------------------------------------------------------------------------------------------
	string GetLabel(AROR_EThreatScale scale, int level)
	{
		if (scale == AROR_EThreatScale.READINESS)
			return Pick(m_aReadiness, level);
		return Pick(m_aCrackdown, level);
	}

	//------------------------------------------------------------------------------------------------
	string GetLine(AROR_EThreatScale scale, int level)
	{
		if (scale == AROR_EThreatScale.READINESS)
			return Pick(m_aReadinessLines, level);
		return Pick(m_aCrackdownLines, level);
	}

	//------------------------------------------------------------------------------------------------
	string GetHeading(AROR_EThreatScale scale)
	{
		if (scale == AROR_EThreatScale.READINESS)
			return m_sReadinessHeading;
		return m_sCrackdownHeading;
	}

	//------------------------------------------------------------------------------------------------
	string GetHeadingLine(AROR_EThreatScale scale)
	{
		if (scale == AROR_EThreatScale.READINESS)
			return m_sReadinessHeadingLine;
		return m_sCrackdownHeadingLine;
	}

	//------------------------------------------------------------------------------------------------
	//! Headline for a level change, as listed under Level-change copy in the design system
	string GetChangeHeadline(AROR_EThreatScale scale, bool raised)
	{
		if (scale == AROR_EThreatScale.READINESS)
		{
			if (raised)
				return m_sReadinessRaised;
			return m_sReadinessLowered;
		}
		if (raised)
			return "CRACKDOWN INCREASED";
		return "CRACKDOWN EASED";
	}

	//------------------------------------------------------------------------------------------------
	//! Intercept line above an alert headline, in the army's own voice, e.g. "ПЕРЕХВАТ · РЕЖИМ УСИЛЕН"
	string GetIntercept(AROR_EThreatScale scale, bool raised)
	{
		if (scale == AROR_EThreatScale.READINESS)
		{
			if (raised)
				return m_sReadinessUpIntercept;
			return m_sReadinessDownIntercept;
		}
		if (raised)
			return m_sCrackdownUpIntercept;
		return m_sCrackdownDownIntercept;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Pick(array<string> labels, int level)
	{
		if (level < 1 || level > labels.Count())
			return "--";
		return labels[level - 1];
	}
}

//! Label sets keyed by the army (faction preset ID) holding a role, so a US-occupied scenario shows THREATCON
//! and a USSR-occupied one shows ГОТОВНОСТЬ with no code changes.
//! Extender mods add their armies with a modded class overriding Create() and calling super for the rest.
class AROR_ThreatLabels
{
	protected static ref map<string, ref AROR_ThreatLabelSet> s_mSets = new map<string, ref AROR_ThreatLabelSet>();

	//------------------------------------------------------------------------------------------------
	//! Labels for the army a role uses in this campaign; every machine (preset IDs are replicated)
	static AROR_ThreatLabelSet ForRole(AROR_EFactionRole role)
	{
		string presetId;
		AROR_FactionPresetsComponent presets = AROR_FactionPresetsComponent.GetInstance();
		if (presets)
			presetId = presets.GetPresetId(role);
		return ForPreset(presetId);
	}

	//------------------------------------------------------------------------------------------------
	static AROR_ThreatLabelSet ForPreset(string presetId)
	{
		AROR_ThreatLabelSet labels = s_mSets.Get(presetId);
		if (!labels)
		{
			labels = Create(presetId);
			s_mSets.Set(presetId, labels);
		}
		return labels;
	}

	//------------------------------------------------------------------------------------------------
	//! What a level means for players, the same for every army
	static string GetEffect(AROR_EThreatScale scale, int level)
	{
		if (scale == AROR_EThreatScale.READINESS)
		{
			switch (level)
			{
				case 1: return "Garrisons only. QRFs are infantry on foot or by truck.";
				case 2: return "Motorised QRFs in light utility vehicles.";
				case 3: return "Armed light vehicles join QRFs.";
				case 4: return "Armoured cars and APCs join QRFs.";
				case 5: return "Helicopter insertions and gunship support.";
			}
			return string.Empty;
		}
		switch (level)
		{
			case 1: return "Patrols on set routes. Slow response to attacks.";
			case 2: return "Roadblocks on main routes. Vehicles get searched.";
			case 3: return "Towns near rebel activity get raided.";
			case 4: return "Bigger, faster QRFs. Civilian support drops after attacks.";
			case 5: return "Curfew. Armed rebels engaged on sight. Every alert gets a QRF.";
		}
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static AROR_ThreatLabelSet Create(string presetId)
	{
		AROR_ThreatLabelSet labels = new AROR_ThreatLabelSet();
		if (presetId == "USSR_ARMY")
		{
			labels.m_sReadinessHeading = "ГОТОВНОСТЬ";
			labels.m_sReadinessHeadingLine = "Combat readiness";
			labels.m_sCrackdownHeading = "РЕЖИМ";
			labels.m_sCrackdownHeadingLine = "Crackdown";
			labels.m_sReadinessRaised = "COMBAT READINESS RAISED";
			labels.m_sReadinessLowered = "READINESS LOWERED";
			labels.m_sReadinessUpIntercept = "ПЕРЕХВАТ · БОЕВАЯ ГОТОВНОСТЬ ПОВЫШЕНА";
			labels.m_sReadinessDownIntercept = "ПЕРЕХВАТ · БОЕВАЯ ГОТОВНОСТЬ ПОНИЖЕНА";
			labels.m_sCrackdownUpIntercept = "ПЕРЕХВАТ · РЕЖИМ УСИЛЕН";
			labels.m_sCrackdownDownIntercept = "ПЕРЕХВАТ · РЕЖИМ ОСЛАБЛЕН";
			labels.m_aReadiness = {"МИРНОЕ ВРЕМЯ", "ПОСТОЯННАЯ", "ПОВЫШЕННАЯ", "ВОЕННАЯ ОПАСНОСТЬ", "ПОЛНАЯ"};
			labels.m_aReadinessLines = {"Peacetime", "Constant readiness", "Increased readiness", "Threat of war", "Full readiness"};
			labels.m_aCrackdown = {"ПАТРУЛИРОВАНИЕ", "БЛОКПОСТЫ", "ЗАЧИСТКА", "КАРАТЕЛЬНЫЕ МЕРЫ", "ВОЕННОЕ ПОЛОЖЕНИЕ"};
			labels.m_aCrackdownLines = {"Patrols", "Checkpoints", "Sweeps", "Punitive measures", "Martial law"};
			return labels;
		}

		// US labels; armies without their own set use the generic English one below
		labels.m_sCrackdownHeading = "CRACKDOWN";
		labels.m_sCrackdownHeadingLine = "How hard they react";
		labels.m_aCrackdown = {"PATROLS", "CHECKPOINTS", "SWEEPS", "REPRISALS", "MARTIAL LAW"};
		labels.m_aCrackdownLines = {"Routine presence", "Roadblocks on main routes", "Raids on towns", "Civilians punished", "Curfew"};
		if (presetId == "US_ARMY")
		{
			labels.m_sReadinessHeading = "THREATCON";
			labels.m_sReadinessHeadingLine = "Threat condition";
			labels.m_sReadinessRaised = "THREATCON RAISED";
			labels.m_sReadinessLowered = "THREATCON LOWERED";
			labels.m_aReadiness = {"THREATCON NORMAL", "THREATCON ALPHA", "THREATCON BRAVO", "THREATCON CHARLIE", "THREATCON DELTA"};
			labels.m_aReadinessLines = {"General threat", "Possible threat", "Increased threat", "Attack likely", "Attack imminent"};
			return labels;
		}

		labels.m_sReadinessHeading = "READINESS";
		labels.m_sReadinessHeadingLine = "What they can bring";
		labels.m_sReadinessRaised = "READINESS RAISED";
		labels.m_sReadinessLowered = "READINESS LOWERED";
		labels.m_aReadiness = {"PEACETIME", "CONSTANT", "INCREASED", "THREAT OF WAR", "FULL"};
		labels.m_aReadinessLines = {"Garrisons only", "Motorised responses", "Armed vehicles", "Armour deployed", "Air support"};
		return labels;
	}
}
