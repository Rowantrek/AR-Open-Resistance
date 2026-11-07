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
let server = true, ready = true, nativeLimit = 8, now = 0, groups, commanding, spawnWorks, clearWorks;
let deleted, hints, spawns, transfers, players, nativeControllers;
const types = ['SCR_AIGroup', 'SCR_ChimeraCharacter', 'SCR_PlayerControllerGroupComponent',
  'SCR_GroupsManagerComponent', 'SCR_CommandingManagerComponent', 'AROR_GameFlowComponent',
  'AROR_FactionRolesComponent', 'AROR_LocationComponent', 'CompartmentAccessComponent',
  'IEntity', 'Faction', 'AROR_SquadRecruit', 'AROR_SquadMemberInfo'];
const env = {
  Replication: { IsServer: () => server },
  GetGame: () => ({ GetWorld: () => ({ GetWorldTime: () => now }), GetPlayerManager: () => ({
    GetPlayerControlledEntity: id => players.get(id), GetPlayerIdFromControlledEntity: e => e?.controlledBy || 0
  }) }),
  Math: { Min: Math.min, Max: Math.max }, vector: { Distance: (a, b) => Math.hypot(...a.map((v, i) => v - b[i])) },
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
    CountLiving: (group, members) => { members.push(...(group?.members || []).filter(e => !e.dead)); return members.length; }
  },
  SCR_EntityHelper: { DeleteEntityAndChildren: e => { deleted.push(e); if (e) e.deleted = true; } },
  AROR_NotificationManagerComponent: { SendHint_S: (id, message) => hints.push({ id, message }) }
};
for (const type of types) env[type] = { Cast: x => x };
env.AROR_SquadRecruit = class {
  constructor() { this.m_bJoined = false; this.m_Character = null; this.m_SpawnGroup = null; }
};
env.SCR_GroupsManagerComponent.GetInstance = () => groups;
env.SCR_CommandingManagerComponent.GetInstance = () => commanding;
env.SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent = id => nativeControllers.get(id);
env.AROR_GameFlowComponent.GetInstance = () => ({ IsPlaythroughReady: () => ready });
env.AROR_FactionRolesComponent.GetInstance = () => ({ GetFaction: () => 'resistance' });

const methods = ['GetLimit', 'GetFlagLocation', 'IsReadyPlayer', 'CanRecruit', 'CountNativeMembers', 'CountMembers', 'EnsureSquad_S',
  'Recruit_S', 'Rename_S', 'GetLedSquad', 'Dismiss_S', 'IsPlayerControlled', 'Update_S', 'RemoveRecruit_S', 'Hint'];
function compile(name) {
  const m = source.match(new RegExp('^\\t[^\\r\\n]*\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  assert.ok(m, `source method ${name}`);
  const args = m[1].replace(/\b(?:out|notnull)\s+/g, '').replace(/\b\w+\s+(\w+)(?=,|$)/g, '$1');
  let body = m[2].replace(/\/\/[^\r\n]*/g, '')
    .replace(/foreach \(\w+ (\w+), \w+ (\w+) : ([^)]+)\)/g, 'for (const [$1, $2] of $3.entries())')
    .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g, 'for (const $1 of $2)')
    .replace(/array<\w+> (\w+) = \{([^;]*)\};/g, 'let $1 = [$2];')
    .replace(/array<\w+> (\w+)/g, 'let $1');
  for (const type of [...types, 'int', 'float', 'bool', 'string', 'vector']) {
    body = body.replace(new RegExp('\\b' + type + '\\s+(\\w+)\\s*;', 'g'), (_, v) => `let ${v} = ${type === 'int' || type === 'float' ? '0' : type === 'string' ? "''" : type === 'vector' ? '[]' : 'null'};`)
      .replace(new RegExp('\\b' + type + '\\s+(\\w+)\\s*=', 'g'), 'let $1 =');
  }
  body = body.replace(/(?<![.\w])m_\w+/g, s => 'this.' + s);
  for (const method of [...methods, 'Publish_S']) body = body.replace(new RegExp('(?<![.\\w])' + method + '\\(', 'g'), 'this.' + method + '(');
  // Model the native out-entity contract; the method itself still checks the actual mocked player.
  body = body.replace(/this\.IsReadyPlayer\(playerId, player\)/g, 'this.IsReadyPlayer(playerId, player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId))');
  return new Function(...Object.keys(env), `return function(${args}) {${body}}`)(...Object.values(env));
}
const compiled = Object.fromEntries(methods.map(n => [n, compile(n)]));
function groupFixture(id = 10, leader = 1) {
  const slave = { members: [], GetAIMembers() { return this.members; }, IsAIControlledCharacterMember(p) { return this.members.includes(p); } };
  return { id, leader, slave, members: [], playerCount: 1,
    GetGroupID() { return this.id; }, GetSlave() { return this.slave; }, GetLeaderID() { return this.leader; },
    GetPlayerCount() { return this.playerCount; }, IsPlayerLeader(p) { return p === this.leader; },
    SetCustomName(name) { this.name = name; }
  };
}
function fixture(hasGroup = true) {
  server = ready = spawnWorks = clearWorks = true; nativeLimit = 8; now = 0;
  deleted = []; hints = []; spawns = []; transfers = []; players = new Map(); nativeControllers = new Map();
  players.set(1, { faction: 'resistance', GetOrigin: () => [0, 0, 0] });
  players.set(2, { faction: 'resistance', GetOrigin: () => [0, 0, 0] });
  const master = groupFixture();
  let assigned = hasGroup ? master : null;
  groups = { GetPlayerGroup: () => assigned, CanCreateNewGroup: () => true,
    CreateNewPlayableGroup: () => master, AddPlayerToGroup: () => { assigned = master; },
    SetGroupLeader: (_, id) => { master.leader = id; }, DeleteAndUnregisterGroup: e => deleted.push(e)
  };
  commanding = { GetMaxAIPerGroup: () => nativeLimit };
  const controller = {
    RPC_AskJoinGroup(id) { assigned = master; this.groupId = id; },
    AddAIToSlaveGroup(p, group) {
      for (const spawn of spawns) spawn.members = spawn.members.filter(e => e !== p);
      group.slave.members.push(p); transfers.push(p);
    },
    RemoveAiFromSlaveGroup(p, group) { group.slave.members = group.slave.members.filter(e => e !== p); }
  };
  nativeControllers.set(1, controller); nativeControllers.set(2, controller);
  const location = { role: 0, GetControllingRole() { return this.role; } };
  const parent = { FindComponent: () => location };
  const flag = { GetOrigin: () => [1, 0, 0], GetParent: () => parent,
    FindComponent: type => type === env.AROR_CaptureFlagPlacementComponent ? {} : null };
  const manager = { ...compiled, m_iMaxRiflemen: 6, m_fFlagDistance: 5, m_fSpawnRadius: 15,
    m_fSpawnTimeout: 30, m_sRecruitGroup: 'base group', m_aRecruits: [], m_aRoster: [], m_iNextId: 1,
    Publish_S() { this.publishes = (this.publishes || 0) + 1; }
  };
  return { manager, master, flag, location, controller };
}
const character = (dressed = true) => ({ dressed, FindComponent: () => null });

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
check(!soldier.deleted, 'a player-controlled recruit cannot be deleted');
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

const phone = read('Scripts/Game/Phone/AROR_PhoneMenu.c');
check(phone.includes('w == m_wSquadTileButton') && phone.includes('OpenApp(AROR_EPhoneApp.SQUAD)'), 'phone routes the cloned tile to Squad');
check(phone.includes('m_Squad.Layout(') && phone.includes('m_Squad.Shutdown()'), 'phone draws and releases the app');
check(read('Prefabs/GameModes/AROR_GameMode_Base.et').includes('AROR_SquadManagerComponent'), 'game-mode template wires the server component');
check(read('Prefabs/Props/Locations/AROR_Prop_CaptureFlag.et').includes('AROR_RecruitRiflemanAction'), 'shared flag wires recruitment for HQ and territories');
const bridge = read('Scripts/Game/Squads/AROR_SquadPlayerController.c');
check(/Rename_S\(GetPlayerId\(\)/.test(bridge) && /Dismiss_S\(GetPlayerId\(\)/.test(bridge), 'RPC caller identity comes from the owning controller');
console.log(JSON.stringify({ assertions, nativeVerified: false, result: 'Player squad server policy and lifecycle passed' }));
