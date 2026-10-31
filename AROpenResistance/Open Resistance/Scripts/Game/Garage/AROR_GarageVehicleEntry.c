//! One inventory node in a garaged vehicle: prefab plus optional magazine fill / health, with nested children.
//! Unlike Resistance Storage, condition is preserved for a later retrieve.
class AROR_GarageItemSnapshot : Managed
{
	ResourceName m_sPrefab;

	//! Remaining rounds when this node is a magazine; -1 if not applicable
	int m_iAmmoCount = -1;

	//! Scaled health when a damage manager exists on the item; -1 if not captured
	float m_fHealthScaled = -1;

	ref array<ref AROR_GarageItemSnapshot> m_aChildren = {};
}

//! Mounted weapon ammo on the hull/turret (separate from cargo inventory).
class AROR_GarageWeaponAmmoSnapshot : Managed
{
	//! Stable order index across FindComponents(BaseWeaponManagerComponent) + GetWeapons
	int m_iWeaponIndex;

	ResourceName m_sMagazinePrefab;

	int m_iAmmoCount;
}

//! Per hit-zone health on a garaged vehicle (wheels, engine, fuel tanks, etc.)
class AROR_GarageHitZoneSnapshot : Managed
{
	string m_sName;
	float m_fHealthScaled = 1;
}

//! Unique Resistance Garage entry (see ARCHITECTURE.md -> Resistance Garage).
//! Prefab alone is never the key — each store gets a monotonic id.
class AROR_GarageVehicleEntry : Managed
{
	int m_iId;
	ResourceName m_sPrefab;

	//! Bought new from Imports: no snapshot was taken, so retrieving it keeps the prefab's own fuel, cargo and
	//! condition instead of applying the (empty) snapshot below
	bool m_bFactoryNew;

	//! Default hit-zone health for UI summaries (0..1)
	float m_fHealthScaled = 1;

	//! Full hit-zone map so wheels/parts survive store/retrieve (not just default health)
	ref array<ref AROR_GarageHitZoneSnapshot> m_aHitZones = {};

	//! 0..1 total fuel / total max fuel at store time (0 when the vehicle has no fuel system)
	float m_fFuelNormalized;

	//! Per SCR_FuelNode tank id + absolute fuel (matched by id on restore)
	ref array<int> m_aFuelTankIds = {};
	ref array<float> m_aFuelAmounts = {};

	ref array<ref AROR_GarageItemSnapshot> m_aCargoRoots = {};
	ref array<ref AROR_GarageWeaponAmmoSnapshot> m_aWeaponAmmo = {};
}
