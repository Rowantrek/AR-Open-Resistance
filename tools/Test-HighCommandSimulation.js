// Execute actual HICOMM status/activation methods with native AI services mocked.
// Native simulation, navigation and dedicated-server behavior still require Workbench.
const fs = require('fs'), path = require('path'), assert = require('assert/strict');
const source = fs.readFileSync(path.join(__dirname, '../AROpenResistance/Open Resistance/Scripts/Game/HighCommand/AROR_HighCommandComponent.c'), 'utf8');
let assertions = 0, server = true, clock = 0, refunds = 0;
const equal = (a, b, why) => { assertions++; assert.deepEqual(a, b, why); };
Object.defineProperties(Array.prototype, {
  Contains: { value(x) { return this.includes(x); }, configurable: true },
  Insert: { value(x) { this.push(x); }, configurable: true },
  Count: { value() { return this.length; }, configurable: true }
});
Object.defineProperty(String.prototype, 'IsEmpty', { value() { return !this.length; }, configurable: true });
const orders = { NONE: 0, MOVE: 1, ATTACK: 2, HOLD: 3 };
const env = {
  Replication: { IsServer: () => server }, Math: { Max: Math.max },
  GetGame: () => ({ GetWorld: () => ({ GetWorldTime: () => clock }), GetPlayerManager: () => ({ GetPlayerIdFromControlledEntity: e => e.player || 0 }) }),
  ChimeraCharacter: { Cast: e => e?.character ? e : null },
  FactionAffiliationComponent: { Cast: x => x },
  AROR_EHighCommandOrder: orders, AROR_EFactionRole: { RESISTANCE: 0 },
  AROR_EconomyComponent: { GetInstance: () => ({ AddFactionFunds_S() { refunds++; }, AddManpower_S() { refunds++; } }) },
  AROR_LocationManagerComponent: { GetInstance: () => null },
  vector: { DistanceXZ: (a, b) => Math.hypot(a[0] - b[0], a[2] - b[2]) },
  Print() {}, string: { Format: () => '' }, LogLevel: { WARNING: 1, NORMAL: 0 }
};
function method(name) {
  const match = source.match(new RegExp('^\\t[^\\r\\n]*\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  assert(match, 'Missing actual method ' + name);
  const args = match[1].replace(/\b\w+ (\w+)(?=,|$)/g, '$1');
  let body = match[2].replace(/foreach \(AIAgent agent : agents\)/g, 'for (const agent of agents)')
    .replace(/array<AIAgent> agents = \{\};/g, 'let agents = [];')
    .replace(/\b(?:int|bool|float|vector|string|IEntity|ChimeraCharacter|CharacterControllerComponent|FactionAffiliationComponent|AROR_\w+) (\w+) =/g, 'let $1 =')
    .replace(/\b(?:AROR_\w+) (\w+);/g, 'let $1 = null;')
    .replace(/\bint (\w+);/g, 'let $1 = 0;')
    .replace(/(?<![.\w])m_\w+/g, x => 'this.' + x);
  for (const n of ['RefreshLeaders_S', 'GetResistanceFactionKey', 'CountAlive', 'RemoveSquad_S', 'IssueWaypoint_S', 'Reply', 'PublishSnapshot_S', 'GridReference'])
    body = body.replace(new RegExp('(?<![.\\w])' + n + '\\(', 'g'), 'this.' + n + '(');
  return new Function(...Object.keys(env), 'return function(' + args + '){' + body + '}')(...Object.values(env));
}
function member({ dead = false, player = 0, character = true } = {}) {
  const e = { character, player, GetCharacterController: () => ({ IsDead: () => dead }), affiliation: '', assignments: 0,
    FindComponent() { return { GetAffiliatedFactionKey: () => this.affiliation, SetAffiliatedFactionByKey: key => { this.affiliation = key; this.assignments++; } }; } };
  return { entity: e, leases: [], activations: [], GetControlledEntity: () => e,
    PreventMaxLOD(seconds) { this.leases.push(seconds); }, ActivateAI(force) { this.activations.push(force); } };
}
function squad(agents = []) {
  return { m_bFormed: false, m_iRaisedAt: 0, m_iNextWake: 0, m_iOwnerId: 7, m_eOrder: orders.NONE,
    m_vPosition: [0, 0, 0], m_vTarget: [500, 0, 0], m_aAffiliated: [],
    m_Group: { dormant: false, stored: 0, requests: 0, players: 0,
      IsDormant() { return this.dormant; }, GetDormantAliveCount() { return this.stored; },
      RequestSpawn() { this.requests++; }, GetPlayerCount() { return this.players; },
      GetAgents: out => out.push(...agents), GetLeaderEntity: () => null, GetOrigin: () => [100, 0, 200] } };
}
const ctx = { m_iUpdateInterval: 2, m_iFormTimeout: 90, m_fMoveArrivalRadius: 25, m_aSquads: [], removed: 0, published: 0,
  RefreshLeaders_S() {}, GetResistanceFactionKey: () => 'RESISTANCE', Reply() {},
  RemoveSquad_S(i) { this.removed++; this.m_aSquads.splice(i, 1); }, PublishSnapshot_S() { this.published++; },
  CountAlive: method('CountAlive'), Update_S: method('Update_S') };
const ai = member(), player = member({ player: 3 }), dead = member({ dead: true }), prop = member({ character: false });
let s = squad([ai, player, dead, prop, null]);
equal(ctx.CountAlive(s, 'RESISTANCE'), 2, 'living AI and players counted; dead/non-character/null agents excluded');
equal([ai.leases, ai.activations], [[10], [true]], 'far-away AI gets a finite activation lease');
equal([player.leases, dead.leases, prop.leases], [[], [], []], 'players, corpses and props never activated');
ctx.CountAlive(s, 'RESISTANCE');
equal(ai.leases, [10, 10], 'lease renews on subsequent status ticks');
equal(ai.entity.assignments, 1, 'simulation renewal does not repeat faction assignment');
ctx.m_iUpdateInterval = 10; ctx.CountAlive(s, 'RESISTANCE');
equal(ai.leases.at(-1), 20, 'lease spans even the longest configured update interval');
ctx.m_iUpdateInterval = 2; server = false;
equal(ctx.CountAlive(s, 'RESISTANCE'), 0, 'client cannot activate or count a server squad');
equal(ai.leases.length, 3, 'client never renews lease'); server = true;
s = squad(); s.m_Group.dormant = true; s.m_Group.stored = 4; ctx.m_aSquads = [s];
ctx.Update_S();
equal([s.m_iAlive, s.m_bFormed, s.m_Group.requests, ctx.removed, refunds], [4, true, 1, 0, 0], 'dormant survivors retained and reactivation requested without refund/removal');
clock = 2000; ctx.Update_S(); equal(s.m_Group.requests, 1, 'wake requests paced while waiting on engine budget');
clock = 10000; ctx.Update_S(); equal(s.m_Group.requests, 2, 'dormant squad retries activation');
s.m_Group.players = 1; clock = 20000; ctx.Update_S(); equal(s.m_Group.requests, 2, 'player-occupied group receives no spawn request');
s.m_Group.players = 0; s.m_Group.dormant = false;
s.m_Group.GetAgents = out => out.push(ai); ctx.Update_S();
equal([s.m_iAlive, s.m_vPosition, ctx.removed], [1, [100, 0, 200], 0], 'reactivated squad resumes normal roster/position updates');
s.m_Group.GetAgents = () => {}; ctx.Update_S(); equal(ctx.removed, 1, 'actual wiped-out squad still removed');
s = squad(); ctx.m_aSquads = [s]; clock = 100001; ctx.Update_S();
equal([ctx.removed, refunds], [2, 2], 'squad that never forms still times out and refunds');
s = squad(); s.m_Group = null; equal(ctx.CountAlive(s, 'RESISTANCE'), 0, 'missing group is safe');
console.log(JSON.stringify({ assertions, note: 'Actual HICOMM methods; native AI budgets, distance simulation and multiplayer remain unverified.' }));
