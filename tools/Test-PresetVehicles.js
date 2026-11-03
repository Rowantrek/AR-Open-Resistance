/* Runs actual Enforce method bodies against mocked native services.
 * node tools/Test-PresetVehicles.js -- Workbench compilation/gameplay still required.
 */
const fs = require('fs');
const path = require('path');
const assert = require('assert/strict');
const root = path.join(__dirname, '../AROpenResistance/Open Resistance');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
for (const [name, fn] of Object.entries({
  IsEmpty() { return this.length === 0; }, Count() { return this.length; },
  Insert(value) { this.push(value); }, Clear() { this.length = 0; }
})) Object.defineProperty(Array.prototype, name, {value: fn, configurable: true});
Object.defineProperty(String.prototype, 'IsEmpty', {value() { return this.length === 0; }, configurable: true});
const category = {VEHICLES: 1};
const forceTypes = {INFANTRY: 0, MOTORIZED: 1, AIRBORNE: 4};
let server = true, randomCalls = 0, selected;
const env = {
  AROR_EFactionPresetCategory: category, AROR_EForceType: forceTypes,
  AROR_FactionPresetsComponent: {GetInstance: () => selected},
  AROR_FactionRolesComponent: {GetInstance: () => ({GetFactionKey: role => `ROLE_${role}`})},
  Replication: {IsServer: () => server},
  FactionAffiliationComponent: {Cast: value => value},
  SCR_ResourceNameUtils: {GetPrefabName: entity => entity.prefab},
  string: {Empty: ''}, Math: {Mod: (a, b) => a % b, RandomInt: (a, b) => { randomCalls++; return b - 1; }}
};
function method(source, name, members = []) {
  const match = source.match(new RegExp('^\\t[^\\r\\n]*\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  assert(match, `Missing ${name}`);
  const args = match[1].replace(/\bnotnull /g, '').replace(/array<(?:ref )?\w+>\s+/g, '').replace(/\b\w+ (\w+)(?=\s*(?:=|,|$))/g, '$1');
  let body = match[2]
    .replace(/array<(?:ref )?\w+> (\w+) = \{\};/g, 'let $1 = [];')
    .replace(/foreach \((\w+) (\w+) : ([^)]+)\)/g, 'for (const $2 of $3)')
    .replace(/\bbool (\w+);/g, 'let $1 = false;')
    .replace(/\b(?:int|string|ResourceName|AROR_\w+|FactionAffiliationComponent) (\w+) =/g, 'let $1 =')
    .replace(/(?<![.\w])m_\w+/g, name => 'this.' + name);
  for (const member of members) body = body.replace(new RegExp('(?<![.\\w])' + member + '\\(', 'g'), 'this.' + member + '(');
  return new Function(...Object.keys(env), `return function(${args}) {${body}}`)(...Object.values(env));
}
const presetSource = read('Scripts/Game/Factions/Presets/AROR_FactionPreset.c');
const componentSource = read('Scripts/Game/Factions/Presets/AROR_FactionPresetsComponent.c');
const profileSource = read('Scripts/Game/Operations/AROR_ForceProfile.c');
const garrisonSource = read('Scripts/Game/Garrisons/AROR_GarrisonComponent.c');
let replacements = {}, enabled = new Set();
const entry = (type, prefab) => ({
  GetListedPrefab: () => prefab, GetPrefab: () => replacements[prefab] || prefab,
  GetTypeValue: () => type, IsKnownType: () => type >= 0
});
function preset(entries) {
  return {
    CollectEntries: (cat, out) => out.push(...entries),
    GetEntries: (cat, type, out) => out.push(...entries.filter(e => e.GetTypeValue() === type && enabled.has(e.GetPrefab()))),
    GetAllEntries: (cat, out) => out.push(...entries.filter(e => enabled.has(e.GetPrefab()))),
    FindVehicleType: method(presetSource, 'FindVehicleType', ['CollectEntries'])
  };
}
const us = preset([entry(4, 'us_apc'), entry(4, 'us_apc_2'), entry(12, 'us_heli')]);
const rhs = preset([entry(4, 'rhs_apc'), entry(4, 'rhs_apc_2'), entry(12, 'rhs_heli'), entry(13, 'rhs_gunship')]);
const armies = {1: rhs, 2: us};
selected = {
  m_aPresets: [us, rhs], GetPreset: role => armies[role],
  GetVehicleType: method(componentSource, 'GetVehicleType'),
  PickVehicle: method(componentSource, 'PickVehicle', ['GetPreset']),
  ResolveVehicle: method(componentSource, 'ResolveVehicle', ['PickVehicle', 'GetVehicleType']),
  InitializeVehicleFaction_S: method(componentSource, 'InitializeVehicleFaction_S', ['GetPreset'])
};
enabled = new Set(['us_apc', 'us_apc_2', 'us_heli', 'rhs_apc', 'rhs_apc_2', 'rhs_heli', 'rhs_gunship']);
assert.equal(selected.ResolveVehicle(1, 'us_apc'), 'rhs_apc', 'legacy source is a type hint; selected army supplies the hull');
assert.equal(selected.ResolveVehicle(2, 'us_apc'), 'us_apc', 'roles query separate presets');
assert.equal(selected.PickVehicle(1, 4), 'rhs_apc_2', 'random selection covers multiple hulls');
assert.equal(selected.PickVehicle(1, 4, 3), 'rhs_apc_2', 'stable ordinal wraps within the type');
enabled.delete('rhs_apc'); enabled.delete('rhs_apc_2');
assert.equal(selected.ResolveVehicle(1, 'us_apc'), '', 'disabled army vehicles never fall back to source army');
assert.equal(selected.GetVehicleType('us_apc'), 4, 'type inference ignores content availability');
assert.equal(selected.ResolveVehicle(1, 'unknown'), '', 'unregistered legacy hints fail closed');
replacements.rhs_apc = 'replacement_apc'; enabled.add('replacement_apc');
assert.equal(selected.PickVehicle(1, 4, 0), 'replacement_apc', 'enabled pack replacement is the spawned prefab');
assert.equal(selected.PickVehicle(99, 4), '', 'missing preset fails closed');
const profile = {
  m_eRole: 1, m_eType: forceTypes.MOTORIZED, m_eVehicleType: 4, m_eEscortVehicleType: 13,
  m_sVehicle: 'us_apc', m_sEscortVehicle: '', m_iEscortCount: 1,
  PickPresetVehicle: method(profileSource, 'PickPresetVehicle'),
  HasPresetVehicles: method(profileSource, 'HasPresetVehicles', ['PickPresetVehicle'])
};
const before = randomCalls;
assert(profile.HasPresetVehicles()); assert.equal(randomCalls, before, 'planning does not consume random selections');
profile.m_eType = forceTypes.AIRBORNE; profile.m_eVehicleType = 12;
assert(profile.HasPresetVehicles()); enabled.delete('rhs_gunship');
assert(!profile.HasPresetVehicles(), 'escorted airborne needs both types');
profile.m_iEscortCount = 0; assert(profile.HasPresetVehicles(), 'transport-only ignores escort availability');
profile.m_eVehicleType = -1; assert.equal(profile.PickPresetVehicle(false, 0), 'replacement_apc', 'legacy profile derives requested type');
const entity = prefab => {
  let faction = 'MOD_NATIVE';
  return {prefab, GetFaction: () => faction, FindComponent: () => ({GetAffiliatedFactionKey: () => faction, SetAffiliatedFactionByKey: key => {faction = key;}})};
};
const hull = entity('replacement_apc');
assert(selected.InitializeVehicleFaction_S(hull, 1)); assert.equal(hull.GetFaction(), 'ROLE_1', 'modded native faction joins selected campaign role');
const foreign = entity('us_apc'); assert(!selected.InitializeVehicleFaction_S(foreign, 1)); assert.equal(foreign.GetFaction(), 'MOD_NATIVE');
server = false; assert(!selected.InitializeVehicleFaction_S(hull, 1)); server = true;
const garrison = {
  m_eRosterRole: 1, m_aExtraParkedVehicleTypes: [12], m_aExtraParkedVehicles: ['us_heli'],
  GetExtraParkedCount: () => 1,
  PresetVehicle: method(garrisonSource, 'PresetVehicle'),
  PresetVehicleOfType: method(garrisonSource, 'PresetVehicleOfType'),
  ParkedPrefab: method(garrisonSource, 'ParkedPrefab', ['PresetVehicle', 'PresetVehicleOfType', 'GetExtraParkedCount']),
  StaticPrefab: method(garrisonSource, 'StaticPrefab', ['PresetVehicle', 'PresetVehicleOfType']),
  MannedPrefab: method(garrisonSource, 'MannedPrefab', ['PresetVehicle', 'PresetVehicleOfType'])
};
const catalogue = {m_eParkedVehicleType: 4, m_eStaticWeaponType: 15, m_aStaticWeaponTypes: [16], m_aMannedVehicleTypes: [4], m_aMannedVehicles: ['us_apc']};
assert.equal(garrison.ParkedPrefab(catalogue, 1), 'rhs_heli', 'saved extra parked ordinal binds selected preset helicopter');
assert.equal(garrison.ParkedPrefab(catalogue, 0), 'replacement_apc');
assert.equal(garrison.MannedPrefab(catalogue, 0), 'replacement_apc');
assert.equal(garrison.StaticPrefab(catalogue, 0), '', 'missing static type omitted rather than replaced by other army');
const savedSlots = [
  {m_iKind: 2, m_iVariant: 1, m_sPrefab: 'us_heli', m_bLost: true, m_fHealth: 0, m_bPlaced: true},
  {m_iKind: 3, m_iVariant: 0, m_sPrefab: 'us_apc', m_bLost: false, m_fHealth: 0.4, m_bPlaced: true},
  {m_iKind: 1, m_iVariant: 0, m_sPrefab: 'old_static', m_bLost: false, m_fHealth: 0.8, m_bPlaced: true}
];
const bind = method(garrisonSource, 'BindAssets_S', ['CharacterPrefab', 'StaticPrefab', 'ParkedPrefab', 'MannedPrefab']);
Object.assign(garrison, {m_aSlots: savedSlots, CharacterPrefab: () => 'role_character'});
Object.assign(catalogue, {m_eRole: 1, m_sFactionKey: 'ROLE_1', m_sAssetFactionKey: 'US', MannedVariantCount: () => 1});
env.Math.Clamp = (value, min, max) => Math.max(min, Math.min(max, value));
assert(bind.call(garrison, catalogue));
assert.deepEqual(savedSlots.map(s => s.m_sPrefab), ['rhs_heli', 'replacement_apc', '']);
assert.deepEqual(savedSlots.map(s => [s.m_iKind, s.m_iVariant, s.m_bLost, s.m_fHealth]), [[2, 1, true, 0], [3, 0, false, 0.4], [1, 0, false, 0.8]], 'save binding preserves kinds, variants, casualties and damage');
assert(savedSlots.every(s => !s.m_bPlaced), 'new hull placements are revalidated on restore');
const filter = method(read('Scripts/Game/Operations/AROR_OperationExecutorComponent.c'), 'GetAvailableProfiles');
const available = [];
filter.call({m_aForceProfiles: [null, {HasPresetVehicles: () => false}, profile]}, available);
assert.deepEqual(available, [profile], 'runtime planning omits equipment-incompatible profiles');
for (const file of ['Worlds/AROR_Everon/AROR_Everon_Layers/Managers.layer', 'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer']) {
  const source = read(file);
  assert.equal([...source.matchAll(/m_sVehicle "/g)].length, [...source.matchAll(/m_eVehicleType /g)].length);
  assert.equal([...source.matchAll(/m_sEscortVehicle "/g)].length, [...source.matchAll(/m_eEscortVehicleType /g)].length);
}
console.log('Preset vehicle selection, content filtering, role assignment, saved variants and runtime availability checks passed. Workbench validation required.');
