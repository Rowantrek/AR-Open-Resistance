//! QRF policy persists independently of Economy. Physical deployments/contacts remain session-local.
class AROR_OperationManagerComponentSerializer : ScriptedComponentSerializer
{
	protected static const int VERSION = 1;
	override static typename GetTargetType()
	{
		return AROR_OperationManagerComponent;
	}
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		const AROR_OperationManagerComponent manager = AROR_OperationManagerComponent.Cast(component);
		context.WriteValue("version", VERSION);
		context.WriteValue("occupierPressure", manager.GetWarPressure(AROR_EFactionRole.OCCUPIERS));
		context.WriteValue("invaderPressure", manager.GetWarPressure(AROR_EFactionRole.INVADERS));
		AROR_QRFFinance finance = manager.GetQRFFinance();
		// Do not debit another component while serializing. Carry pending confirmed losses as debt.
		context.WriteValue("occupierDebt", finance.Debt(AROR_EFactionRole.OCCUPIERS) + finance.PendingLossValue(AROR_EFactionRole.OCCUPIERS));
		context.WriteValue("invaderDebt", finance.Debt(AROR_EFactionRole.INVADERS) + finance.PendingLossValue(AROR_EFactionRole.INVADERS));
		return ESerializeResult.OK;
	}
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		AROR_OperationManagerComponent manager = AROR_OperationManagerComponent.Cast(component);
		int version;
		if (!context.ReadValue("version", version) || version != VERSION)
			return false;
		int occupierPressure, invaderPressure, occupierDebt, invaderDebt;
		if (!context.ReadValue("occupierPressure", occupierPressure) || !context.ReadValue("invaderPressure", invaderPressure))
			return false;
		if (!context.ReadValue("occupierDebt", occupierDebt) || !context.ReadValue("invaderDebt", invaderDebt))
			return false;
		manager.RestoreQRFStrategy_S(occupierPressure, invaderPressure, occupierDebt, invaderDebt);
		return true;
	}
}
