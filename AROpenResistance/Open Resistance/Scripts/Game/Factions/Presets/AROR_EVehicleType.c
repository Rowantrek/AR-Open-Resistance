//! Faction preset vehicle types (empty vehicles, no crew). A preset can list many vehicles per type.
//! Fixed list used by the core: do not extend. Presets ignore entries whose type is not in this list.
enum AROR_EVehicleType
{
	MOTOR_CAR,
	MOTOR_TRUCK,
	ARMED_CAR,
	ARMED_TRUCK,
	APC,
	IFV,
	TANK,
	AA_VEHICLE,
	RADAR_AA_VEHICLE,
	SUPPLY_TRUCK,
	FUEL_TRUCK,
	AMBULANCE,
	HELI_TRANSPORT,
	HELI_ATTACK,
	BOAT,
	STATIC_MG_LOW,
	STATIC_MG_HIGH,
	STATIC_AT,
	STATIC_AA,
	STATIC_MORTAR,
	STATIC_ARTILLERY
}
