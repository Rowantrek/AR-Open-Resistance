// Runs extracted Enforce classification/fit decisions with native services mocked. Not an Enforce compiler.
// Node: node tools/Test-WCSIntegration.js
function runWCSIntegrationTests(sources) {
  let assertions = 0;
  const check = (ok, message) => { assertions++; if (!ok) throw Error(message); };
  function enumValues(source) {
    const body = source.slice(source.indexOf('{') + 1, source.lastIndexOf('}')).replace(/\/\/[^\n]*/g, '');
    let value = -1;
    return Object.fromEntries(body.split(',').map(x => x.trim()).filter(Boolean).map(entry => {
      const [name, explicit] = entry.split('=').map(x => x.trim());
      value = explicit === undefined ? value + 1 : Number(explicit);
      return [name, value];
    }));
  }
  const types = enumValues(sources.vehicleTypes), weapons = enumValues(sources.weaponTypes);
  const categories = enumValues(sources.categories), requirements = enumValues(sources.requirements);
  const env = {
    AROR_EVehicleType: types, AROR_EWeaponType: weapons, AROR_EFactionPresetCategory: categories,
    AROR_EImportPointRequirement: requirements,
    AROR_EForceType: {ARMOUR: 2},
    EEditableEntityLabel: new Proxy({}, {get: (_, key) => key}),
    SCR_EArsenalItemType: new Proxy({}, {get: (_, key) => key}),
    string: {Empty: '', Format: (s, ...args) => s.replace(/%(\d+)/g, (_, i) => args[i - 1])},
    Math: {Max: Math.max}, HelicopterControllerComponent: {},
    AROR_CatalogClassifier: {}
  };
  function compile(source, name, outTerms = false) {
    const method = source.match(new RegExp('^\\t[^\\r\\n]*?\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if (!method) throw Error('Missing method ' + name);
    const args = method[1].split(',').map(arg => arg.trim().replace(/\b(?:out|notnull|ResourceName|string|int|bool|SCR_EntityCatalogEntry|SCR_EArsenalItemType|AROR_\w+)\s+/g, '')).join(',');
    let body = method[2].replace(/\/\/[^\n]*/g, '')
      .replace(/\b(?:ResourceName|string|int|bool|AROR_\w+) (\w+) =/g, 'let $1 =')
      .replace(/\b(?:ResourceName|string|int|bool|AROR_\w+) (\w+);/g, 'let $1;')
      .replace(/(\w+)\.ToLower\(\);/g, '$1 = $1.toLowerCase();')
      .replace(/\.Contains\(/g, '.includes(').replace(/\.LastIndexOf\(/g, '.lastIndexOf(')
      .replace(/\.Substring\(/g, '.substr(').replace(/\.Length\(\)/g, '.length');
    for (const helper of ['LowerName', 'IsTankName', 'IsAntiAirName', 'StaticWeaponType']) {
      body = body.replace(new RegExp('(?<![.\\w])' + helper + '\\(', 'g'), 'AROR_CatalogClassifier.' + helper + '(');
    }
    if (outTerms) body = body.replace(/\breturn;/g, 'return {cost, importPoints, importRequirement};') + '\nreturn {cost, importPoints, importRequirement};';
    return new Function('env', 'with(env) { return function(' + args + ') {' + body + '}; }')(env);
  }
  for (const name of ['LowerName', 'IsTankName', 'IsAntiAirName', 'StaticWeaponType', 'VehicleType', 'DefaultReadiness', 'WeaponType']) {
    env.AROR_CatalogClassifier[name] = compile(sources.classifier, name);
  }
  const classifier = env.AROR_CatalogClassifier;
  const entry = (prefab, labels = []) => ({GetPrefab: () => prefab, HasEditableEntityLabel: label => labels.includes(label)});
  const fixtures = [
    ['T14_Green.et', ['VEHICLE_APC'], types.TANK],
    ['Tank_T14_Base.et', ['VEHICLE_TRUCK', 'VEHICLE_APC', 'TRAIT_ARMED'], types.TANK],
    ['T72B3_Woodland.et', ['VEHICLE_APC'], types.TANK],
    ['M1A2_SEP.et', ['VEHICLE_APC'], types.TANK],
    ['M923A1_tanker.et', ['VEHICLE_TRUCK', 'TRAIT_REFUELING'], types.FUEL_TRUCK],
    ['Ural4320_tanker_CIV_randomized.et', ['VEHICLE_TRUCK', 'TRAIT_REFUELING'], types.FUEL_TRUCK],
    ['M923A1_ZU23.et', ['VEHICLE_TRUCK', 'TRAIT_ARMED', 'TRAIT_LOGISTICS'], types.AA_VEHICLE],
    ['M923A1_M2.et', ['VEHICLE_TRUCK', 'TRAIT_ARMED'], types.ARMED_TRUCK],
    ['M923A1_transport.et', ['VEHICLE_TRUCK'], types.MOTOR_TRUCK],
    ['M923A1_supply.et', ['VEHICLE_TRUCK', 'TRAIT_LOGISTICS'], types.SUPPLY_TRUCK],
    ['BMP3_Green.et', ['VEHICLE_APC'], types.IFV],
    ['BTR70_KBA105_Green.et', ['VEHICLE_APC'], types.APC],
    ['AH64D_USArmy_M261.et', ['VEHICLE_HELICOPTER', 'TRAIT_ARMED'], types.HELI_ATTACK],
    ['UH1H_unarmed.et', ['VEHICLE_HELICOPTER'], types.HELI_TRANSPORT],
    ['WeaponTripod_ZU23_USSR.et', ['VEHICLE_TURRET'], types.STATIC_AA],
    ['Tripod_9M113.et', ['VEHICLE_TURRET'], types.STATIC_AT],
    ['Tripod_M122_MG_M60.et', ['VEHICLE_TURRET'], types.STATIC_MG_LOW],
    ['Tripod_6T7_NSV.et', ['VEHICLE_TURRET'], types.STATIC_MG_HIGH],
    ['Mortar_M252.et', ['VEHICLE_TURRET'], types.STATIC_MORTAR]
  ];
  for (const [prefab, labels, expected] of fixtures) {
    check(classifier.VehicleType(entry('{0000000000000000}Prefabs/Test/' + prefab, labels)) === expected, 'Correct classification: ' + prefab);
  }
  check(classifier.DefaultReadiness(categories.VEHICLES, types.TANK) === 5, 'Tanks remain gated until readiness 5');
  check(classifier.DefaultReadiness(categories.VEHICLES, types.STATIC_AA) === 3, 'Static AA does not enter readiness-1 MG pools');
  check(classifier.StaticWeaponType(entry('WeaponTripod_ZU23_FIA.et')) === types.STATIC_AA, 'Tripod catalog classifier recognises ZU23 without vehicle labels');
  check(classifier.WeaponType('ROCKET_LAUNCHER', 'launcher_9k38_igla.et') === weapons.LAUNCHER_AA, 'Igla is anti-air, not anti-tank');
  check(classifier.WeaponType('ROCKET_LAUNCHER', 'launcher_fim92_stinger.et') === weapons.LAUNCHER_AA, 'Stinger is anti-air, not anti-tank');
  check(classifier.WeaponType('ROCKET_LAUNCHER', 'launcher_rpg7.et') === weapons.LAUNCHER_AT, 'RPG retains anti-tank classification');
  check(classifier.WeaponType('RIFLE', 'rifle_scar.et') === weapons.ASSAULT_RIFLE, 'WCS rifle remains an assault rifle');

  const vehicleTerms = compile(sources.classifier, 'DefaultVehicleTerms', true);
  const tank = vehicleTerms(types.TANK);
  check(tank.cost === 40000 && tank.importPoints === 6 && tank.importRequirement === requirements.PORT, 'New tanks have non-zero tank pricing and port requirements');
  const gunship = vehicleTerms(types.HELI_ATTACK);
  check(gunship.cost === 60000 && gunship.importRequirement === requirements.AIR, 'Attack helicopters need aircraft pricing and air facilities');
  const weaponTerms = compile(sources.classifier, 'DefaultWeaponTerms', true);
  for (const launcher of [weapons.LAUNCHER_AT, weapons.LAUNCHER_AA]) {
    const terms = weaponTerms(launcher);
    check(terms.importPoints === 1 && terms.importRequirement === requirements.PORT, 'AT and AA launchers are imported from the first seaport');
  }

  const airMisfit = compile(sources.air, 'PresetAircraftMisfit');
  const aircraft = (crew, cargo, helicopter = true) => ({m_Vehicle: {FindComponent: () => helicopter ? {} : null}, CountRequiredCrewSeats_S: () => crew, CountCargoSeats_S: () => cargo});
  check(airMisfit(aircraft(1, 8), true) === '', 'Single-pilot transport with cargo is compatible');
  check(airMisfit(aircraft(1, 0), true).includes('passenger'), 'Transport with no cabin falls back');
  check(airMisfit(aircraft(0, 8), true).includes('pilot'), 'Aircraft with no pilot seat falls back');
  check(airMisfit(aircraft(1, 0), false) === '', 'Pilot-controlled gunship remains compatible');
  check(airMisfit(aircraft(2, 0), false).includes('turret crew'), 'Multi-crew gunship falls back instead of dispatching unarmed');
  check(airMisfit(aircraft(1, 8, false), true).includes('controller'), 'Non-helicopter asset cannot replace a helicopter');
  check(airMisfit({m_Vehicle: null}, true).includes('controller'), 'Missing aircraft is rejected safely');

  // Existing ground dispatch fallback must continue rejecting hulls that exceed its reservation.
  const groundMisfit = compile(sources.ground, 'PresetHullMisfit');
  const profile = {m_iCrewCount: 1, m_eType: 0, m_bFillPassengerSeats: true, m_iPersonnelBudget: 8};
  const hull = (crew, cargo) => ({CountRequiredCrewSeats_S: () => crew, CountCargoSeats_S: () => cargo});
  const fit = hull(3, 8);
  check(groundMisfit(fit, profile) === '' && fit.m_iCrewCount === 3, 'WCS ground hull receives all required crew');
  check(groundMisfit(hull(8, 0), profile) !== '', 'Ground hull must leave space in the reserved personnel budget');
  check(groundMisfit(hull(2, 0), profile) !== '', 'Passenger package cannot select a hull with no cargo seats');
  check(groundMisfit(hull(-1, 8), profile) !== '', 'Unpiloted hull falls back to authored vehicle');
  return {assertions, result: 'PASS', limitations: 'Mocked native services; no Workbench compile, multiplayer session or AI flight/drive test.'};
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = runWCSIntegrationTests;
  if (require.main === module) {
    const fs = require('fs'), path = require('path');
    const root = path.resolve(__dirname, '../AROpenResistance/Open Resistance/Scripts/Game');
    const files = {classifier: 'Factions/Presets/AROR_CatalogClassifier.c', vehicleTypes: 'Factions/Presets/AROR_EVehicleType.c', weaponTypes: 'Factions/Presets/AROR_EWeaponType.c', categories: 'Factions/Presets/AROR_EFactionPresetCategory.c', requirements: 'Factions/Presets/AROR_EImportPointRequirement.c', air: 'Operations/AROR_ReaperQRFExecutor.c', ground: 'Operations/AROR_OperationExecutorComponent.c'};
    const sources = Object.fromEntries(Object.entries(files).map(([name, file]) => [name, fs.readFileSync(path.join(root, file), 'utf8')]));
    console.log(JSON.stringify(runWCSIntegrationTests(sources), null, 2));
  }
}
