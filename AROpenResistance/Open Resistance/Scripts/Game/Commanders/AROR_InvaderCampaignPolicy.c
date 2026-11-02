//! Bounded strategic decisions. No spawning, bank, ownership or random-number authority.
class AROR_InvaderCampaignPolicy
{
	static int TargetShare(int baseline, int readiness, int crackdown)
	{
		return Math.Clamp(baseline + Math.Max(0, readiness - 2) * 5 + Math.Floor(Math.Max(0, crackdown - 10) / 10.0), 10, 80);
	}
	static int Funding(int baseline, int perReadiness, int perCrackdown, int readiness, int crackdown)
	{
		return baseline + Math.Max(0, readiness - 2) * perReadiness + Math.Clamp(crackdown, 0, 100) * perCrackdown;
	}
	//! A soft equilibrium: either side can attack above/below the target, with bounded bias.
	static int InvaderAttackChance(int held, int total, int targetShare, int invaderWin = 50, int occupierWin = 50)
	{
		if (total <= 0)
			return 50;
		float share = 100.0 * held / total;
		float balance = 100.0 * occupierWin / Math.Max(1, invaderWin + occupierWin);
		return Math.Clamp(Math.Round(balance + (targetShare - share) * 2), 10, 90);
	}
	static int BattleChance(int attack, int defence, int attackIndustry, int defenceIndustry)
	{
		float commitment = Math.Max(1, Math.Max(attack, defence));
		int advantage = Math.Round(25 * (attack - defence) / commitment);
		return Math.Clamp(50 + advantage + Math.Clamp(attackIndustry, 0, 15) - Math.Clamp(defenceIndustry, 0, 15), 15, 85);
	}
	static int Cost(AROR_ELocationType type, int baseline)
	{
		switch (type)
		{
			case AROR_ELocationType.OUTPOST: return Math.Round(baseline * 0.7);
			case AROR_ELocationType.MILITARY_BASE: return Math.Round(baseline * 1.2);
			case AROR_ELocationType.RESOURCES: return Math.Round(baseline * 0.9);
			case AROR_ELocationType.FACTORY: return Math.Round(baseline * 1.6);
			case AROR_ELocationType.PORT: return Math.Round(baseline * 1.5);
			case AROR_ELocationType.HELIPORT: return Math.Round(baseline * 1.5);
			case AROR_ELocationType.AIRPORT: return Math.Round(baseline * 2.5);
		}
		return 0;
	}
}
