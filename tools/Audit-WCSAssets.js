// Read-only config/resource audit. It models ID-based addon config overrides, not native resource loading.
const fs = require('fs'), path = require('path'), crypto = require('crypto');
const root = path.resolve(__dirname, '..');
const core = path.join(root, 'AROpenResistance/Open Resistance');
const rhs = path.join(root, 'OptionalAddons/OpenResistanceRHSCompat/OpenResistanceRHSCompat');
const wcs = path.join(root, 'OptionalAddons/OpenResistanceWCSCompat');
const combined = path.join(root, 'OptionalAddons/OpenResistanceWCSRHSCompat/OpenResistanceWCSRHSCompat');

function parseConfig(text, source = '') {
  const tokens = [];
  for (const m of text.matchAll(/\/\/[^\r\n]*|"(?:\\.|[^"\\])*"|[{}]|[^\S\r\n]+|\r?\n|[^\s{}"]+/g)) {
    const t = m[0];
    if (/^\/\//.test(t) || /^[^\S\r\n]+$/.test(t)) continue;
    tokens.push(t.startsWith('"') ? { value: t.slice(1, -1).replace(/\\"/g, '"').replace(/\\\\/g, '\\') } : { value: t });
  }
  let i = 0;
  function list(nested) {
    const nodes = [];
    while (i < tokens.length) {
      if (/^\r?\n$/.test(tokens[i].value)) { i++; continue; }
      if (tokens[i].value === '}') { if (!nested) throw Error('Unexpected closing brace: ' + source); i++; return nodes; }
      const head = [];
      while (i < tokens.length && !['{', '}'].includes(tokens[i].value) && !/^\r?\n$/.test(tokens[i].value)) head.push(tokens[i++].value);
      let next = i;
      while (next < tokens.length && /^\r?\n$/.test(tokens[next].value)) next++;
      if (tokens[next]?.value === '{') { i = next + 1; nodes.push({ head, children: list(true), source }); }
      else if (head.length) nodes.push({ head, children: null, source });
      else throw Error('Unrecognized config syntax: ' + source);
    }
    if (nested) throw Error('Unclosed config block: ' + source);
    return nodes;
  }
  return list(false);
}
const field = (node, name) => node?.children?.find(n => n.head[0] === name);
const value = (node, name, fallback = '') => field(node, name)?.head[1] ?? fallback;
const values = node => (node?.children || []).flatMap(n => n.head);
const objects = (node, name) => field(node, name)?.children?.filter(n => n.children !== null) || [];
const clone = node => ({ ...node, head: [...node.head], children: node.children?.map(clone) ?? null });
const identity = node => node.head.find(v => /^\{[A-Fa-f0-9]{16}\}$/.test(v));
function mergeConfig(base, patch) {
  const result = clone(base);
  for (const change of patch.children || []) {
    const id = identity(change);
    // Named fields merge; array objects merge only when they have an existing instance ID.
    const appendObject = base.head[0].startsWith('m_a') && change.children !== null && !id;
    const index = appendObject ? -1 : result.children.findIndex(n => id ? identity(n)?.toUpperCase() === id.toUpperCase() && n.head[0] === change.head[0] : n.head[0] === change.head[0]);
    if (index < 0) result.children.push(clone(change));
    else if (change.children && result.children[index].children && !(change.head[0].startsWith('m_a') && change.children.every(n => n.children === null))) result.children[index] = mergeConfig(result.children[index], change);
    else result.children[index] = clone(change);
  }
  return result;
}
const load = file => parseConfig(fs.readFileSync(file, 'utf8'), path.relative(root, file).replaceAll('\\', '/'))[0];
function walk(node, visit) { visit(node); for (const child of node.children || []) walk(child, visit); }
function resourceReferences(node) {
  const result = [];
  walk(node, n => { for (const token of n.head) {
    const m = token.match(/^\{([A-Fa-f0-9]{16})\}(.+\.et)$/);
    if (m) result.push({ guid: m[1].toUpperCase(), resource: m[2], source: n.source });
  } });
  return result;
}
function projects(addonsRoot, baseGameRoot) {
  const result = new Map();
  function add(directory, repository = false) {
    const project = path.join(directory, 'addon.gproj');
    if (!fs.existsSync(project)) return;
    const text = fs.readFileSync(project, 'utf8'), id = text.match(/\bGUID "([A-Fa-f0-9]+)"/)?.[1]?.toUpperCase();
    if (!id) return;
    result.set(id, { id, directory, repository, name: text.match(/\bTITLE "([^"]+)"/)?.[1] || path.basename(directory),
      dependencies: [...(text.match(/Dependencies\s*\{([^}]+)\}/)?.[1] || '').matchAll(/"([A-Fa-f0-9]{16})"/g)].map(m => m[1].toUpperCase()) });
  }
  if (fs.existsSync(addonsRoot)) for (const e of fs.readdirSync(addonsRoot, { withFileTypes: true })) if (e.isDirectory()) add(path.join(addonsRoot, e.name));
  for (const directory of [core, rhs, wcs, combined]) add(directory, true);
  result.set('58D0FB3206B6F859', { id: '58D0FB3206B6F859', directory: baseGameRoot, name: 'ArmaReforger', dependencies: [] });
  return result;
}
function audit(options = {}) {
  const addonsRoot = options.addonsRoot || path.join(process.env.USERPROFILE, 'OneDrive/Documents/My Games/ArmaReforger/addons');
  const baseGameRoot = options.baseGameRoot || 'D:/SteamLibrary/steamapps/common/Arma Reforger/addons/data';
  const inventory = projects(addonsRoot, baseGameRoot), closure = new Set(), missingDependencies = new Set();
  function include(id) { if (closure.has(id)) return; closure.add(id); const p = inventory.get(id); if (!p) missingDependencies.add(id); else p.dependencies.forEach(include); }
  include('6AC468457E10B1AE');
  const packNodes = [load(path.join(core, 'Configs/ContentPacks/BaseGame/AROR_ContentPack_BaseGame.conf')),
    load(path.join(rhs, 'Configs/ContentPacks/AROR_ContentPack_RHS.conf')), load(path.join(wcs, 'Configs/ContentPacks/WCS/AROR_ContentPack_WCS.conf'))];
  const enabledEntries = [], skippedGroups = [], replacements = new Map();
  for (const pack of packNodes) for (const group of objects(pack, 'm_aGroups')) {
    const required = values(field(group, 'm_aRequiredAddons'));
    if (required.some(id => !closure.has(id.toUpperCase()))) { skippedGroups.push({ pack: value(pack, 'm_sId'), group: value(group, 'm_sLabel'), required }); continue; }
    for (const entry of objects(group, 'm_aEntries')) {
      const prefab = value(entry, 'm_sPrefab');
      enabledEntries.push({ pack: value(pack, 'm_sId'), group: value(group, 'm_sLabel'), prefab, type: value(entry, 'm_eType'),
        category: entry.head[0], readiness: Number(value(entry, 'm_iMinimumReadinessLevel', 1)), node: entry });
      if (entry.head[0] === 'AROR_ContentPackVehicle' && !enabledEntries.at(-1).type) enabledEntries.at(-1).type = 'MOTOR_CAR';
      for (const original of values(field(entry, 'm_aReplaces'))) if (!replacements.has(original)) replacements.set(original, prefab);
    }
  }
  function resolve(prefab) { let result = prefab; for (let n = 0; n < 8 && replacements.has(result); n++) result = replacements.get(result); return result; }
  const catalogue = new Map();
  for (const entry of enabledEntries) if (!replacements.has(entry.prefab) && !catalogue.has(entry.prefab)) catalogue.set(entry.prefab, entry);
  const presetNames = ['AROR_Preset_RHS_USAF_FORECON.conf', 'AROR_Preset_RHS_AFRF_MSV_VPKO3_EMR.conf'];
  const presetNodes = presetNames.map(name => mergeConfig(load(path.join(rhs, 'Configs/FactionPresets', name)), load(path.join(combined, 'Configs/FactionPresets', name))));
  const refs = new Map();
  for (const node of [...presetNodes, ...packNodes]) for (const ref of resourceReferences(node)) {
    const key = ref.guid + ref.resource.toLowerCase();
    if (!refs.has(key)) refs.set(key, { ...ref, sources: [], owners: [], renamed: [] });
    if (!refs.get(key).sources.includes(ref.source)) refs.get(key).sources.push(ref.source);
  }
  const refList = [...refs.values()];
  let databases = 0;
  for (const project of inventory.values()) {
    const database = path.join(project.directory, 'resourceDatabase.rdb');
    if (!fs.existsSync(database)) continue;
    databases++;
    const buffer = fs.readFileSync(database), text = buffer.toString('latin1'), lower = text.toLowerCase();
    for (const ref of refList) {
      const owner = { id: project.id, name: project.name, inDependencyTree: closure.has(project.id) };
      const at = lower.indexOf(ref.resource.toLowerCase() + '\0');
      if (at >= 0 && at + ref.resource.length + 15 <= buffer.length) {
        const actualGuid = buffer.readBigUInt64LE(at + ref.resource.length + 7).toString(16).padStart(16, '0').toUpperCase();
        ref.owners.push({ ...owner, guid: actualGuid, exact: actualGuid === ref.guid });
      }
      if (at < 0 || !ref.owners.at(-1)?.exact) {
        const guidBytes = Buffer.alloc(8); guidBytes.writeBigUInt64LE(BigInt('0x' + ref.guid));
        let offset = buffer.indexOf(guidBytes);
        while (offset >= 0) {
          const end = offset - 7;
          if (end > 0 && buffer[end] === 0) {
            const prefix = text.slice(Math.max(0, end - 1024), end), start = prefix.toLowerCase().lastIndexOf('prefabs/');
            const candidate = start >= 0 ? prefix.slice(start) : '';
            if (candidate.endsWith('.et') && !/[\x00-\x1f]/.test(candidate)) ref.renamed.push({ ...owner, resource: candidate });
          }
          offset = buffer.indexOf(guidBytes, offset + 8);
        }
      }
    }
  }
  for (const ref of refList) ref.status = ref.owners.some(o => o.exact && o.inDependencyTree) ? 'exact' :
    ref.renamed.some(o => o.inDependencyTree) ? 'same_guid_different_path' : ref.owners.some(o => o.exact) ? 'outside_dependency_tree' : 'unresolved';
  const presets = presetNodes.map(preset => {
    const units = objects(preset, 'm_aUnits').map(unit => ({ type: value(unit, 'm_eType', '<missing>'), prefab: value(unit, 'm_sPrefab'),
      loadouts: objects(unit, 'm_aLoadouts').map(kit => ({ tier: Number(value(kit, 'm_iMinimumReadiness', 1)), weight: Number(value(kit, 'm_iWeight', 1)),
        references: resourceReferences(kit).map(r => ({ guid: r.guid, resource: r.resource, status: refs.get(r.guid + r.resource.toLowerCase())?.status })) })) }));
    const vehicles = objects(preset, 'm_aVehicles').map(vehicle => {
      const prefab = resolve(value(vehicle, 'm_sPrefab')), offered = catalogue.get(prefab);
      return { type: value(vehicle, 'm_eType', 'MOTOR_CAR'), prefab, offered: !!offered, pack: offered?.pack, group: offered?.group, catalogueType: offered?.type, readiness: offered?.readiness };
    });
    return { id: value(preset, 'm_sId'), roles: values(field(preset, 'm_aUsableRoles')), units, vehicles,
      missingCoreTypes: ['RIFLEMAN', 'AUTORIFLEMAN', 'MACHINEGUNNER', 'GRENADIER', 'MEDIC', 'MARKSMAN', 'SNIPER', 'AT', 'AA', 'ENGINEER', 'RADIO_OPERATOR', 'TEAMLEADER', 'CREW', 'PILOT', 'SQUADLEADER', 'LAT'].filter(t => !units.some(u => u.type === t && u.loadouts.length)) };
  });
  const sourceFiles = new Set([...presetNodes, ...packNodes].flatMap(n => { const names = []; walk(n, child => names.push(child.source)); return names; }));
  const defaultsPath = path.join(combined, 'Prefabs/GameModes/AROR_GameMode_Base.et');
  const defaultComponent = objects(load(defaultsPath), 'components').find(n => n.head[0] === 'AROR_FactionPresetsComponent');
  sourceFiles.add(path.relative(root, defaultsPath).replaceAll('\\', '/'));
  const report = { generatedAt: new Date().toISOString(), nativeVerified: false, note: 'ID-based text override model and installed resource indexes; no native resource or inventory loading. Catalogue models BASE_GAME + RHS + WCS enabled with their dependency-tree groups. Missing declared dependencies still prevent certification of addon loading.',
    sourceHashFormat: 'SHA-256 of UTF-8 text with CRLF normalized to LF',
    sourceHashes: Object.fromEntries([...sourceFiles].sort().map(file => [file, crypto.createHash('sha256').update(fs.readFileSync(path.join(root, file), 'utf8').replace(/\r\n/g, '\n')).digest('hex')])),
    addonDefaults: { occupiers: value(defaultComponent, 'm_sDefaultOccupiers'), invaders: value(defaultComponent, 'm_sDefaultInvaders') },
    dependencyCount: closure.size, databases, missingDependencies: [...missingDependencies], skippedGroups,
    missingDependencyParents: Object.fromEntries([...missingDependencies].map(id => [id, [...inventory.values()].filter(p => closure.has(p.id) && p.dependencies.includes(id)).map(p => ({ id: p.id, name: p.name }))])),
    catalogue: { entries: catalogue.size, replacements: replacements.size, byPack: Object.fromEntries(packNodes.map(p => [value(p, 'm_sId'), enabledEntries.filter(e => e.pack === value(p, 'm_sId')).length])) },
    presets, references: refList, summary: { references: refList.length, statuses: Object.fromEntries(['exact', 'same_guid_different_path', 'outside_dependency_tree', 'unresolved'].map(s => [s, refList.filter(r => r.status === s).length])),
      unavailablePresetVehicles: presets.flatMap(p => p.vehicles.filter(v => !v.offered).map(v => ({ preset: p.id, ...v }))),
      mismatchedVehicleTypes: presets.flatMap(p => p.vehicles.filter(v => v.offered && v.type !== v.catalogueType).map(v => ({ preset: p.id, ...v }))) } };
  return report;
}
if (require.main === module) {
  const report = audit();
  // Keep checked-in evidence reviewable. audit() still returns every per-resource result to callers.
  const saved = { ...report, exactReferencesOmitted: report.references.filter(r => r.status === 'exact').length,
    references: report.references.filter(r => r.status !== 'exact'),
    presets: report.presets.map(p => ({ ...p, units: p.units.map(u => ({ ...u, loadouts: u.loadouts.map(k => ({
      tier: k.tier, weight: k.weight, referenceCount: k.references.length,
      statuses: Object.fromEntries([...new Set(k.references.map(r => r.status))].map(s => [s, k.references.filter(r => r.status === s).length]))
    })) })) })) };
  const output = process.argv[2] || 'docs/validation/WCS_ASSET_AUDIT_2026-11-07.json';
  fs.writeFileSync(path.resolve(root, output), JSON.stringify(saved, null, 2) + '\n');
  console.log(JSON.stringify({ ...report.summary, missingDependencies: report.missingDependencies, skippedGroups: report.skippedGroups,
    catalogue: report.catalogue, presets: report.presets.map(p => ({ id: p.id, units: p.units.length, vehicles: p.vehicles.length, missingCoreTypes: p.missingCoreTypes })),
    unresolved: report.references.filter(r => r.status === 'unresolved').map(r => ({ guid: r.guid, resource: r.resource, owners: r.owners })) }, null, 2));
}
module.exports = { parseConfig, mergeConfig, field, value, values, objects, resourceReferences, audit };
