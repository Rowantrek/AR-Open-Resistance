// Execute the actual preset/default/loadout selectors against effective addon configs.
// Native ResourceName normalization, prefab loading and inventory compatibility remain Workbench checks.
const fs = require('fs'), path = require('path'), assert = require('assert/strict'), crypto = require('crypto');
const { parseConfig, mergeConfig, field, value, values, objects } = require('./Audit-WCSAssets');
const root = path.resolve(__dirname, '..'), core = 'AROpenResistance/Open Resistance/';
const rhs = 'OptionalAddons/OpenResistanceRHSCompat/OpenResistanceRHSCompat/';
const wcs = 'OptionalAddons/OpenResistanceWCSCompat/';
const compat = 'OptionalAddons/OpenResistanceWCSRHSCompat/OpenResistanceWCSRHSCompat/';
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const config = file => parseConfig(read(file), file)[0];
let assertions = 0;
function check(condition, message) { assertions++; assert(condition, message); }
function equal(actual, expected, message) { assertions++; assert.deepEqual(actual, expected, message); }
for (const [name, fn] of Object.entries({
  Count() { return this.length; }, IsEmpty() { return !this.length; }, Contains(v) { return this.includes(v); },
  Insert(v) { this.push(v); }, Clear() { this.length = 0; }, GetRandomElement() { return this[Math.floor(rollFraction * this.length)]; }
})) Object.defineProperty(Array.prototype, name, { value: fn, configurable: true });
Object.defineProperty(String.prototype, 'IsEmpty', { value() { return !this.length; }, configurable: true });
const roles = { RESISTANCE: 0, OCCUPIERS: 1, INVADERS: 2, CIVILIANS: 3 };
const category = { UNITS: 0, VEHICLES: 1 };
function enumValues(name) {
  const text = read(core + 'Scripts/Game/Factions/Presets/' + name + '.c').replace(/\/\/[^\r\n]*/g, '');
  return Object.fromEntries(text.match(/\{([^}]+)\}/)[1].split(',').map(s => s.trim()).filter(Boolean).map((s, i) => [s, i]));
}
const vehicleTypes = enumValues('AROR_EVehicleType'), unitTypes = enumValues('AROR_EUnitType');
let server = true, bumps = 0, rollFraction = 0;
let offered = new Set(), replacements = new Map();
const env = {
  AROR_EFactionRole: roles, AROR_EFactionPresetCategory: category, AROR_EUnitType: unitTypes,
  AROR_ContentPacksComponent: { Resolve: p => replacements.get(p) || p, IsOffered: p => offered.has(p) },
  AROR_FactionPresetUnit: { Cast: v => v },
  AROR_FactionRoles: { GetAll: a => a.push(...Object.values(roles)) },
  Replication: { IsServer: () => server, BumpMe: () => bumps++ },
  Math: { Mod: (a, b) => a % b, RandomInt: (a, b) => Math.floor(a + (b - a) * rollFraction) },
  int: { MAX: 2147483647 }, string: { Empty: '', Format: (s, ...a) => s.replace(/%(\d+)/g, (_, n) => a[n - 1]) },
  typename: { EnumToString: (e, v) => Object.keys(e).find(k => e[k] === v) }, Print() {}, LogLevel: { WARNING: 1, ERROR: 2 }
};
function method(source, name, members = []) {
  const match = source.match(new RegExp('^\\t[^\\r\\n]*\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  assert(match, 'Missing actual method ' + name);
  const args = match[1].replace(/\bnotnull /g, '').replace(/array<(?:ref )?\w+>\s+/g, '').replace(/\b\w+ (\w+)(?=\s*(?:=|,|$))/g, '$1');
  let body = match[2]
    .replace(/array<(?:ref )?\w+> (\w+) = \{\};/g, 'let $1 = [];')
    .replace(/foreach \((\w+) (\w+) : ([^)]+)\)/g, 'for (const $2 of $3)')
    .replace(/\bint (\w+);/g, 'let $1 = 0;')
    .replace(/\bstring (\w+);/g, 'let $1 = "";')
    .replace(/\b(?:int|string|ResourceName|AROR_\w+) (\w+) =/g, 'let $1 =')
    .replace(/(?<![.\w])m_\w+/g, name => 'this.' + name);
  for (const member of members) body = body.replace(new RegExp('(?<![.\\w])' + member + '\\(', 'g'), 'this.' + member + '(');
  return new Function(...Object.keys(env), `return function(${args}) {${body}}`)(...Object.values(env));
}
// Parser/merge fixtures guard the audit model against the sparse US override being read as a whole preset.
const fixture = text => parseConfig(text)[0];
const baseFixture = fixture('Root {\n m_sId "army"\n m_aUnits {\n Unit "{1111111111111111}" {\n m_eType RIFLEMAN\n m_sPrefab "{2222222222222222}Prefabs/A.et" // comment\n m_aKit {\n "a" "b"\n }\n }\n }\n}');
const patchFixture = fixture('Root {\n m_aUnits {\n Unit "{1111111111111111}" {\n m_sPrefab "{3333333333333333}Prefabs/B.et"\n m_aKit {\n "c"\n }\n }\n Unit {\n m_eType MEDIC\n }\n Unit {\n m_eType CREW\n }\n }\n}');
const mergedFixture = mergeConfig(baseFixture, patchFixture), fixtureUnits = objects(mergedFixture, 'm_aUnits');
equal(value(mergedFixture, 'm_sId'), 'army', 'sparse root inherits its identity');
equal(fixtureUnits.length, 3, 'unidentified array additions append');
equal(value(fixtureUnits[0], 'm_eType'), 'RIFLEMAN', 'scalar-only ID patch preserves other fields');
equal(value(fixtureUnits[0], 'm_sPrefab'), '{3333333333333333}Prefabs/B.et', 'quoted GUID braces remain one scalar');
equal(values(field(fixtureUnits[0], 'm_aKit')), ['c'], 'primitive list override replaces its list');
equal(value(objects(baseFixture, 'm_aUnits')[0], 'm_sPrefab'), '{2222222222222222}Prefabs/A.et', 'merge never mutates base tree');

const report = JSON.parse(read('docs/validation/WCS_ASSET_AUDIT_2026-11-07.json'));
for (const [file, hash] of Object.entries(report.sourceHashes)) {
  equal(crypto.createHash('sha256').update(read(file).replace(/\r\n/g, '\n')).digest('hex'), hash, 'resource audit must match current config: ' + file);
}
equal(report.summary.statuses, { exact: report.summary.references, same_guid_different_path: 0, outside_dependency_tree: 0, unresolved: 0 }, 'all audited prefab references match installed indexes in the declared dependency tree');
check(!report.nativeVerified, 'source/index checks never imply native loading');
const pack = config(wcs + 'Configs/ContentPacks/WCS/AROR_ContentPack_WCS.conf');
const catalogue = objects(pack, 'm_aGroups').flatMap(g => objects(g, 'm_aEntries'));
check(!catalogue.some(e => value(e, 'm_sPrefab').startsWith('{658B80A790C7EFA7}')), 'unrelated FTA Mi24 is absent');
for (const [guid, type] of Object.entries({
  AF4613B2B820E34D: 'STATIC_AA', '3DCACB48619AA029': 'STATIC_AA', '1ECCF96D673DC5D8': 'STATIC_AA',
  E8F0DA0C05A8FED4: 'STATIC_AT', '023236B2A390BA5B': 'STATIC_AT',
  '962C400354E4BD6C': 'LAUNCHER_AA', '3DF204EEE0534EF2': 'LAUNCHER_AA'
})) equal(value(catalogue.find(e => value(e, 'm_sPrefab').startsWith('{' + guid + '}')), 'm_eType'), type, 'catalogue classifies the weapon role: ' + guid);

const componentSource = read(core + 'Scripts/Game/Factions/Presets/AROR_FactionPresetsComponent.c');
const presetSource = read(core + 'Scripts/Game/Factions/Presets/AROR_FactionPreset.c');
const entrySource = read(core + 'Scripts/Game/Factions/Presets/AROR_FactionPresetEntries.c');
const kitSource = read(core + 'Scripts/Game/Factions/Presets/AROR_UnitLoadout.c');
const pickKit = method(kitSource, 'Pick');
function entry(node, cat) {
  return {
    m_sPrefab: value(node, 'm_sPrefab'), GetCategory: () => cat,
    GetTypeValue: () => (cat === category.VEHICLES ? vehicleTypes : unitTypes)[value(node, 'm_eType', cat === category.VEHICLES ? 'MOTOR_CAR' : 'RIFLEMAN')],
    IsKnownType() { return this.GetTypeValue() !== undefined; },
    GetPrefab: method(entrySource, 'GetPrefab'), GetListedPrefab: method(entrySource, 'GetListedPrefab'),
    HasPrefab() { return !!this.m_sPrefab; }, HasLoadouts: () => objects(node, 'm_aLoadouts').length > 0,
    PickLoadout: readiness => pickKit(objects(node, 'm_aLoadouts').map(k => ({
      m_iMinimumReadiness: Number(value(k, 'm_iMinimumReadiness', 1)), m_iWeight: Number(value(k, 'm_iWeight', 1)), node: k
    })), readiness)
  };
}
function preset(node) {
  const entries = { 0: objects(node, 'm_aUnits').map(n => entry(n, 0)), 1: objects(node, 'm_aVehicles').map(n => entry(n, 1)) };
  return {
    m_sId: value(node, 'm_sId'), GetId() { return this.m_sId; },
    IsUsableAs: role => values(field(node, 'm_aUsableRoles')).includes(Object.keys(roles).find(k => roles[k] === role)),
    CollectEntries: (cat, out) => out.push(...(entries[cat] || [])),
    IsOffered: method(presetSource, 'IsOffered'), GetEntries: method(presetSource, 'GetEntries', ['CollectEntries', 'IsOffered']),
    GetAllEntries: method(presetSource, 'GetAllEntries', ['CollectEntries', 'IsOffered']),
    GetUnit: method(presetSource, 'GetUnit', ['GetEntries'])
  };
}
const names = ['AROR_Preset_RHS_USAF_FORECON.conf', 'AROR_Preset_RHS_AFRF_MSV_VPKO3_EMR.conf'];
const nodes = names.map(name => mergeConfig(config(rhs + 'Configs/FactionPresets/' + name), config(compat + 'Configs/FactionPresets/' + name)));
const armies = nodes.map(preset);
const modeOverride = config(compat + 'Prefabs/GameModes/AROR_GameMode_Base.et');
equal(modeOverride.head, ['SCR_BaseGameMode', ':', '{49BA4199470D70DA}Prefabs/GameModes/AROR_GameMode_Base.et'], 'game-mode override explicitly inherits the underlying prefab');
const defaults = objects(modeOverride, 'components')[0];
equal(defaults.head, ['AROR_FactionPresetsComponent', '{5C71FA2147F19FB3}'], 'override targets the existing core component');
equal(defaults.children.map(n => n.head[0]).sort(), ['m_sDefaultInvaders', 'm_sDefaultOccupiers'], 'addon override changes only the requested defaults');
equal(config(core + 'Prefabs/GameModes/AROR_GameMode_Base.et.meta').children[0].head,
  config(compat + 'Prefabs/GameModes/AROR_GameMode_Base.et.meta').children[0].head, 'override reuses original resource identity');
check(read(core + 'Prefabs/GameModes/AROR_GameMode_Base.et').includes('AROR_FactionPresetsComponent "{5C71FA2147F19FB3}"'), 'core owns the overridden component ID');
const baseMode = config(core + 'Prefabs/GameModes/AROR_GameMode_Base.et');
const radioMode = config('OptionalAddons/OpenResistanceRadio/Prefabs/GameModes/AROR_GameMode_Base.et');
for (const overlays of [[modeOverride, radioMode], [radioMode, modeOverride]]) {
  const mergedMode = overlays.reduce(mergeConfig, baseMode), components = objects(mergedMode, 'components');
  check(components.some(c => c.head[0] === 'AROR_SquadManagerComponent'), 'overlay model preserves core squad component');
  check(components.some(c => c.head[0] === 'ORRadio_ManagerComponent'), 'overlay model preserves optional Radio component');
  equal(value(components.find(c => c.head[0] === 'AROR_FactionPresetsComponent'), 'm_sDefaultOccupiers'), 'RHS_USAF_FORECON', 'unrelated Radio overlay preserves enemy defaults in either modelled order');
}
check(componentSource.includes('[Attribute("US_ARMY"') && componentSource.includes('[Attribute("USSR_ARMY"'), 'core-only defaults remain available');
check(read(core + 'Scripts/Game/GameFlow/UI/AROR_SetupDraft.c').includes('SetPresetId(role, presets.GetDefaultPresetId(role))'), 'new campaign setup reads configured role defaults');
const existing = ['FIA', 'US_ARMY', 'USSR_ARMY', 'EVERON_CIV'].map((id, role) => ({ GetId: () => id, IsUsableAs: r => r === role }));
const selected = { 0: 'FIA', 1: armies[0].GetId(), 2: armies[1].GetId(), 3: 'EVERON_CIV' };
const component = {
  m_sDefaultResistance: 'FIA', m_sDefaultOccupiers: value(defaults, 'm_sDefaultOccupiers'),
  m_sDefaultInvaders: value(defaults, 'm_sDefaultInvaders'), m_sDefaultCivilians: 'EVERON_CIV',
  m_aPresets: [...existing, ...armies], m_OnPresetsChanged: { Invoke() {} }, LogPresets() {}, LogFallback() {},
  GetPreset(role) { return this.FindPreset(selected[role]); }, SetRolePreset(role, id) { selected[role] = id; },
  GetReadiness: () => 1, GetRoleBasePrefab: role => role === roles.CIVILIANS ? '' : 'role-shell-' + role
};
env.AROR_FactionPresetsComponent = { GetInstance: () => component };
env.AROR_EVehicleType = vehicleTypes;
for (const [name, members] of Object.entries({
  FindPreset: [], GetDefaultPresetId: [], ResolvePresetId: ['FindPreset', 'GetDefaultPresetId'],
  SetPresets_S: ['SetRolePreset', 'ResolvePresetId', 'LogPresets'], RestorePresets_S: ['SetPresets_S'],
  IsUsableUnit: ['GetRoleBasePrefab'], FindUnit: ['GetPreset', 'IsUsableUnit', 'FindPreset', 'GetDefaultPresetId', 'LogFallback'],
  GetUnitLoadout: ['FindUnit', 'GetReadiness'], GetSpawnPrefab: ['GetRoleBasePrefab', 'FindUnit'], PickVehicle: ['GetPreset']
})) component[name] = method(componentSource, name, members);
component.SetPresets_S([]);
equal(Object.values(selected), ['FIA', armies[0].GetId(), armies[1].GetId(), 'EVERON_CIV'], 'new campaign gets WCS/RHS enemies and existing Resistance/civilian defaults');
component.RestorePresets_S(['FIA', 'US_ARMY', 'USSR_ARMY', 'EVERON_CIV']);
equal(Object.values(selected), ['FIA', 'US_ARMY', 'USSR_ARMY', 'EVERON_CIV'], 'valid saved vanilla choices survive addon defaults');
component.RestorePresets_S(['FIA', 'REMOVED_ADDON_ARMY', armies[1].GetId(), 'EVERON_CIV']);
equal(selected[1], armies[0].GetId(), 'missing saved enemy selection falls back to configured addon default');
server = false; const before = bumps;
component.SetPresets_S(['FIA', 'US_ARMY', 'USSR_ARMY', 'EVERON_CIV']);
equal(bumps, before, 'client cannot change or publish campaign faction choices');
equal(selected[1], armies[0].GetId(), 'client mutation leaves authoritative selection intact'); server = true;
const formations = [...new Set([...read(core + 'Scripts/Game/Factions/Presets/AROR_Formations.c').matchAll(/outUnits.Insert\(AROR_EUnitType\.(\w+)\)/g)].map(m => m[1]))];
for (const [index, army] of armies.entries()) {
  const role = index + 1; selected[role] = army.GetId();
  equal(objects(nodes[index], 'm_aUnits').length, 15, 'effective preset preserves inherited infantry roster');
  for (const type of [...formations, 'CREW']) {
    const unit = component.FindUnit(role, unitTypes[type]);
    equal(unit, army.GetUnit(unitTypes[type]), 'formation/crew uses its selected army without a stand-in: ' + army.GetId() + ':' + type);
    for (let readiness = 1; readiness <= 5; readiness++) for (const fraction of [0, 0.5, 0.999999]) {
      rollFraction = fraction;
      const kit = unit.PickLoadout(readiness);
      check(kit && objects(kit.node, 'm_aClothing').length && objects(kit.node, 'm_aWeapons').length, 'all readiness rolls produce clothing and weapons: ' + army.GetId() + ':' + type);
      const expectedTier = Math.max(...objects(objects(nodes[index], 'm_aUnits').find(n => value(n, 'm_eType') === type), 'm_aLoadouts').map(k => Number(value(k, 'm_iMinimumReadiness', 1))).filter(t => t <= readiness));
      equal(kit.m_iMinimumReadiness, expectedTier, 'actual selector chooses the highest eligible readiness tier');
    }
  }
  equal(component.GetSpawnPrefab(role, unitTypes.RIFLEMAN), 'role-shell-' + role, 'military role shell is intentionally equipped after spawning');
}
equal(component.FindUnit(roles.INVADERS, unitTypes.PILOT), armies[1].GetUnit(unitTypes.RIFLEMAN), 'missing unit falls back inside the configured WCS/RHS army');
offered = new Set(report.presets.flatMap(p => p.vehicles.filter(v => v.offered).map(v => v.prefab)));
const usSupply = component.PickVehicle(roles.OCCUPIERS, vehicleTypes.SUPPLY_TRUCK, 0);
check(usSupply.startsWith('{ACCF3C5D1629AF7F}'), 'motorized US QRF and supply missions can select the WCS FMTV');
check(component.PickVehicle(roles.OCCUPIERS, vehicleTypes.STATIC_AA, 0).startsWith('{3DCACB48619AA029}'), 'US static AA uses the offered WCS ZU23 counterpart');
env.AROR_ContentPacksComponent.IsAvailableAtReadiness = () => true;
const missionVehicle = method(read(core + 'Scripts/Game/Missions/AROR_MissionBoardComponent.c'), 'GetVehiclePrefab_S');
equal(missionVehicle(roles.OCCUPIERS, vehicleTypes.SUPPLY_TRUCK), usSupply, 'actual supply/convoy mission selector uses the WCS FMTV');
env.AROR_ContentPacksComponent.IsAvailableAtReadiness = () => false;
equal(missionVehicle(roles.OCCUPIERS, vehicleTypes.SUPPLY_TRUCK), usSupply, 'existing mission readiness fallback stays inside the selected army');
for (const [index, armyReport] of report.presets.entries()) {
  for (const type of ['ARMED_CAR', 'APC', 'SUPPLY_TRUCK', 'HELI_TRANSPORT', 'HELI_ATTACK']) {
    const prefab = component.PickVehicle(index + 1, vehicleTypes[type], 0);
    check(prefab && armyReport.vehicles.some(v => v.type === type && v.prefab === prefab && v.offered), 'required force type has an enabled hull: ' + armyReport.id + ':' + type);
  }
  const enabledAircraft = armyReport.vehicles.filter(v => v.offered && v.type === 'HELI_ATTACK');
  check(enabledAircraft.some(v => v.pack === 'WCS'), 'selected army includes enabled WCS gunships');
}
const managers = parseConfig(read(core + 'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer'));
const profiles = [];
function visit(n) { if (n.head[0] === 'AROR_ForceProfile') profiles.push(n); for (const c of n.children || []) visit(c); }
managers.forEach(visit);
check(profiles.length >= 10, 'Chicken force profiles were discovered');
for (const profile of profiles) for (const key of ['m_eVehicleType', 'm_eEscortVehicleType']) {
  const type = value(profile, key); if (!type) continue;
  check(component.PickVehicle(Number(value(profile, 'm_eRole')), vehicleTypes[type], 0), 'Chicken force profile can obtain requested army vehicle: ' + value(profile, 'm_sId') + ':' + type);
}
const replacement = usSupply + '.replacement'; replacements.set(usSupply, replacement); offered.add(replacement);
equal(component.PickVehicle(roles.OCCUPIERS, vehicleTypes.SUPPLY_TRUCK, 0), replacement, 'actual entry getter carries content replacement through vehicle selection');
offered.delete(replacement); offered.delete(usSupply);
equal(component.PickVehicle(roles.OCCUPIERS, vehicleTypes.SUPPLY_TRUCK, 0), '', 'disabled selected-army supply hull does not fall back to another army');
equal(missionVehicle(roles.OCCUPIERS, vehicleTypes.SUPPLY_TRUCK), '', 'mission selector also fails closed when selected army vehicle is disabled');
replacements.clear(); offered.clear();
equal(component.PickVehicle(roles.OCCUPIERS, vehicleTypes.HELI_ATTACK), '', 'disabled catalogue yields no gunship');
check(armies[0].GetUnit(unitTypes.RIFLEMAN).HasLoadouts(), 'disabling shops does not erase authored infantry kits');
// Every loadout prefab category goes through replacement resolution, including nested contents.
replacements.set('original', 'wcs-replacement');
for (const source of [entrySource, ...kitSource.split(/\r?\n(?=class )/).filter(s => s.includes('ResourceName GetPrefab()'))]) {
  equal(method(source, 'GetPrefab').call({ m_sPrefab: 'original' }), 'wcs-replacement', 'actual prefab getter resolves enabled replacement');
}
equal(method(kitSource, 'GetMagazine').call({ m_sMagazine: 'original' }), 'wcs-replacement', 'loaded magazines resolve replacements');
const attachments = [];
method(kitSource, 'GetAttachments').call({ m_aAttachments: ['original', ''] }, attachments);
equal(attachments, ['wcs-replacement'], 'attachments resolve replacements and skip empty entries');
// Execute Imports.Build with the authored WCS catalogue and native offer/economy services mocked.
env.AROR_ContentPacksComponent.GetInstance = () => ({ GetEnabledEntries: (cat, out) => out.push(...catalogue
  .filter(n => n.head[0] === 'AROR_ContentPackVehicle').map(n => ({
    GetTypeValue: () => vehicleTypes[value(n, 'm_eType', 'MOTOR_CAR')],
    CanResistancePurchase: () => value(n, 'm_bResistanceCanPurchase', '1') !== '0', prefab: value(n, 'm_sPrefab')
  }))) });
env.AROR_ImportShop = { CreateOffer: (entry, category) => ({ prefab: entry.prefab, category }) };
const importsSource = read(core + 'Scripts/Game/BlackMarket/AROR_Imports.c');
const imports = { GetCategoryOf: method(importsSource, 'GetCategoryOf'), Build: method(importsSource, 'Build', ['GetCategoryOf']) };
const offers = []; imports.Build(offers);
check(offers.some(o => o.prefab.startsWith('{72C04A6EABC12143}') && o.category === 3), 'canonical WCS AH6 appears in the helicopter imports category');
check(offers.some(o => o.prefab === usSupply && o.category === 1), 'WCS supply FMTV appears in truck imports');
check(!offers.some(o => /SightDebug|658B80A790C7EFA7/.test(o.prefab)), 'test hulls and the unrelated FTA hull are not sold by imports');
console.log(JSON.stringify({ assertions, effectivePresets: armies.map(p => p.GetId()), forceProfiles: profiles.length,
  note: 'Actual Enforce selector bodies with native services mocked; ID-based config override model and installed resource indexes. Workbench compile/loading/gameplay still required.' }));
