// Exercises extracted Enforce method bodies with native services mocked; this is NOT an Enforce compiler.
// Run with Node: node tools/Test-FieldSystems.js
function runFieldSystemTests(sources) {
  let assertions = 0;
  const check = (condition, message) => { assertions++; if (!condition) throw Error(message); };
  const A = (items = []) => Object.assign(items, {
    Count() { return this.length; }, IsEmpty() { return this.length === 0; },
    Insert(x) { this.push(x); }, Contains(x) { return this.includes(x); },
    Remove(i) { this.splice(i, 1); }, RemoveItem(x) { const i = this.indexOf(x); if (i >= 0) this.splice(i, 1); },
    Clear() { this.length = 0; }
  });
  let server = true, now = 1000, validPlayer = true;
  const groups = new Map([[1, {id: 'alpha'}], [2, null], [3, null]]);
  groups.set(2, groups.get(1));
  const hints = [], deleted = [];
  const distance = (a, b) => Math.hypot(a[0] - b[0], a[2] - b[2]);
  const env = {
    A, UPDATE_SECONDS: 5, MAX_STALLS: 3,
    AROR_EMarkerScope: {PRIVATE: 0, SQUAD: 1, FACTION: 2},
    AROR_EFactionRole: {RESISTANCE: 0, CIVILIANS: 3},
    AROR_ERecruitAction: {RECRUIT: 0, FOLLOW: 1, HOLD: 2, REGROUP: 3, BOARD_PASSENGER: 4, BOARD_DRIVER: 5, BOARD_GUNNER: 6, DISMOUNT: 7, TRANSFER: 8, DISMISS: 9},
    Replication: {IsServer: () => server},
    AROR_FieldAccess: {
      Player: () => validPlayer ? {} : null, PlayerKey: id => 'identity_' + id,
      MapPosition: p => p[0] >= 0 && p[0] <= 12800 && p[2] >= 0 && p[2] <= 12800,
      Caption: text => text.slice(0, 48).replace(/[|;\n\r<>]/g, ' ').trim()
    },
    SCR_GroupsManagerComponent: {GetInstance: () => ({GetPlayerGroup: id => groups.get(id)})},
    AROR_NotificationManagerComponent: {SendHint_S: (id, text) => hints.push({id, text})},
    GetGame: () => ({GetWorld: () => ({GetWorldTime: () => now, GetSurfaceY: () => 10})}),
    string: {Format: (s, ...args) => s.replace(/%(\d+)/g, (_, i) => args[i - 1])},
    vector: {DistanceXZ: distance, Distance: (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2])}, Vector: (x, y, z) => [x, y, z],
    Math: {Max: Math.max, Min: Math.min, Round: Math.round, AbsFloat: Math.abs},
    AROR_PlayerMarker: function () { this.m_iExpiresAt = 0; },
    SCR_EntityHelper: {DeleteEntityAndChildren: entity => deleted.push(entity)},
    AROR_ForceSpawner: {
      CountAlive: () => 0,
      CountLiving: (group, members) => { members.push(...(group?.living || [])); return members.length; },
      HasPlayerAboard: vehicle => !!vehicle?.player,
      IsDestroyed: vehicle => !!vehicle?.destroyed,
      SeatGroup: () => false
    },
    Print: () => {}, LogLevel: {WARNING: 1}
  };
  function extract(source, name) {
    const match = source.match(new RegExp('^\\t(?:protected |static |override |const )*[^\\r\\n]*?\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if (!match) throw Error('Cannot extract method ' + name);
    return {args: match[1], body: match[2]};
  }
  function compile(source, name, tail = '') {
    const method = extract(source, name);
    const args = method.args.split(',').map(arg => arg.trim().split(/\s+/).pop()).filter(Boolean).join(',');
    let body = method.body.replace(/\/\/[^\n]*/g, '')
      .replace(/foreach \(\w+ (\w+), \w+ (\w+) : ([^)]+)\)/g, 'for (const [$1, $2] of $3.entries())')
      .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g, 'for (const $1 of $2)')
      .replace(/array<\w+> (\w+) = \{\};/g, 'let $1 = A();')
      .replace(/vector (\w+)\[(\d+)\];/g, 'let $1 = Array.from({length:$2},()=>[0,0,0]);')
      .replace(/\b(?:int|float) (\w+);/g, 'let $1 = 0;')
      .replace(/\bbool (\w+);/g, 'let $1 = false;')
      .replace(/\bstring (\w+);/g, 'let $1 = "";')
      .replace(/\bvector (\w+);/g, 'let $1 = [0,0,0];')
      .replace(/\b(?:AROR_\w+|SCR_\w+|IEntity|FactionAffiliationComponent|Physics|CompartmentAccessComponent|BaseCompartmentSlot|BaseCompartmentManagerComponent) (\w+);/g, 'let $1 = null;')
      .replace(/\b(?:int|float|bool|string|vector|AROR_\w+|SCR_\w+|IEntity|Physics|CompartmentAccessComponent|BaseCompartmentSlot|BaseCompartmentManagerComponent) (\w+) =/g, 'let $1 =')
      .replace(/caption\.IsEmpty\(\)/g, '(caption.length === 0)')
      .replace(/(\w+(?:\[[^\]]+\])?)\.ToInt\(\)/g, 'parseInt($1, 10)')
      .replace(/(\w+(?:\[[^\]]+\])?)\.ToFloat\(\)/g, 'parseFloat($1)')
      .replace(/row\.Split\("\|", fields, false\);/g, 'fields.push(...row.split("|"));')
      .replace(/marker\.m_sOwnerKey\.IsEmpty\(\)/g, '(marker.m_sOwnerKey.length === 0)')
      .replace(/\bm_(\w+)\b/g, 'this.m_$1');
    // Fields of records are already explicitly qualified; don't insert this into those accesses.
    body = body.replace(/\.this\.m_/g, '.m_');
    for (const methodName of ['Publish_S', 'VisibleToPlayers', 'PlayerNear', 'Remove', 'ClearWaypoint', 'IssueRoute', 'PickDestination', 'Board', 'SetOrder', 'IsSeated', 'Waypoint', 'Refund', 'Reply', 'Remove_S', 'Recruit_S', 'IsBoardOrder', 'Owned', 'NearHQ', 'FriendlyOrCivilian', 'Dismount']) {
      body = body.replace(new RegExp('(?<![.\\w])' + methodName + '\\(', 'g'), 'this.' + methodName + '(');
    }
    try { return new Function('env', 'with(env) { return function(' + args + ') {' + body + tail + '}; }')(env); }
    catch (error) { throw Error(name + ': ' + error.message + '\n' + body); }
  }
  const markerManager = {m_aMarkers: A(), m_iNextId: 1, m_iPerPlayerLimit: 2, m_iCampaignLimit: 4, Publish_S() {}};
  markerManager.request = compile(sources.markers, 'Request_S');
  markerManager.snapshot = compile(sources.markers, 'Snapshot_S');
  const request = (...args) => markerManager.request(...args);
  request(1, 0, [100, 0, 200], 'Private', 0, 0, 0);
  check(markerManager.m_aMarkers.length === 1, 'Author can create a private marker');
  check(markerManager.snapshot(1).includes('Private'), 'Author sees their private marker');
  check(!markerManager.snapshot(2).includes('Private'), 'Squadmate cannot see a private marker');
  check(!markerManager.snapshot(3).includes('identity_'), 'Snapshot never includes backend identity keys');
  request(2, 1, [200, 0, 200], 'Stolen', 2, 1, 0);
  check(markerManager.m_aMarkers[0].m_sCaption === 'Private', 'Other players cannot edit an annotation');
  request(2, -1, [0, 0, 0], '', 0, 0, 0);
  check(markerManager.m_aMarkers.length === 1, 'Other players cannot delete an annotation');
  request(1, 0, [300, 0, 300], 'Shared', 1, 2, 600);
  check(markerManager.snapshot(2).includes('Shared'), 'Squadmate receives squad markers');
  check(!markerManager.snapshot(3).includes('Shared'), 'Other squads do not receive squad markers');
  groups.set(2, {id: 'bravo'});
  check(!markerManager.snapshot(2).includes('Shared'), 'Leaving a squad removes its marker visibility');
  check(markerManager.m_aMarkers[1].m_iExpiresAt === 601000, 'Expiry is based on server time');
  request(1, 0, [400, 0, 400], 'Over cap', 0, 0, 0);
  check(markerManager.m_aMarkers.length === 2, 'Per-player quota blocks another creation');
  request(1, 1, [500, 0, 500], 'Edited', 2, 3, 0);
  check(markerManager.snapshot(3).includes('Edited'), 'Resistance markers are visible across squads');
  check(markerManager.m_aMarkers.length === 2, 'Editing at quota does not allocate another marker');
  request(1, -1, [0, 0, 0], '', 0, 0, 0);
  check(markerManager.m_aMarkers.length === 1, 'Author deletion removes only the selected marker');
  const before = markerManager.m_aMarkers.length;
  for (const [position, scope, kind, ttl] of [
    [[NaN, 0, 0], 0, 0, 0], [[Infinity, 0, 0], 0, 0, 0], [[-1, 0, 0], 0, 0, 0],
    [[20, 0, 20], 3, 0, 0], [[20, 0, 20], 0, 5, 0], [[20, 0, 20], 0, 0, -1], [[20, 0, 20], 0, 0, 86401]
  ]) request(1, 0, position, 'Invalid', scope, kind, ttl);
  check(markerManager.m_aMarkers.length === before, 'Malformed bounds, scope, type and lifetime are rejected');
  request(3, 0, [20, 0, 20], 'No squad', 1, 0, 0);
  check(markerManager.m_aMarkers.length === before, 'Squad annotations require native squad membership');
  server = false;
  request(1, 0, [20, 0, 20], 'Client mutation', 0, 0, 0);
  check(markerManager.m_aMarkers.length === before, 'Client cannot execute server marker mutations');
  server = true; validPlayer = false;
  request(1, 0, [20, 0, 20], 'Dead player', 0, 0, 0);
  check(markerManager.m_aMarkers.length === before, 'Inactive/dead/non-Resistance requests are rejected');
  validPlayer = true;

  // Save only durable scopes; a reload resumes remaining TTL rather than using old world timestamps.
  request(1, 0, [700, 0, 700], 'Timed private', 0, 0, 600);
  request(2, 0, [800, 0, 800], 'Durable faction', 2, 0, 0);
  markerManager.m_aMarkers.push({m_iId: 100, m_iScope: 0, m_iExpiresAt: now - 1});
  const savedRows = A();
  compile(sources.markers, 'GetSavedRows').call(markerManager, savedRows);
  check(savedRows.length === 2, 'Save excludes squad annotations and already-expired records');
  check(savedRows.some(row => row.includes('Timed private') && row.endsWith('|600')), 'Save records remaining lifetime');
  const restored = {m_aMarkers: A(), m_iNextId: 1, m_iCampaignLimit: 4, m_iPerPlayerLimit: 2};
  now = 5000;
  compile(sources.markers, 'RestoreRows_S').call(restored, A([...savedRows, savedRows[0], 'invalid|row']));
  check(restored.m_aMarkers.length === 2, 'Restore rejects duplicate IDs and malformed records');
  check(restored.m_aMarkers.find(marker => marker.m_sCaption === 'Timed private').m_iExpiresAt === 605000, 'Expiry resumes from new session clock');
  check(restored.m_iNextId > Math.max(...restored.m_aMarkers.map(marker => marker.m_iId)), 'Restored creation counter does not reuse saved marker IDs');

  const actor = position => ({GetOrigin: () => position});
  const players = new Map([[1, actor([100, 0, 100])], [2, actor([110, 0, 110])]]);
  env.AROR_FieldAccess.Player = id => validPlayer ? players.get(id) : null;
  env.AROR_ForceSpawner.IsAlive = entity => !!entity && !entity.dead;
  env.SCR_EntityHelper.RplIdToEntity = entity => entity;
  env.AROR_GarageSnapshotHelper = {IsVehicle: entity => !!entity.vehicle};
  const recruitRecord = (id, owner, extra = {}) => Object.assign({m_iId: id, m_iOwnerId: owner,
    m_sOwnerKey: 'identity_' + owner, m_bFormed: true, m_Character: actor([105, 0, 100])}, extra);
  const personal = {
    m_aRecruits: A([recruitRecord(1, 1), recruitRecord(2, 2)]), m_iPlayerLimit: 2,
    request: compile(sources.recruitment, 'Request_S'),
    IsBoardOrder: order => order >= 4 && order <= 6,
    Owned(id) { return this.m_aRecruits.filter(x => x.m_iOwnerId === id).length; },
    NearHQ: () => true, FriendlyOrCivilian: vehicle => !vehicle.enemy,
    IsSeated: () => false, Dismount: () => true,
    SetOrder(recruit, order) { recruit.m_iOrder = order; },
    Reply: () => {}, Refund(recruit) { recruit.refunded = true; }, Waypoint: () => {},
    Remove_S(index) { this.m_aRecruits.Remove(index); }, Recruit_S() { this.raised = true; }
  };
  personal.request(1, 2, 2, 0, null);
  check(personal.m_aRecruits[1].m_iOrder === undefined, 'Personal orders cannot control another player recruit');
  personal.request(1, 2, 0, 0, null);
  check(personal.m_aRecruits[0].m_iOrder === 2, 'All-selected command changes only requester recruits');
  personal.request(1, 1, -1, 0, null);
  check(personal.m_aRecruits[0].m_iOrder === 2, 'Negative recruit IDs are rejected instead of selecting all');
  const enemyCar = Object.assign(actor([100, 0, 100]), {vehicle: true, enemy: true});
  personal.request(1, 5, 1, 0, enemyCar);
  check(personal.m_aRecruits[0].m_iOrder === 2, 'Crew orders reject hostile vehicles');
  const farCar = Object.assign(actor([500, 0, 500]), {vehicle: true});
  personal.request(1, 5, 1, 0, farCar);
  check(personal.m_aRecruits[0].m_iOrder === 2, 'Server rejects remote vehicle references');
  const highCar = Object.assign(actor([100, 0, 100]), {vehicle: true, GetOrigin: () => [100, 300, 100]});
  personal.request(1, 5, 1, 0, highCar);
  check(personal.m_aRecruits[0].m_iOrder === 2, 'Horizontal proximity cannot board a vehicle high above the player');
  const friendlyCar = Object.assign(actor([110, 0, 100]), {vehicle: true});
  personal.request(1, 5, 1, 0, friendlyCar);
  check(personal.m_aRecruits[0].m_Vehicle === friendlyCar, 'Nearby friendly car accepted for crew assignment');
  check(personal.m_aRecruits[0].m_iBoardDeadline === now + 20000, 'Boarding attempts have a finite deadline');
  personal.request(1, 8, 1, 2, null);
  check(personal.m_aRecruits[0].m_iOwnerId === 2 && personal.m_aRecruits[0].m_sOwnerKey === 'identity_2', 'Transfer changes owner ID and identity together');
  check(personal.m_aRecruits[0].m_iOrder === 1, 'Transferred recruit follows new owner');
  personal.request(1, 9, 1, 0, null);
  check(personal.m_aRecruits.length === 2, 'Former owner cannot dismiss a transferred recruit');
  personal.NearHQ = () => false;
  personal.request(2, 9, 1, 0, null);
  check(personal.m_aRecruits.length === 2, 'Dismissal refund cannot be used away from HQ');
  personal.NearHQ = () => true;
  const dismissed = personal.m_aRecruits[0];
  personal.request(2, 9, 1, 0, null);
  check(dismissed.refunded && personal.m_aRecruits.length === 1, 'HQ dismissal refunds and removes exactly one recruit');

  // Exercise actual native-seat decision code, especially occupancy after a vehicle starts moving.
  let seated = true, speed = 20, requestedSeat = null;
  const driverSeat = {GetVehicle: () => car, IsOccupied: () => false, GetOccupant: () => null, IsCompartmentAccessible: () => true};
  const cargoSeat = {cargo: true, GetVehicle: () => car, IsOccupied: () => false, GetOccupant: () => null, IsCompartmentAccessible: () => true};
  const seatAccess = {
    IsGettingIn: () => false, IsGettingOut: () => false, IsInCompartment: () => seated,
    GetCompartment: () => driverSeat,
    GetInVehicle: (_, seat) => { requestedSeat = seat; return true; }
  };
  env.CompartmentAccessComponent = {Cast: x => x};
  env.BaseCompartmentManagerComponent = {Cast: x => x};
  env.SCR_AIVehicleUsageComponent = {Cast: x => x};
  env.CargoCompartmentSlot = {Cast: seat => seat.cargo ? seat : null};
  env.TurretCompartmentSlot = {Cast: seat => seat.turret ? seat : null};
  env.ECloseDoorAfterActions = {CLOSE_DOOR: 1};
  const usage = {GetPilotCompartmentSlot: () => driverSeat};
  const compartments = {GetCompartments: out => out.push(cargoSeat, driverSeat)};
  const car = {
    GetOrigin: () => [100, 0, 100], GetPhysics: () => ({GetVelocity: () => ({Length: () => speed})}),
    FindComponent: type => type === env.BaseCompartmentManagerComponent ? compartments : usage
  };
  const boardingOwner = {FriendlyOrCivilian: () => true};
  const boardingRecruit = {m_Vehicle: car, m_iOrder: 5,
    m_Character: {GetOrigin: () => [100, 0, 100], FindComponent: () => seatAccess},
    m_Group: {GetGroupUtilityComponent: () => ({AddUsableVehicle() {}})}};
  const board = compile(sources.recruitment, 'Board');
  check(board.call(boardingOwner, boardingRecruit), 'Seated recruit remains confirmed aboard a moving vehicle');
  seated = false;
  check(!board.call(boardingOwner, boardingRecruit) && !requestedSeat, 'No new boarding request while vehicle is moving');
  speed = 0;
  check(!board.call(boardingOwner, boardingRecruit) && requestedSeat === driverSeat, 'Driver request uses pilot seat, not first available cargo seat');
  boardingRecruit.m_iOrder = 4;
  requestedSeat = null;
  board.call(boardingOwner, boardingRecruit);
  check(requestedSeat === cargoSeat, 'Passenger assignment uses a cargo seat');
  driverSeat.IsOccupied = () => true;
  boardingRecruit.m_iOrder = 5;
  requestedSeat = null;
  board.call(boardingOwner, boardingRecruit);
  check(requestedSeat === null, 'Occupied driver seat is never evicted');

  const recruitTick = {
    m_iSpawnTimeout: 90, m_aRecruits: A(), refunds: 0, removed: [],
    IsBoardOrder: order => order >= 4 && order <= 6,
    Board: () => false, IsSeated: () => false, Reply() {}, Waypoint() {},
    Refund() { this.refunds++; }, SetOrder(recruit, order) { recruit.m_iOrder = order; },
    Remove_S(index, members) { this.removed.push(members); this.m_aRecruits.Remove(index); }
  };
  now = 100000;
  const pendingRecruit = {m_iRaisedAt: 0, m_bFormed: false, m_Group: {living: []}, m_aAffiliated: A()};
  const deadRecruit = {m_bFormed: true, m_Group: {living: []}, m_aAffiliated: A()};
  const boardingTimeout = {m_bFormed: true, m_Group: {living: [actor([100, 0, 100])]}, m_aAffiliated: A(), m_iOrder: 5, m_iBoardDeadline: 99000};
  recruitTick.m_aRecruits.push(pendingRecruit, deadRecruit, boardingTimeout);
  env.AROR_ForceSpawner.CountAlive = group => group?.living?.length || 0;
  const updateRecruits = compile(sources.recruitment, 'Update_S');
  updateRecruits.call(recruitTick);
  check(recruitTick.refunds === 1, 'Formation timeout refunds pending recruit, never a dead recruit');
  check(recruitTick.removed.includes(true) && recruitTick.removed.includes(false), 'Timeout cancels spawning; casualties retain world bodies');
  check(boardingTimeout.m_iOrder === 2, 'Boarding deadline changes failed boarding to hold');
  updateRecruits.call(recruitTick);
  check(recruitTick.refunds === 1, 'Repeated status tick cannot refund timed-out recruit twice');

  const civilianManager = {
    m_aPlayers: A(), m_fTownRadius: 1000, m_fRetentionDistance: 1300,
    m_iCleanupDelay: 60, m_iSpawnTimeout: 90,
    PlayerNear: compile(sources.civilians, 'PlayerNear'),
    VisibleToPlayers: compile(sources.civilians, 'VisibleToPlayers'),
    update: compile(sources.civilians, 'UpdateActivity'),
    removed: [], routes: 0, ClearWaypoint() {},
    Remove(activity) { this.removed.push(activity); },
    IssueRoute() { this.routes++; }, PickDestination() { return false; }
  };
  function activity(changes = {}) {
    return Object.assign({m_iAge: 0, m_iStateAge: 0, m_iUnwatched: 0, m_iStalled: 0, m_iStalls: 0,
      m_bReady: true, m_bRetiring: false, m_bClaimedVehicle: false,
      m_aAffiliated: A(), m_aMembers: A(), m_iExpected: 1,
      m_vCentre: [1000, 0, 1000], m_vPosition: [1000, 0, 1000], m_vTarget: [1500, 0, 1000], m_vProgress: [1000, 0, 1000],
      m_Group: {living: [], GetAgents() {}}}, changes);
  }
  civilianManager.m_aPlayers.push([1800, 0, 1000]);
  check(civilianManager.PlayerNear([1000, 0, 1000], 1000), 'Player within 1 km activates town');
  check(!civilianManager.PlayerNear([1000, 0, 1000], 750), 'Activation compares real distance');
  const visible = activity({m_bRetiring: true});
  for (let i = 0; i < 20; i++) civilianManager.update(visible);
  check(civilianManager.removed.length === 0, 'Retiring civilians are not deleted while town is occupied');
  civilianManager.m_aPlayers.Clear();
  for (let i = 0; i < 11; i++) check(civilianManager.update(visible), 'Cleanup waits for grace period');
  check(!civilianManager.update(visible), 'Unwatched activity expires after cleanup delay');
  const claimed = activity({m_Vehicle: {player: true, GetOrigin: () => [1000, 0, 1000]}});
  civilianManager.update(claimed);
  check(claimed.m_bClaimedVehicle && claimed.m_bRetiring, 'Player boarding relinquishes ambient ownership');
  claimed.m_Vehicle.player = false;
  civilianManager.update(claimed);
  check(claimed.m_bClaimedVehicle, 'Claim remains set when player leaves the car');
  const timeoutGroup = {living: [], GetAgents() {}};
  const timeout = activity({m_bReady: false, m_iStateAge: 85, m_Group: timeoutGroup});
  civilianManager.m_aPlayers.push([1000, 0, 1000]);
  civilianManager.update(timeout);
  check(timeout.m_bRetiring && !timeout.m_Group, 'Timed-out civilian construction cancels native queue');
  check(deleted.includes(timeoutGroup), 'Timeout deletes group controller while retaining visible members');

  const refundOwner = {m_aRecruits: A([
    {m_bFormed: false, m_iFundsCost: 200, m_iManpowerCost: 1, m_Group: null},
    {m_bFormed: true, m_iFundsCost: 200, m_iManpowerCost: 1, m_Group: {living: [{}]}},
    {m_bFormed: true, m_iFundsCost: 200, m_iManpowerCost: 1, m_Group: {living: []}}
  ])};
  const refund = compile(sources.recruitment, 'GetRefundValue_S', '\nreturn {funds, manpower};').call(refundOwner, 0, 0);
  check(refund.funds === 400 && refund.manpower === 2, 'Saving refunds living and pending recruits, never casualties');

  // Resource/integration checks complement behavior tests. No engine compile or pathfinding is simulated.
  const names = [...sources.layout.matchAll(/Name "([^"]+)"/g)].map(m => m[1]);
  check(new Set(names).size === names.length, 'FIELD resource widget names are unique');
  for (const [prefix, count] of [['FieldPanel', 40], ['FieldText', 64], ['FieldBtnBg', 32], ['FieldBtnLabel', 32], ['FieldBtn', 32]]) {
    for (let i = 0; i < count; i++) check(names.includes(prefix + i), 'Required pool widget ' + prefix + i);
  }
  for (const component of ['AROR_PlayerMarkersComponent', 'AROR_RecruitmentComponent', 'AROR_CivilianManagerComponent']) {
    check(sources.prefab.includes(component + ' "'), component + ' wired to inherited game-mode template');
  }
  check(sources.phone.includes('OpenApp(AROR_EPhoneApp.FIELD)'), 'FIELD tile opens the app');
  check(sources.phone.includes('m_Field.Shutdown()'), 'Phone releases FIELD widgets on close');
  check(sources.economy.includes('refundFunds += recruitFunds') && sources.economy.includes('refundManpower += recruitManpower'), 'Economy serializer includes recruit refunds');
  check(sources.controller.includes('RplRcver.Owner') && !sources.markers.includes('[RplProp'), 'Private marker state uses owner RPC, never global RplProp');
  check(!sources.fieldApp.includes('m_wCaption.SetExactFontSize'), 'EditBox uses supported UIWidget methods');
  return {assertions, result: 'PASS', limitations: 'Mocked native services; Workbench compilation, replication, seating and pathfinding still require native acceptance.'};
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = runFieldSystemTests;
  if (require.main === module) {
    const fs = require('fs'), path = require('path');
    const base = path.resolve(__dirname, '../AROpenResistance/Open Resistance');
    const files = {markers: 'Scripts/Game/Field/AROR_PlayerMarkersComponent.c', recruitment: 'Scripts/Game/Recruitment/AROR_RecruitmentComponent.c', civilians: 'Scripts/Game/Civilians/AROR_CivilianManagerComponent.c', layout: 'UI/Layouts/Phone/AROR_Field.layout', prefab: 'Prefabs/GameModes/AROR_GameMode_Base.et', phone: 'Scripts/Game/Phone/AROR_PhoneMenu.c', economy: 'Scripts/Game/Economy/AROR_EconomyComponentSerializer.c', controller: 'Scripts/Game/Field/AROR_FieldPlayerController.c', fieldApp: 'Scripts/Game/Phone/AROR_FieldApp.c'};
    const sources = Object.fromEntries(Object.entries(files).map(([name, file]) => [name, fs.readFileSync(path.join(base, file), 'utf8')]));
    console.log(JSON.stringify(runFieldSystemTests(sources), null, 2));
  }
}
