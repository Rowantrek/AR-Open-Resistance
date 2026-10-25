//! Adapter to Rowan's authoritative Economy. No direct balance mutation, duplicate income or refunds.
class AROR_QRFFinance
{
	ref array<ref AROR_QRFFunding> m_aDeployments = {};
	int m_iOccupierDebt;
	int m_iInvaderDebt;
	int m_iOccupierLossValue;
	int m_iInvaderLossValue;
	//! Read-only save snapshot: preserve deaths confirmed since the last periodic reconciliation.
	int PendingLossValue(AROR_EFactionRole role)
	{
		int value;
		foreach (AROR_QRFFunding account : m_aDeployments)
		{
			if (account.m_eRole != role)
				continue;
			foreach (AROR_QRFAssetAccount asset : account.m_aAssets)
			{
				if (asset.IsLost())
					value += asset.m_iValue;
			}
		}
		return value;
	}

	int Debt(AROR_EFactionRole role)
	{
		if (role == AROR_EFactionRole.OCCUPIERS)
			return m_iOccupierDebt;
		if (role == AROR_EFactionRole.INVADERS)
			return m_iInvaderDebt;
		return 0;
	}
	void SetDebt_S(AROR_EFactionRole role, int amount)
	{
		if (!Replication.IsServer())
			return;
		if (role == AROR_EFactionRole.OCCUPIERS)
			m_iOccupierDebt = Math.Clamp(amount, 0, 100000000);
		else if (role == AROR_EFactionRole.INVADERS)
			m_iInvaderDebt = Math.Clamp(amount, 0, 100000000);
	}
	int Available(AROR_EFactionRole role)
	{
		if (role != AROR_EFactionRole.OCCUPIERS && role != AROR_EFactionRole.INVADERS)
			return 0;
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (!economy || Debt(role) > 0)
			return 0;
		int reserved;
		foreach (AROR_QRFFunding deployment : m_aDeployments)
		{
			if (deployment.m_eRole == role)
				reserved += deployment.Reserved();
		}
		return Math.Max(0, economy.GetFactionFunds(role) - reserved);
	}
	AROR_QRFFunding Reserve_S(AROR_Operation operation, int assets, int overhead)
	{
		if (!Replication.IsServer() || !operation || assets < 0 || overhead < 0)
			return null;
		if (!AROR_EconomyComponent.GetInstance())
			return null;
		if (Debt(operation.GetRole()) > 0)
			return null;
		if (operation.GetRole() != AROR_EFactionRole.OCCUPIERS && operation.GetRole() != AROR_EFactionRole.INVADERS)
			return null;
		foreach (AROR_QRFFunding existing : m_aDeployments)
		{
			if (existing.m_sOperationId == operation.GetId())
				return existing;
		}
		if (assets + overhead > Available(operation.GetRole()))
			return null;
		AROR_QRFFunding account = new AROR_QRFFunding();
		account.m_sOperationId = operation.GetId();
		account.m_eRole = operation.GetRole();
		account.m_iQuote = assets;
		account.m_iOverhead = overhead;
		m_aDeployments.Insert(account);
		Print(string.Format("[AROR FINANCE] %1 reserved role=%2 assets=%3 overhead=%4", operation.GetId(), operation.GetRole(), assets, overhead), LogLevel.NORMAL);
		return account;
	}
	//! External systems can spend collateral. Any resulting shortfall becomes saved debt, never a negative bank.
	void Charge_S(AROR_EFactionRole role, int amount)
	{
		if (!Replication.IsServer() || amount <= 0)
			return;
		if (role != AROR_EFactionRole.OCCUPIERS && role != AROR_EFactionRole.INVADERS)
			return;
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		int paid;
		if (economy)
		{
			int take = Math.Min(amount, economy.GetFactionFunds(role));
			if (take > 0 && economy.SpendFactionFunds_S(role, take))
				paid = take;
		}
		SetDebt_S(role, Debt(role) + amount - paid);
		Print(string.Format("[AROR FINANCE] role=%1 charge=%2 paid=%3 outstandingDebt=%4", role, amount, paid, Debt(role)), LogLevel.NORMAL);
	}
	void Activate_S(AROR_QRFFunding account)
	{
		if (!Replication.IsServer() || !account || account.m_bActivated || account.m_bClosed)
			return;
		// Set before Economy invokes its subscribers; repeated notifications cannot double-charge.
		account.m_bActivated = true;
		Charge_S(account.m_eRole, account.m_iOverhead);
	}
	void Observe_S(AROR_QRFFunding account)
	{
		if (!Replication.IsServer() || !account)
			return;
		int losses = account.ObserveLosses_S();
		if (account.m_eRole == AROR_EFactionRole.OCCUPIERS)
			m_iOccupierLossValue += losses;
		else
			m_iInvaderLossValue += losses;
		Charge_S(account.m_eRole, losses);
	}
	void Tick_S()
	{
		if (!Replication.IsServer())
			return;
		array<AROR_EFactionRole> roles = {AROR_EFactionRole.OCCUPIERS, AROR_EFactionRole.INVADERS};
		foreach (AROR_EFactionRole role : roles)
		{
			int debt = Debt(role);
			if (debt <= 0)
				continue;
			AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
			if (!economy || economy.GetFactionFunds(role) <= 0)
				continue;
			SetDebt_S(role, 0);
			Charge_S(role, debt);
		}
		for (int index = m_aDeployments.Count() - 1; index >= 0; index--)
		{
			AROR_QRFFunding account = m_aDeployments[index];
			Observe_S(account);
			if (account.m_bClosed && account.Reserved() == 0)
				m_aDeployments.Remove(index);
		}
	}
}
