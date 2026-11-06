//! Base SCR_AISwitchMagazine resolves the weapon manager from the AI's compartment owner and passes it
//! unchecked to GetCurrentMagazineComponent. An AI seated in an ordinary vehicle seat (no weapon manager on
//! the vehicle), as QRF crews are, crashes the VM when a mod's behaviour requests a magazine switch.
modded class SCR_AISwitchMagazine
{
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		if (m_CompartmentAccessComp)
		{
			BaseCompartmentSlot compartmentSlot = m_CompartmentAccessComp.GetCompartment();
			if (compartmentSlot && !compartmentSlot.GetOwner().FindComponent(BaseWeaponManagerComponent))
				return ENodeResult.FAIL;
		}
		return super.EOnTaskSimulate(owner, dt);
	}
}
