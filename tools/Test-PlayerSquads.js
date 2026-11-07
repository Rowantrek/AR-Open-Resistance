// Execute the actual recruitment methods with native entities/replication mocked.
// This is a server policy/lifecycle check, not an Enforce compiler or engine simulation.
const fs = require('fs'), path = require('path'), assert = require('assert');
const root = path.resolve(__dirname, '..');
const addon = path.join(root, 'AROpenResistance/Open Resistance');
const read = p => fs.readFileSync(path.join(addon, p), 'utf8');
const source = read('Scripts/Game/Squads/AROR_SquadManagerComponent.c');
let assertions = 0;
const check = (value, message) => { assertions++; assert.ok(value, message); };
for (const [name, fn] of Object.entries({
  Count() { return this.length; }, IsEmpty() { return !this.length; }, Insert(v) { this.push(v); },
  Clear() { this.length = 0; }, RemoveOrdered(i) { this.splice(i, 1); }
})) Object.defineProperty(Array.prototype, name, { value: fn, configurable: true });
Object.defineProperty(String.prototype, 'IsEmpty', { value() { return !this.length; }, configurable: true });
Object.defineProperty(String.prototype, 'Split', { value(separator, out, skipEmpty) {
  out.push(...this.split(separator).filter(s => !skipEmpty || s.length));
}, configurable: true });
Object.defineProperty(String.prototype, 'ToInt', { value() { return Number.parseInt(this, 10); }, configurable: true });
Object.defineProperty(String.prototype, 'Length', { value() { return this.length; }, configurable: true });
Object.defineProperty(String.prototype, 'IndexOf', { value(s) { return this.indexOf(s); }, configurable: true });
Object.defineProperty(String.prototype, 'Get', { value(i) { return this[i]; }, configurable: true });
Object.defineProperty(String.prototype, 'Substring', { value(start, count) { return this.slice(start, start + count); }, configurable: true });
let server = true, ready = true, nativeLimit = 8, now = 0, groups, commanding, spawnWorks, clearWorks;
let deleted, hints, spawns, transfers, players, nativeControllers, bumps, activeManager, uiPlayer, uiRequests;
const types = ['SCR_AIGroup', 'SCR_ChimeraCharacter', 'SCR_PlayerControllerGroupComponent',
  'SCR_GroupsManagerComponent', 'SCR_CommandingManagerComponent', 'AROR_GameFlowComponent',
  'AROR_FactionRolesComponent', 'AROR_LocationComponent', 'CompartmentAccessComponent',
  'IEntity', 'Faction', 'AIControlComponent', 'AIAgent', 'AIGroup', 'AROR_SquadRecruit', 'AROR_SquadMemberInfo',
  'SCR_PlayerController', 'AROR_SquadManagerComponent'];
const env = {
  Replication: { IsServer: () => server, BumpMe: () => bumps++ },
  GetGame: () => ({ GetPlayerController: () => uiPlayer, GetWorld: () => ({ GetWorldTime: () => now }), GetPlayerManager: () => ({
    GetPlayerControlledEntity: id => players.get(id), GetPlayerIdFromControlledEntity: e => e?.controlledBy || 0
  }) }),
  Math: { Min: Math.min, Max: Math.max, Ceil: Math.ceil, Clamp: (n, lo, hi) => Math.max(lo, Math.min(hi, n)) }, vector: { Distance: (a, b) => Math.hypot(...a.map((v, i) => v - b[i])) },
  AROR_EFactionRole: { RESISTANCE: 0 }, AROR_EUnitType: { RIFLEMAN: 0 },
  SCR_Faction: { GetEntityFaction: p => p?.faction },
  AROR_CaptureFlagPlacementComponent: {},
  string: { Empty: '', Format: (s, ...args) => s.replace(/%(\d+)/g, (_, n) => args[n - 1]) },
  AROR_ResistanceName: { Validate: s => /^[A-Za-z0-9]+(?: [A-Za-z0-9]+)*$/.test(s) && s.length <= 32 },
  AROR_LoadoutOutfitter: { IsUndressed: p => !p.dressed },
  AROR_ForceSpawner: {
    IsAlive: e => !!e && !e.dead,
    FindClearPosition: () => clearWorks,
    SpawnGroup: (prefab, role, unitTypes) => {
      check(unitTypes.length === 1 && unitTypes[0] === 0 && role === 0, 'only Resistance riflemen go through preset spawning');
      if (!spawnWorks) return null;
      const group = groupFixture(-1); group.slave = null; group.playerCount = 0; spawns.push(group); return group;
    },
    CountLiving: (group, members) => {
      const living = (group?.members || []).filter(e => !e.dead);
      for (const e of living) if (!Object.hasOwn(e, 'parent')) e.parent = group;
      members.push(...living); return members.length;
    }
  },
  SCR_EntityHelper: { DeleteEntityAndChildren: e => { deleted.push(e); if (e) e.deleted = true; } },
  AROR_NotificationManagerComponent: { SendHint_S: (id, message) => hints.push({ id, message }) }
};
for (const type of types) env[type] = { Cast: x => x };
env.AROR_SquadRecruit = class {
  constructor() { this.m_bJoined = false; this.m_Character = null; this.m_SpawnGroup = null; }
};
env.AROR_SquadMemberInfo = class {};
env.SCR_GroupsManagerComponent.GetInstance = () => groups;
env.AROR_SquadManagerComponent.GetInstance = () => activeManager;
env.AROR_UIColors = new Proxy({ Get: token => token }, { get: (o, key) => o[key] || key });
env.SCR_CommandingManagerComponent.GetInstance = () => commanding;
env.SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent = id => nativeControllers.get(id);
env.AROR_GameFlowComponent.GetInstance = () => ({ IsPlaythroughReady: () => ready });
env.AROR_FactionRolesComponent.GetInstance = () => ({ GetFaction: () => 'resistance' });

const methods = ['GetLimit', 'GetRoster', 'GetFlagLocation', 'IsReadyPlayer', 'CanRecruit', 'CountNativeMembers', 'CountMembers', 'EnsureSquad_S',
  'Recruit_S', 'Rename_S', 'GetLedSquad', 'Dismiss_S', 'IsPlayerControlled', 'GetCharacterGroup', 'CanRetireRecruit', 'Update_S', 'RemoveRecruit_S', 'Hint',
  'Publish_S', 'OnRosterReplicated'];
function compile(name, code = source, inherited = []) {
  const m = code.match(new RegExp('^\\t[^\\r\\n]*\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  assert.ok(m, `source method ${name}`);
  const args = m[1].replace(/\b(?:out|notnull)\s+/g, '').replace(/\b\w+\s+(\w+)(?=,|$)/g, '$1');
  let body = m[2].replace(/\/\/[^\r\n]*/g, '')
    .replace(/foreach \(\w+ (\w+), \w+ (\w+) : ([^\r\n]+)\)/g, 'for (const [$1, $2] of $3.entries())')
    .replace(/foreach \(\w+ (\w+) : ([^\r\n]+)\)/g, 'for (const $1 of $2)')
    .replace(/array<\w+> (\w+) = \{([^;]*)\};/g, 'let $1 = [$2];')
    .replace(/array<\w+> (\w+)/g, 'let $1');
  for (const type of [...types, 'int', 'float', 'bool', 'string', 'vector']) {
    body = body.replace(new RegExp('\\b' + type + '\\s+(\\w+)\\s*;', 'g'), (_, v) => `let ${v} = ${type === 'int' || type === 'float' ? '0' : type === 'string' ? "''" : type === 'vector' ? '[]' : 'null'};`)
      .replace(new RegExp('\\b' + type + '\\s+(\\w+)\\s*=', 'g'), 'let $1 =');
  }
  body = body.replace(/(?<![.\w])m_\w+/g, s => 'this.' + s);
  const localMethods = [...code.matchAll(/\b(?:void|bool|int|float|SCR_AIGroup|AIGroup|string|Color)\s+([A-Z]\w*)\(/g)].map(m => m[1]);
  for (const method of new Set([...methods, ...inherited, ...localMethods])) body = body.replace(new RegExp('(?<![.\\w])' + method + '(?=\\(|\\s*[,\\)])', 'g'), 'this.' + method);
  // Model the native out-entity contract; the method itself still checks the actual mocked player.
  body = body.replace(/this\.IsReadyPlayer\(playerId, player\)/g, 'this.IsReadyPlayer(playerId, player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId))');
  return new Function(...Object.keys(env), `return function(${args}) {${body}}`)(...Object.values(env));
}
const compiled = Object.fromEntries(methods.map(n => [n, compile(n)]));
env.AROR_SquadManagerComponent.GetFlagLocation = compiled.GetFlagLocation;
function groupFixture(id = 10, leader = 1) {
  const slave = { members: [], GetAIMembers() { return this.members; }, IsAIControlledCharacterMember(p) { return this.members.includes(p); } };
  return { id, leader, slave, members: [], playerCount: 1,
    GetGroupID() { return this.id; }, GetSlave() { return this.slave; }, GetLeaderID() { return this.leader; },
    GetPlayerCount() { return this.playerCount; }, IsPlayerLeader(p) { return p === this.leader; },
    GetCustomName() { return this.name || ''; },
    SetCustomName(name) { this.name = name; }
  };
}
function fixture(hasGroup = true) {
  server = ready = spawnWorks = clearWorks = true; nativeLimit = 8; now = 0;
  deleted = []; hints = []; spawns = []; transfers = []; players = new Map(); nativeControllers = new Map(); bumps = 0;
  players.set(1, { faction: 'resistance', GetOrigin: () => [0, 0, 0] });
  players.set(2, { faction: 'resistance', GetOrigin: () => [0, 0, 0] });
  const master = groupFixture();
  const membership = new Map([[1, hasGroup ? master : null], [2, hasGroup ? master : null]]);
  groups = { GetPlayerGroup: id => membership.get(id), CanCreateNewGroup: () => true,
    CreateNewPlayableGroup: () => master,
    SetGroupLeader: (_, id) => { master.leader = id; }, DeleteAndUnregisterGroup: e => deleted.push(e)
  };
  commanding = { GetMaxAIPerGroup: () => nativeLimit };
  const controller = {
    RPC_AskJoinGroup(id) { membership.set(1, master); this.groupId = id; },
    AddAIToSlaveGroup(p, group) {
      for (const spawn of spawns) spawn.members = spawn.members.filter(e => e !== p);
      group.slave.members.push(p); p.parent = group.slave; transfers.push(p);
    },
    RemoveAiFromSlaveGroup(p, group) { group.slave.members = group.slave.members.filter(e => e !== p); }
  };
  nativeControllers.set(1, controller); nativeControllers.set(2, controller);
  const location = { role: 0, GetControllingRole() { return this.role; } };
  const parent = { FindComponent: () => location };
  const flag = { GetOrigin: () => [1, 0, 0], GetParent: () => parent,
    FindComponent: type => type === env.AROR_CaptureFlagPlacementComponent ? {} : null };
  const manager = { ...compiled, m_iMaxRiflemen: 6, m_fFlagDistance: 5, m_fSpawnRadius: 15,
    m_fSpawnTimeout: 30, m_sRecruitGroup: 'base group', m_aRecruits: [], m_aRoster: [], m_iNextId: 1, m_sRoster: ''
  };
  activeManager = manager; uiRequests = [];
  uiPlayer = { id: 1, GetPlayerId() { return this.id; },
    AROR_RequestSquadRename: (...args) => uiRequests.push({ type: 'rename', args }),
    AROR_RequestSquadDismiss: (...args) => uiRequests.push({ type: 'dismiss', args }) };
  return { manager, master, flag, location, controller, membership };
}
const character = (dressed = true) => ({ dressed, FindComponent(type) {
  if (type === env.AIControlComponent) return { GetControlAIAgent: () => ({ GetParentGroup: () => this.parent || null }) };
  if (type === env.CompartmentAccessComponent) return this.access || null;
  return null;
} });

let f = fixture(false);
f.manager.Recruit_S(1, f.flag);
check(f.controller.groupId === 10 && f.manager.m_aRecruits.length === 1, 'first recruit joins through the native handler, initializing the controller command group');
for (let i = 0; i < 9; i++) f.manager.Recruit_S(1, f.flag);
check(spawns.length === 6 && f.manager.CountMembers(f.master) === 6, 'pending spawns reserve the squad cap immediately');
f = fixture(); nativeLimit = 2;
for (let i = 0; i < 4; i++) f.manager.Recruit_S(1, f.flag);
check(spawns.length === 2, 'native command cap bounds the designer cap');
for (const alter of [
  () => { server = false; }, () => { ready = false; }, () => { f.location.role = 1; },
  () => { players.get(1).dead = true; }, () => { players.get(1).faction = 'enemy'; },
  () => { f.flag.GetOrigin = () => [10, 0, 0]; }, () => { f.master.leader = 2; },
  () => { commanding = null; }, () => { f.flag.GetParent = () => null; },
  () => { f.flag.FindComponent = () => null; }, () => { clearWorks = false; }, () => { spawnWorks = false; }
]) {
  f = fixture(); alter(); f.manager.Recruit_S(1, f.flag);
  check(f.manager.m_aRecruits.length === 0, 'unauthorized, invalid, distant and failed requests cannot reserve recruits');
}
f = fixture(); f.manager.Recruit_S(1, f.flag);
const recruit = f.manager.m_aRecruits[0], soldier = character(false);
spawns[0].members.push(soldier); f.manager.Update_S();
check(!transfers.length && !recruit.m_bJoined, 'queued recruit waits for the faction preset outfit');
soldier.dressed = true; f.manager.Update_S();
check(transfers.length === 1 && recruit.m_bJoined && !recruit.m_SpawnGroup, 'equipped recruit joins through the native server API');
check(deleted.length === 1 && deleted[0] === spawns[0] && !soldier.deleted, 'only empty temporary group is deleted after transfer');
check(f.manager.CountMembers(f.master) === 1, 'native member and transaction are not double-counted');
f.manager.Rename_S(2, 10, 'Enemy Squad');
check(!f.master.name, 'nonleader cannot rename');
f.manager.Rename_S(1, 11, 'Stale Squad');
check(!f.master.name, 'stale group ID cannot rename');
f.manager.Rename_S(1, 10, '<bad>');
check(!f.master.name, 'invalid name cannot enter native squad state');
f.manager.Rename_S(1, 10, 'Falcon 1');
check(f.master.name === 'Falcon 1', 'leader renames the native group');
f.manager.Dismiss_S(2, 10, recruit.m_iId);
f.manager.Dismiss_S(1, 11, recruit.m_iId);
f.manager.Dismiss_S(1, 10, 999);
check(!soldier.deleted && f.manager.m_aRecruits.length === 1, 'nonleader, stale group and unknown recruit dismissals are refused');
soldier.controlledBy = 2; f.manager.Dismiss_S(1, 10, recruit.m_iId);
check(!soldier.deleted && f.manager.m_aRecruits.length === 1, 'a player-controlled recruit cannot be dismissed or deleted');
soldier.controlledBy = 0; f.master.leader = 2;
f.manager.Dismiss_S(1, 10, recruit.m_iId);
check(!soldier.deleted, 'former leader loses dismissal permission immediately');
f.manager.Dismiss_S(2, 10, recruit.m_iId);
check(soldier.deleted && !f.manager.m_aRecruits.length && !f.master.slave.members.length, 'new leader dismisses through native detach and soldier-only deletion');

f = fixture(); f.manager.Recruit_S(1, f.flag); now = 31000; f.manager.Update_S();
check(!f.manager.m_aRecruits.length && deleted.includes(spawns[0]), 'failed delayed spawn times out and releases its reservation');
f.manager.Recruit_S(1, f.flag);
check(f.manager.m_aRecruits.length === 1, 'timed-out slot is reusable');
f = fixture(); f.manager.Recruit_S(1, f.flag);
const unseen = character(); spawns[0].members.push(unseen);
f.manager.Dismiss_S(1, 10, f.manager.m_aRecruits[0].m_iId);
check(unseen.deleted && deleted.includes(spawns[0]), 'cancelling before a polling tick deletes an already-spawned but unobserved AI');
f = fixture(); f.manager.Recruit_S(1, f.flag);
const connected = character(); spawns[0].members.push(connected); f.manager.Update_S();
players.get(1).dead = true; f.manager.Update_S();
check(!connected.deleted && f.manager.m_aRecruits.length === 1, 'leader death retains native squad recruits for respawn');
f.master.playerCount = 0; f.manager.Update_S();
check(connected.deleted && !f.manager.m_aRecruits.length, 'last player leaving retires managed AI');
f = fixture(); f.manager.Recruit_S(1, f.flag);
const casualty = character(); spawns[0].members.push(casualty); f.manager.Update_S();
casualty.dead = true; f.manager.Update_S();
check(!f.manager.m_aRecruits.length && !casualty.deleted, 'casualties release the roster slot while preserving the corpse');
f = fixture(); f.manager.Recruit_S(1, f.flag);
const transferred = character(); spawns[0].members.push(transferred); f.manager.Update_S();
f.master.slave.members.length = 0;
f.manager.Dismiss_S(1, 10, f.manager.m_aRecruits[0].m_iId);
check(!transferred.deleted && !f.manager.m_aRecruits.length, 'stale dismissal cannot delete a rifleman released or moved to another squad');
f = fixture(); f.manager.Recruit_S(1, f.flag);
const waiting = character(); spawns[0].members.push(waiting);
f.master.slave.members.push(...Array.from({ length: 6 }, () => character()));
f.manager.Update_S();
check(waiting.deleted && !transfers.length && !f.manager.m_aRecruits.length, 'native recruitment during a pending spawn cannot overflow the cap');
f = fixture(); f.manager.Recruit_S(1, f.flag);
const takeover = character(); takeover.controlledBy = 2; spawns[0].members.push(takeover);
f.manager.Update_S();
check(!takeover.deleted && !transfers.length && !f.manager.m_aRecruits.length && !spawns[0].deleted, 'pending player takeover preserves both character and its native group');
f = fixture(); f.manager.Recruit_S(1, f.flag);
const unseenTakeover = character(); unseenTakeover.controlledBy = 2; spawns[0].members.push(unseenTakeover);
f.manager.Dismiss_S(1, 10, f.manager.m_aRecruits[0].m_iId);
check(!unseenTakeover.deleted && !spawns[0].deleted, 'cancellation before a polling tick also protects a player takeover');
f = fixture(); server = false;
f.manager.m_aRoster = Array.from({ length: 6 }, () => ({ m_iGroupId: 10, m_sState: 'Ready' }));
check(f.manager.CountMembers(f.master) === 6, 'client capacity includes distant recruits outside the streaming range');
f.manager.m_aRoster[0].m_sState = 'Arriving';
f.master.slave.members.push(...Array.from({ length: 5 }, () => character()));
check(f.manager.CountMembers(f.master) === 6, 'client capacity counts pending slots without duplicating streamed ready members');

// Regression probes are isolated so one fault does not hide the remaining lifecycle failures.
const regressionFailures = [];
function scenario(name, run) {
  try { run(); } catch (error) { regressionFailures.push({ name, error: error.message }); }
}
scenario('recruit dies before outfit transfer', () => {
  f = fixture(); f.manager.Recruit_S(1, f.flag);
  const dead = character(false); spawns[0].members.push(dead); f.manager.Update_S();
  dead.dressed = true; dead.dead = true; f.manager.Update_S();
  check(!transfers.length && !f.manager.m_aRecruits.length && !dead.deleted, 'dead arrival is never transferred and frees capacity without deleting its corpse');
});
scenario('deleted character while another recruit publishes', () => {
  f = fixture(); f.manager.Recruit_S(1, f.flag);
  const gone = character(); spawns[0].members.push(gone); f.manager.Update_S();
  f.manager.m_aRecruits[0].m_Character = null; f.master.slave.members.length = 0;
  f.manager.Recruit_S(1, f.flag);
  check(f.manager.m_aRoster.length === 1 && f.manager.m_aRoster[0].m_sState === 'Arriving', 'publication tolerates a deleted character between polling ticks');
});
scenario('release followed by the last player leaving', () => {
  f = fixture(); f.manager.Recruit_S(1, f.flag);
  const released = character(); spawns[0].members.push(released); f.manager.Update_S();
  f.master.slave.members.length = 0; f.master.playerCount = 0;
  f.manager.Update_S();
  check(!released.deleted && !f.manager.m_aRecruits.length, 'last-player cleanup does not delete a released recruit');
});
scenario('transfer followed by original group deletion', () => {
  f = fixture(); f.manager.Recruit_S(1, f.flag);
  const moved = character(); spawns[0].members.push(moved); f.manager.Update_S();
  moved.parent = groupFixture(30).slave; f.master.slave.members.length = 0;
  f.manager.m_aRecruits[0].m_Master = null; f.manager.Update_S();
  check(!moved.deleted && !f.manager.m_aRecruits.length, 'cleanup of a deleted squad preserves a recruit now controlled by another group');
});
scenario('pending arrival moved to a different AI group', () => {
  f = fixture(); f.manager.Recruit_S(1, f.flag);
  const moved = character(false); spawns[0].members.push(moved); f.manager.Update_S();
  spawns[0].members.length = 0; moved.parent = groupFixture(30).slave; moved.dressed = true;
  f.manager.Update_S();
  check(!moved.deleted && !transfers.length && !f.manager.m_aRecruits.length, 'pending transfer never steals an arrival now owned by another group');
});
scenario('transferred actor still present in a stale native roster', () => {
  f = fixture(); f.manager.Recruit_S(1, f.flag);
  const moved = character(); spawns[0].members.push(moved); f.manager.Update_S();
  moved.parent = groupFixture(30).slave;
  f.manager.Dismiss_S(1, 10, f.manager.m_aRecruits[0].m_iId);
  check(!moved.deleted, 'actual AI parent ownership protects a transferred actor even while an old membership list is stale');
});
check(!regressionFailures.length, JSON.stringify(regressionFailures));
f = fixture(); f.manager.Recruit_S(1, f.flag);
const initializing = character(); const findComponent = initializing.FindComponent;
initializing.FindComponent = () => null; spawns[0].members.push(initializing); f.manager.Update_S();
check(!transfers.length && f.manager.m_aRecruits.length === 1, 'arrival waits when its AI control component has not initialized');
initializing.FindComponent = findComponent; f.manager.Update_S();
check(transfers.length === 1, 'arrival joins after its native agent becomes available');

// Use the actual shared validator, including its boundaries and prohibited delimiters/markup.
const nameSource = read('Scripts/Game/GameFlow/AROR_ResistanceName.c');
const MAX_LENGTH = Number(nameSource.match(/MAX_LENGTH = (\d+);/)[1]);
const ALLOWED = nameSource.match(/ALLOWED = "([^"]+)";/)[1];
env.MAX_LENGTH = MAX_LENGTH; env.ALLOWED = ALLOWED;
env.AROR_ResistanceName.Validate = compile('Validate', nameSource);
for (const name of ['', ' leading', 'trailing ', 'two  spaces', 'a\nb', 'a|b', 'a;b', '<color>bad</color>', 'a'.repeat(33), 'Falcon\t1', 'Falcon_1']) {
  f = fixture(); f.manager.Rename_S(1, 10, name);
  check(!f.master.name, 'actual name validator refuses ' + JSON.stringify(name));
}
for (const name of ['Falcon 1', 'a'.repeat(32), '7', 'Alpha Bravo Charlie']) {
  f = fixture(); f.manager.Rename_S(1, 10, name);
  check(f.master.name === name, 'actual name validator accepts ' + JSON.stringify(name));
}
for (const alter of [() => { server = false; }, () => { ready = false; }, () => { players.get(1).dead = true; },
  () => { players.get(1).faction = 'enemy'; }, () => { players.delete(1); }]) {
  f = fixture(); f.manager.Recruit_S(1, f.flag);
  const target = character(); spawns[0].members.push(target); f.manager.Update_S();
  alter(); f.manager.Rename_S(1, 10, 'Unauthorized'); f.manager.Dismiss_S(1, 10, f.manager.m_aRecruits[0].m_iId);
  check(!f.master.name && !target.deleted && f.manager.m_aRecruits.length === 1, 'all mutation entry points reject invalid caller/campaign state');
}
f = fixture();
const otherSquad = groupFixture(20, 2); f.membership.set(2, otherSquad);
f.manager.Recruit_S(1, f.flag); f.manager.Recruit_S(2, f.flag);
const mine = character(), theirs = character(); spawns[0].members.push(mine); spawns[1].members.push(theirs); f.manager.Update_S();
const foreignId = f.manager.m_aRecruits.find(r => r.m_Master === otherSquad).m_iId;
f.manager.Dismiss_S(1, 10, foreignId);
check(!theirs.deleted && f.manager.m_aRecruits.length === 2, 'a squad leader cannot dismiss another squad recruit using its ID');
f.manager.Rename_S(1, 20, 'Stolen');
check(!otherSquad.name, 'a leader cannot rename another group even with a valid group ID');
check(f.manager.m_aRoster.filter(r => r.m_iGroupId === 10).length === 1 && f.manager.m_aRoster.filter(r => r.m_iGroupId === 20).length === 1, 'snapshot keeps independent squads separate');

f = fixture(); f.manager.Recruit_S(1, f.flag);
check(bumps === 1 && f.manager.m_aRoster[0].m_sState === 'Arriving', 'pending arrival publishes and parses a compact snapshot');
const vehicleOccupant = character(); spawns[0].members.push(vehicleOccupant); f.manager.Update_S();
check(bumps === 2 && f.manager.m_aRoster[0].m_sState === 'Ready', 'native membership updates the replicated status');
f.manager.Update_S(); f.manager.Publish_S();
check(bumps === 2, 'unchanged snapshots cause no replication bumps');
const vehicle = { deleted: false }; vehicleOccupant.access = { IsInCompartment: () => true, vehicle };
f.manager.Update_S();
check(bumps === 3 && f.manager.m_aRoster[0].m_sState === 'In vehicle', 'seated status survives the snapshot round trip');
f.manager.Dismiss_S(1, 10, f.manager.m_aRecruits[0].m_iId);
check(bumps === 4 && f.manager.m_sRoster === '' && !f.manager.m_aRoster.length && !vehicle.deleted, 'seated dismissal publishes an empty roster and preserves the vehicle');
f.manager.m_sRoster = 'broken;1|2;4|20|Ready;5|20|In vehicle;'; f.manager.OnRosterReplicated();
check(f.manager.m_aRoster.length === 2 && f.manager.m_aRoster[1].m_sState === 'In vehicle', 'snapshot parser skips incomplete rows without losing valid entries');

// Execute the app's layout/action methods against widget pools; this checks emitted state, not rendering.
const appSource = read('Scripts/Game/Phone/AROR_SquadApp.c').replace('super.SetVisible(', 'BaseSetVisible(');
env.PAGE_SIZE = Number(appSource.match(/PAGE_SIZE = (\d+);/)[1]);
const inherited = ['BeginFrame', 'EndFrame', 'Text', 'Button', 'Panel', 'PlaceRaw', 'BaseSetVisible'];
const appMethods = Object.fromEntries(['C', 'Layout', 'SetVisible', 'DoAction', 'DrawInstructions'].map(name => [name, compile(name, appSource, inherited)]));
function appFixture() {
  const field = { text: '', GetText() { return this.text; }, SetText(text) { this.text = text; },
    SetVisible(value) { this.visible = !!value; }, SetEnabled(value) { this.enabled = !!value; } };
  return { ...appMethods, m_wName: field, m_iGroupId: -1, m_iPage: 0, m_iConfirmDismiss: -1,
    m_bShown: false, m_aMembers: [], m_fScale: 1, buttons: [], texts: [],
    BaseSetVisible() {}, BeginFrame(_x, _y, scale) { this.m_fScale = scale; this.buttons = []; this.texts = []; },
    EndFrame() {}, Panel() {}, PlaceRaw() {}, Text(...args) { this.texts.push(args); },
    Button(...args) { this.buttons.push({ label: args[4], action: args[5], x: args[0], y: args[1], width: args[2], height: args[3] }); }
  };
}
f = fixture(false); let app = appFixture(); app.SetVisible(true, 1); app.Layout(0, 0, 1);
check(!app.m_wName.visible && app.texts.some(a => String(a[5]).includes('Recruit your first rifleman')), 'empty-squad app explains how to recruit and hides the name field');
f = fixture(); f.master.name = 'Falcon'; app = appFixture(); app.SetVisible(true, 1); app.Layout(0, 0, 1);
check(app.m_wName.text === 'Falcon' && app.m_wName.enabled, 'app fills the native squad name and enables leader editing');
app.m_wName.text = 'Falcon Two'; app.Layout(0, 0, 0.6);
check(app.m_wName.text === 'Falcon Two', 'layout preserves a leader\'s in-progress name edit');
app.DoAction('rename');
check(uiRequests.length === 1 && uiRequests[0].args[0] === 10 && uiRequests[0].args[1] === 'Falcon Two', 'rename sends the displayed group ID and edited name');
app.m_wName.text = '<bad>'; app.Layout(0, 0, 1);
check(app.buttons.find(b => b.label === 'RENAME SQUAD').action === '', 'invalid name disables the rename hit target');
app.SetVisible(false, 1); app.DoAction('rename');
check(uiRequests.length === 1 && !app.m_wName.visible, 'hidden app cannot send actions');
app.SetVisible(true, 1); uiPlayer.id = 2; app.Layout(0, 0, 1);
check(!app.m_wName.enabled && app.buttons.find(b => b.label === 'RENAME SQUAD').action === '', 'nonleader sees a disabled name field');
f.master.name = 'Renamed By Leader'; app.Layout(0, 0, 1);
check(app.m_wName.text === 'Renamed By Leader', 'nonleader name field follows replicated squad renames');
f.manager.m_aRoster = Array.from({ length: 6 }, (_, i) => ({ m_iId: i + 1, m_iGroupId: 10, m_sState: 'Ready' }));
uiPlayer.id = 1; app.Layout(0, 0, 1);
check(app.buttons.filter(b => b.label === 'DISMISS').length === 5, 'roster displays five members on the first page');
app.DoAction('next'); app.Layout(0, 0, 1);
check(app.buttons.filter(b => b.label === 'DISMISS').length === 1 && app.buttons.some(b => b.action === 'dismiss:6'), 'second page reaches the sixth recruit');
app.DoAction('dismiss:6'); app.Layout(0, 0, 1);
check(!uiRequests.some(r => r.type === 'dismiss') && app.buttons.some(b => b.label === 'CONFIRM DISMISS'), 'first dismissal click requests inline confirmation');
app.DoAction('dismiss:6');
check(uiRequests.some(r => r.type === 'dismiss' && r.args[0] === 10 && r.args[1] === 6), 'confirmed dismissal sends a stable recruit ID');
f.manager.m_aRoster.length = 2; app.Layout(0, 0, 1);
check(app.m_iPage === 0 && app.buttons.filter(b => b.label === 'DISMISS').length === 2, 'roster shrink clamps pagination after dismissals/deaths');
app.m_iConfirmDismiss = 1; f.membership.set(1, groupFixture(30, 1)); app.Layout(0, 0, 1);
check(app.m_iGroupId === 30 && app.m_iConfirmDismiss === -1, 'changing native squads clears stale dismissal confirmation');

const bridgeSource = read('Scripts/Game/Squads/AROR_SquadPlayerController.c');
const bridgeMethods = ['AROR_RequestSquadRename', 'AROR_RpcAsk_SquadRename', 'AROR_RequestSquadDismiss', 'AROR_RpcAsk_SquadDismiss'];
const bridgeFns = Object.fromEntries(bridgeMethods.map(name => [name, compile(name, bridgeSource, ['GetPlayerId', 'Rpc'])]));
f = fixture(); const sent = [];
const bridgeInstance = { ...bridgeFns, GetPlayerId: () => 2, Rpc(fn, ...args) { sent.push({ fn, args }); } };
bridgeInstance.AROR_RequestSquadRename(10, 'Impersonation');
check(!f.master.name, 'server RPC handler derives the actual nonleader caller identity');
server = false; bridgeInstance.AROR_RequestSquadRename(10, 'Requested'); bridgeInstance.AROR_RequestSquadDismiss(10, 77);
check(sent.length === 2 && sent[0].fn === bridgeInstance.AROR_RpcAsk_SquadRename && sent[1].args[1] === 77, 'client wrappers route requests through the owned-controller server RPCs');

// Interleaved deployment, occupancy changes, replacement failures and shutdown for many independent squads.
f = fixture(); const squadCount = 32, squads = [], replacements = [];
for (let id = 1; id <= squadCount; id++) {
  const squad = groupFixture(100 + id, id); squads.push(squad); f.membership.set(id, squad);
  players.set(id, { faction: 'resistance', GetOrigin: () => [0, 0, 0] }); nativeControllers.set(id, f.controller);
  for (let n = 0; n < 7; n++) f.manager.Recruit_S(id, f.flag);
  check(f.manager.CountMembers(squad) === 6, 'soak: independent native squad reserves exactly six members');
}
check(f.manager.m_aRecruits.length === 192 && f.manager.m_aRoster.length === 192, 'soak: interleaved squads publish all 192 pending arrivals');
const actors = spawns.map((spawn, i) => { const actor = character(i % 2 === 0); spawn.members.push(actor); return actor; });
f.manager.Update_S();
check(transfers.length === 96, 'soak: ready arrivals join while the other half await equipment');
actors.forEach(a => { a.dressed = true; }); f.manager.Update_S();
check(transfers.length === 192 && squads.every(s => f.manager.CountMembers(s) === 6), 'soak: completing outfits never doubles capacity');
const stableBumps = bumps;
for (let tick = 0; tick < 60; tick++) { now += 1000; f.manager.Update_S(); }
check(bumps === stableBumps, 'soak: sixty unchanged updates produce no replication churn');
actors.forEach((a, i) => { if (i % 3 === 0) a.access = { IsInCompartment: () => true }; }); f.manager.Update_S();
check(f.manager.m_aRoster.filter(r => r.m_sState === 'In vehicle').length === 64, 'soak: vehicle occupancy is published for 64 recruits');
for (const squad of squads) {
  const record = f.manager.m_aRecruits.find(r => r.m_Master === squad);
  f.manager.Dismiss_S(squad.leader, squad.id, record.m_iId);
  if (squad.leader % 2 === 0) { f.manager.Recruit_S(squad.leader, f.flag); replacements.push(f.manager.m_aRecruits.at(-1)); }
}
check(f.manager.m_aRecruits.length === 176 && replacements.length === 16, 'soak: per-squad dismissal and replacement stay isolated');
now += 31000; f.manager.Update_S();
check(f.manager.m_aRecruits.length === 160 && replacements.every(r => r.m_SpawnGroup.deleted), 'soak: failed replacements release all sixteen reserved slots');
const protectedActor = f.manager.m_aRecruits[0].m_Character; protectedActor.controlledBy = 99;
squads.forEach(s => { s.playerCount = 0; }); f.manager.Update_S();
check(!f.manager.m_aRecruits.length && !f.manager.m_aRoster.length && !protectedActor.deleted, 'soak: mass departure clears the board while preserving player takeover');
check(squads.every(s => !deleted.includes(s) && !deleted.includes(s.slave)), 'soak: cleanup leaves native player groups to the engine lifecycle');

const actionSource = read('Scripts/Game/Squads/AROR_RecruitRiflemanAction.c');
const action = { ...Object.fromEntries(['CanBeShownScript', 'CanBePerformedScript', 'PerformAction'].map(name => [name, compile(name, actionSource, ['GetOwner', 'SetCannotPerformReason'])])),
  GetOwner: () => f.flag, SetCannotPerformReason() {} };
f = fixture(); const player = players.get(1); player.controlledBy = 1;
check(action.CanBeShownScript(player) && action.CanBePerformedScript(player), 'owned flag offers recruitment to a living Resistance squad leader');
server = false; action.PerformAction(f.flag, player);
check(!spawns.length, 'client-side interaction cannot directly spawn a rifleman');
server = true; f.location.role = 1; action.PerformAction(f.flag, player);
check(!spawns.length && !action.CanBeShownScript(player), 'ownership flip between display and interaction is rechecked by the server');
f.location.role = 0; action.PerformAction(f.flag, player);
check(spawns.length === 1, 'authoritative action derives the interacting player and recruits once');
const directParent = f.flag.GetParent();
f.flag.GetParent = () => ({ FindComponent: () => null, GetParent: () => directParent });
check(f.manager.GetFlagLocation(f.flag) === f.location, 'nested flag hierarchy resolves its owning territory');

// Resource and inherited-action contracts, including the Workbench override error reported by the user.
const captureAction = read('Scripts/Game/Capture/AROR_CaptureLocationAction.c');
check(actionSource.match(/override void PerformAction\(([^)]+)\)/)[1] === captureAction.match(/override void PerformAction\(([^)]+)\)/)[1], 'PerformAction parameter names match the established native action override');
const flagPrefab = read('Prefabs/Props/Locations/AROR_Prop_CaptureFlag.et');
check((flagPrefab.match(/ActionsManagerComponent /g) || []).length === 1 && flagPrefab.includes('AROR_CaptureLocationAction'), 'recruitment preserves the existing capture action and single actions manager');
check(flagPrefab.includes('Hierarchy ') && flagPrefab.includes('RplComponent '), 'shared flag retains hierarchy resolution and replication components');
const flagGuid = read('Prefabs/Props/Locations/AROR_Prop_CaptureFlag.et.meta').match(/\{([A-F0-9]{16})\}/)[1];
for (const type of ['HQ', 'Port', 'Outpost', 'Airport', 'Factory', 'MilitaryBase', 'Heliport', 'Resources']) {
  check(read(`Prefabs/Locations/AROR_Location_${type}.et`).includes(`{${flagGuid}}Prefabs/Props/Locations/AROR_Prop_CaptureFlag.et`), `${type} inherits recruitment through the shared flag resource`);
}
const layout = read('UI/Layouts/Phone/AROR_BlackMarket.layout');
const widgetNames = [...layout.matchAll(/\bName "([^"]+)"/g)].map(m => m[1]);
check(new Set(widgetNames).size === widgetNames.length, 'pooled layout has unique widget names within each app instance');
for (const [prefix, count] of [['BMPanel', 96], ['BMText', 120], ['BMBtnBg', 64], ['BMBtnLabel', 64], ['BMBtn', 64], ['BMPic', 16], ['BMPrev', 16]])
  check(Array.from({ length: count }, (_, i) => prefix + i).every(n => widgetNames.includes(n)), 'Squad layout has a complete ' + prefix + ' pool');
const layoutGuid = read('UI/Layouts/Phone/AROR_BlackMarket.layout.meta').match(/\{([A-F0-9]{16})\}/)[1];
check(appSource.includes(`{${layoutGuid}}UI/Layouts/Phone/AROR_BlackMarket.layout`), 'Squad app uses the existing layout GUID');

const phone = read('Scripts/Game/Phone/AROR_PhoneMenu.c');
check(phone.includes('w == m_wSquadTileButton') && phone.includes('OpenApp(AROR_EPhoneApp.SQUAD)'), 'phone routes the cloned tile to Squad');
check(phone.includes('m_Squad.Layout(') && phone.includes('m_Squad.Shutdown()'), 'phone draws and releases the app');
check(read('Prefabs/GameModes/AROR_GameMode_Base.et').includes('AROR_SquadManagerComponent'), 'game-mode template wires the server component');
check(read('Prefabs/Props/Locations/AROR_Prop_CaptureFlag.et').includes('AROR_RecruitRiflemanAction'), 'shared flag wires recruitment for HQ and territories');
const bridge = read('Scripts/Game/Squads/AROR_SquadPlayerController.c');
check(/Rename_S\(GetPlayerId\(\)/.test(bridge) && /Dismiss_S\(GetPlayerId\(\)/.test(bridge), 'RPC caller identity comes from the owning controller');
console.log(JSON.stringify({ assertions, nativeVerified: false, result: 'Player squad server policy and lifecycle passed' }));
