//! QRF policy persists independently of Economy. Physical deployments/contacts remain session-local.
class AROR_OperationManagerComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 4;
	override static typename GetTargetType()
	{
		return AROR_OperationManagerComponent;
	}
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_OperationManagerComponent manager = AROR_OperationManagerComponent.Cast(component);
		context.WriteValue("version", VERSION);
		context.WriteValue("occupierPressure", manager.GetReadinessPressure(AROR_EFactionRole.OCCUPIERS));
		context.WriteValue("invaderPressure", manager.GetReadinessPressure(AROR_EFactionRole.INVADERS));
		context.WriteValue("occupierCrackdown", manager.GetCrackdownForSave(AROR_EFactionRole.OCCUPIERS));
		context.WriteValue("invaderCrackdown", manager.GetCrackdownForSave(AROR_EFactionRole.INVADERS));
		AROR_QRFFinance finance = manager.GetQRFFinance();
		// Do not debit another component while serializing. Carry pending confirmed losses as debt.
		context.WriteValue("occupierDebt", finance.Debt(AROR_EFactionRole.OCCUPIERS) + finance.PendingLossValue(AROR_EFactionRole.OCCUPIERS));
		context.WriteValue("invaderDebt", finance.Debt(AROR_EFactionRole.INVADERS) + finance.PendingLossValue(AROR_EFactionRole.INVADERS));
		context.WriteValue("occupierQRFDispatches", manager.GetQRFDispatchCount(AROR_EFactionRole.OCCUPIERS));
		context.WriteValue("occupierAirDispatches", manager.GetQRFDispatchCount(AROR_EFactionRole.OCCUPIERS, true));
		context.WriteValue("invaderQRFDispatches", manager.GetQRFDispatchCount(AROR_EFactionRole.INVADERS));
		context.WriteValue("invaderAirDispatches", manager.GetQRFDispatchCount(AROR_EFactionRole.INVADERS, true));
		return ESerializeResult.OK;
	}
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_OperationManagerComponent manager = AROR_OperationManagerComponent.Cast(component);
		int version;
		if (!context.ReadValue("version", version) || version < 1 || version > VERSION)
			return false;
		int occupierPressure, invaderPressure, occupierDebt, invaderDebt;
		if (!context.ReadValue("occupierPressure", occupierPressure) || !context.ReadValue("invaderPressure", invaderPressure))
			return false;
		if (!context.ReadValue("occupierDebt", occupierDebt) || !context.ReadValue("invaderDebt", invaderDebt))
			return false;
		int occupierCrackdown = -1;
		int invaderCrackdown = -1;
		if (version >= 2)
		{
			// Version 2 saved the same meter under its old "Aggression" name.
			string occupierKey = "occupierCrackdown";
			string invaderKey = "invaderCrackdown";
			if (version == 2)
			{
				occupierKey = "occupierAggression";
				invaderKey = "invaderAggression";
			}
			if (!context.ReadValue(occupierKey, occupierCrackdown) || !context.ReadValue(invaderKey, invaderCrackdown))
				return false;
			if (occupierCrackdown < 0 || occupierCrackdown > 1000 || invaderCrackdown < 0 || invaderCrackdown > 1000)
				return false;
		}
		int occupierDispatches;
		int occupierAir;
		int invaderDispatches;
		int invaderAir;
		if (version >= 4)
		{
			if (!context.ReadValue("occupierQRFDispatches", occupierDispatches) || !context.ReadValue("occupierAirDispatches", occupierAir) || !context.ReadValue("invaderQRFDispatches", invaderDispatches) || !context.ReadValue("invaderAirDispatches", invaderAir))
				return false;
			if (!AROR_QRFResponseMix.CountsValid(occupierDispatches, occupierAir) || !AROR_QRFResponseMix.CountsValid(invaderDispatches, invaderAir))
				return false;
		}
		manager.RestoreQRFStrategy_S(occupierPressure, invaderPressure, occupierDebt, invaderDebt, occupierCrackdown, invaderCrackdown);
		manager.RestoreQRFResponseMix_S(occupierDispatches, occupierAir, invaderDispatches, invaderAir);
		return true;
	}
}
