// Actual garrison scan/tick, retirement and aircraft-seat methods with native services mocked.
const fs = require('fs'), path = require('path'), assert = require('assert/strict');
const gameRoot = path.join(__dirname, '../AROpenResistance/Open Resistance/Scripts/Game');
const read = p => fs.readFileSync(path.join(gameRoot, p), 'utf8');
const manager = read('Garrisons/AROR_GarrisonManagerComponent.c');
const garrison = read('Garrisons/AROR_GarrisonComponent.c');
const forces = read('Capture/AROR_LocationForces.c');
let assertions = 0, server = true, clock = 0, ready = true;
const check = (condition, why) => { assertions++; assert.ok(condition, why); };
for (const [name, fn] of Object.entries({ InsertAll(xs) { this.push(...xs); }, Clear() { this.length = 0; }, Insert(x) { this.push(x); }, Count() { return this.length; }, IsEmpty() { return !this.length; } }))
  Object.defineProperty(Array.prototype, name, { value: fn, configurable: true });
let players = [];
const playerManager = { GetPlayers: out => players.forEach((_, i) => out.push(i)), GetPlayerControlledEntity: i => players[i] };
const env = {
  Replication: { IsServer: () => server },
  GetGame: () => ({ GetPlayerManager: () => playerManager, GetWorld: () => ({ GetWorldTime: () => clock }) }),
  Math: { Max: Math.max, Min: Math.min, Mod: (a, b) => a % b },
  vector: { DistanceXZ: (a, b) => Math.hypot(a[0] - b[0], a[2] - b[2]) },
  ChimeraCharacter: { Cast: e => e?.character ? e : null },
  CompartmentAccessComponent: { Cast: x => x }, HelicopterControllerComponent: 'helicopter',
  AROR_GarrisonPolicy: { IsEnemyRole: role => role === 1 || role === 2 }, AROR_LocationForces: {},
  AROR_PopulationBudget:{GetAvailable_S:()=>10000},
  AROR_ResistancePresence:{GetPositions:out=>{out.Clear();for(const e of players){if(e.character&&!e.dead&&!env.AROR_LocationForces.IsInAircraft(e))out.Insert(e.GetOrigin());}}},
  Print() {}, string: { Format: () => '' }, LogLevel: { NORMAL: 0 }
};
function compile(source, name) {
  const m = source.match(new RegExp('^\\t[^\\r\\n]*\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  assert(m, 'Missing actual method ' + name);
  const args = m[1].replace(/\bnotnull /g, '').replace(/array<[^>]+>\s+/g, '').replace(/\b(?:int|float|bool|ChimeraCharacter|AROR_\w+)\s+/g, '');
  let body = m[2].replace(/\bfloat\.MAX/g, 'Number.MAX_VALUE')
    .replace(/array<\w+> (\w+) = \{\};/g, 'let $1 = [];')
    .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g, 'for (const $1 of $2)')
    .replace(/\b(?:int|float|bool|IEntity|ChimeraCharacter|CompartmentAccessComponent|BaseCompartmentSlot|AROR_\w+) (\w+) =/g, 'let $1 =')
    .replace(/\b(?:int|bool) (\w+);/g, 'let $1 = 0;')
    .replace(/(?<![.\w])m_\w+/g, s => 'this.' + s);
  for (const method of ['PlayerNear', 'NearestPlayerDistance', 'HasRecentCombat', 'CanDefendRole', 'ScanProximity_S', 'EnsureRegistry_S', 'FactionFor'])
    body = body.replace(new RegExp('(?<![.\\w])' + method + '\\(', 'g'), 'this.' + method + '(');
  return new Function(...Object.keys(env), 'return function(' + args + '){' + body + '}')(...Object.values(env));
}
env.AROR_LocationForces.IsInAircraft = compile(forces, 'IsInAircraft');
function vehicle(helicopter, parent = null) {
  return { GetRootParent: () => parent, FindComponent: type => type === 'helicopter' && helicopter ? {} : null };
}
function player(seatVehicle = null, position = [0, 300, 0]) {
  return { character: true, dead: false, seated: !!seatVehicle, vehicle: seatVehicle, position,
    GetOrigin() { return this.position; }, GetCharacterController() { return { IsDead: () => this.dead }; },
    FindComponent() { return { IsInCompartment: () => this.seated, GetCompartment: () => this.vehicle ? { GetVehicle: () => this.vehicle } : null }; } };
}
const heli = vehicle(true), truck = vehicle(false), gun = vehicle(false, heli);
check(env.AROR_LocationForces.IsInAircraft(player(heli)), 'pilot/passenger seat on helicopter detected');
check(env.AROR_LocationForces.IsInAircraft(player(gun)), 'door gun on helicopter child entity detected');
check(!env.AROR_LocationForces.IsInAircraft(player(truck)), 'ground vehicle remains eligible');
check(!env.AROR_LocationForces.IsInAircraft(player()), 'dismounted player remains eligible');
const tick = compile(manager, 'Tick');
function fixture(pressure = false) {
  const location = { GetCenter: () => [0, 0, 0], GetId: () => 'test', GetControllingRole: () => 1 };
  const site = { active: false, started: false, starts: 0, spawns: 0, retired: 0, observed: 0,
    m_iOutsideSince: -1, m_Location: location, m_aSlots: [],
    GetLocation: () => location, IsEligible: () => true, IsCapacityWaiting: () => pressure,
    IsGarrisonActive() { return this.active; }, HasDeploymentStarted() { return this.started; },
    BeginDeployment_S() { this.started = true; this.starts++; },
    RenewSimulation_S(){},IsDeploymentClosed: () => false, UpdateDeployment_S() {}, RefreshPending_S() {},
    HasRoster() { return this.active; }, GetRosterRole: () => 1, CommittedPersonnel: () => 4,
    Observe_S() { this.observed++; },
    CanRetire: compile(garrison, 'CanRetire'), OutsideLongEnough: compile(garrison, 'OutsideLongEnough'),
    Retire_S() { this.active = false; this.started = false; this.retired++; },
    Generate_S: () => true, BindAssets_S: () => true, HasSuitableDefenceTerrain_S: () => true,
    NextPersonnelCost: () => 4, SetCapacityWait_S() {},
    SpawnNextGroup_S() { this.spawns++; this.active = true; return true; },
    SpawnNextAsset_S() { throw Error('Unexpected optional spawn'); }
  };
  const state = { m_bGarrisonsActive: true, m_iNextProximityAt: 0, m_iCursor: 0, m_fActivationRadius: 1000, m_fProximityInterval: 1,
    m_fRetirementRadius: 2500, m_iRetirementDelay: 120, m_fPressureRetirementRadius: 1200, m_iPressureRetirementDelay: 15,
    m_iMaxPersonnel: 96, m_fPlayerSpawnClearance: 6, m_aNearby: [], m_aGarrisons: [site], m_Flow: { IsPlaythroughReady: () => ready },
    EnsureRegistry_S: () => true, CanDefendRole: () => true, FactionFor: () => ({}), HasRecentCombat: () => false,
    PlayerNear: compile(manager, 'PlayerNear'), NearestPlayerDistance: compile(manager, 'NearestPlayerDistance'), ScanProximity_S: compile(manager, 'ScanProximity_S') };
  return { site, state, at(now) { clock = now; tick.call(state); } };
}
for (const seat of [heli, gun]) {
  const f = fixture(); players = [player(seat)]; f.at(0); f.at(1000);
  check(f.site.starts === 0 && f.site.spawns === 0 && f.state.m_aNearby.length === 0, 'helicopter crew alone cannot start deployment or spawn');
  players[0].position = [0, 0, 0]; f.at(2000);
  check(f.site.spawns === 0, 'landing while seated still cannot trigger a garrison');
  players[0].seated = false; f.at(3000);
  check(f.site.spawns === 1, 'disembarking admits deployment on next proximity scan');
}
for (const seat of [null, truck]) {
  const f = fixture(); players = [player(seat, [1000, 0, 0])]; f.at(0);
  check(f.site.spawns === 1, 'living ground player or ground-vehicle occupant activates at radius boundary');
}
for (const pressure of [false, true]) {
  const f = fixture(pressure), ground = player(null, [0, 0, 0]), pilot = player(heli);
  players = [ground, pilot]; f.at(0);
  check(f.site.spawns === 1, 'nearby ground player triggers garrison with helicopter supporting');
  ground.dead = true; f.at(1000); f.at(121000);
  check(f.site.active && f.site.retired === 0 && f.site.observed > 0, 'helicopter preserves and observes defenders after ground player dies, including under pressure');
  check(f.site.spawns === 1 && f.state.m_aNearby.length === 0, 'helicopter does not request additional deployment after ground player dies');
  players = [pilot]; f.at(122000); f.at(242000);
  check(f.site.active && f.site.retired === 0, 'helicopter alone retains defenders after dead ground character disappears');
  pilot.position = [3000, 500, 0]; f.at(243000);
  check(f.site.retired === 0, 'leaving starts existing retirement grace without immediate cleanup');
  f.at(243000 + (pressure ? 15000 : 120000));
  check(f.site.retired === 1, 'garrison retires normally after helicopter leaves and grace expires');
}
// Retirement must also protect tracked defenders away from their parent site.
const detached = fixture(true); detached.site.active = true; detached.site.started = true;
detached.site.m_aSlots = [{ m_Entity: { GetOrigin: () => [3000, 0, 0] } }];
players = [player(heli, [3000, 600, 0])]; detached.at(0); detached.at(120000);
check(detached.site.retired === 0, 'helicopter near tracked actors blocks pressure cleanup even far from site');
// A helicopter cannot accidentally activate other sites along its route.
const distant = fixture(); players = [player(null, [2000, 0, 0]), player(heli, [0, 400, 0])]; distant.at(0);
check(distant.site.spawns === 0, 'ground player outside activation range cannot combine with overflight to admit a site');
const dead = fixture(); const corpse = player(); corpse.dead = true; players = [corpse]; dead.at(0);
check(dead.site.spawns === 0, 'dead ground players still cannot activate');
const client = fixture(); players = [player()]; server = false; client.at(0);
check(client.site.spawns === 0, 'clients cannot activate garrisons'); server = true;
ready = false; client.at(1000); check(client.site.spawns === 0, 'campaign readiness still gates deployment'); ready = true;
console.log(JSON.stringify({ assertions, note: 'Actual source with native services mocked; Workbench compile and multiplayer flight acceptance still required.' }));
