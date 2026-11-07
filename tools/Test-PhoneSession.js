// Actual Enforce presentation/lifecycle methods with native menus, maps and widgets mocked.
// This does not compile scripts or test native input/rendering.
const fs = require('fs'), path = require('path'), assert = require('assert/strict');
const root = path.resolve(__dirname, '../AROpenResistance/Open Resistance');
const read = name => fs.readFileSync(path.join(root, 'Scripts/Game/Phone', name + '.c'), 'utf8');
const sources = Object.fromEntries(['PhoneSessionState', 'PhoneComponent', 'PhoneMenu', 'HiCommApp', 'HiCommMap', 'OpsApp', 'SquadApp', 'BlackMarketApp', 'ImportsApp', 'MarsApp', 'RebIntelApp', 'BankTransferApp'].map(n => [n, read('AROR_' + n)]));
let assertions = 0;
const equal = (a, b, why) => { assertions++; assert.deepEqual(a, b, why); };
const check = (condition, why) => { assertions++; assert(condition, why); };
for (const [name, fn] of Object.entries({
  Copy(a) { this.splice(0, this.length, ...a); }, Clear() { this.length = 0; }, Count() { return this.length; },
  IsEmpty() { return !this.length; }, Insert(v) { this.push(v); }
})) Object.defineProperty(Array.prototype, name, { value: fn, configurable: true });
Object.defineProperty(String.prototype, 'IsEmpty', { value() { return !this.length; }, configurable: true });
function enumFrom(source, name) {
  return Object.fromEntries(source.match(new RegExp('enum ' + name + '\\s*\\{([^}]+)\\}'))[1].replace(/\/\/[^\r\n]*/g, '').split(',').map(s => s.trim()).filter(Boolean).map((s, i) => [s, i]));
}
const apps = enumFrom(sources.PhoneMenu, 'AROR_EPhoneApp');
const hiScreens = enumFrom(sources.HiCommApp, 'AROR_EHiCommScreen');
let session, dedicated = false, play = true, inputCalls = [], events = [];
const componentScope = { instance: null }, menuScope = { instance: null };
const env = {
  AROR_EPhoneApp: apps, AROR_EHiCommScreen: hiScreens, AROR_EOpsScreen: { LOGIN: 0, BOARD: 1 },
  AROR_EHighCommandOrder: { MOVE: 0 }, AROR_EFactionRole: { OCCUPIERS: 1 }, AROR_EResponseType: { COMBINED_ARMS: 0 },
  AROR_PhoneComponent: { GetSession: () => session },
  Math: { Clamp: (v, a, b) => Math.max(a, Math.min(v, b)), AbsFloat: Math.abs, Max: Math.max }, Vector: (x, y, z) => [x, y, z],
  MIN_ZOOM: Number(sources.HiCommMap.match(/MIN_ZOOM = ([\d.]+)/)[1]), MAX_ZOOM: Number(sources.HiCommMap.match(/MAX_ZOOM = ([\d.]+)/)[1]),
  string: { Empty: '' },
  INPUT_ACTION: 'AROR_OpenPhone', EActionTrigger: { DOWN: 1 },
  RplMode: { Dedicated: 1 }, RplSession: { Mode: () => dedicated ? 1 : 0 }, Print() {}, LogLevel: { NORMAL: 0 },
  GetGame: () => ({ InPlayMode: () => play, GetInputManager: () => ({
    AddActionListener: () => inputCalls.push('add'), RemoveActionListener: () => inputCalls.push('remove')
  }), GetCallqueue: () => ({ CallLater: () => inputCalls.push('schedule'), Remove: () => inputCalls.push('unschedule') }) })
};
function freshState() {
  const result = {};
  for (const m of sources.PhoneSessionState.matchAll(/^\t(?:ref )?([\w<>]+) (m_\w+)(?: = ([^;]+))?;/gm)) {
    const [, type, name, initial] = m;
    result[name] = initial === '{}' ? [] : initial ? new Function(...Object.keys(env), 'return ' + initial)(...Object.values(env)) :
      type === 'string' ? '' : type === 'bool' ? false : type === 'vector' ? [0, 0, 0] : 0;
  }
  return result;
}
env.AROR_PhoneSessionState = function() { return freshState(); };
function method(source, name, members = [], scope = menuScope) {
  const m = source.match(new RegExp('^\\t[^\\r\\n]*\\b' + name + '\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  assert(m, 'Missing actual method: ' + name);
  const args = m[1].replace(/\bnotnull /g, '').replace(/\b\w+ (\w+)(?=\s*(?:=|,|$))/g, '$1');
  let body = m[2].replace(/\b(?:bool|int|float|vector|ResourceName|string|AROR_\w+|InputManager) (\w+) =/g, 'let $1 =')
    .replace(/super\.(\w+)\(/g, 'this.Base$1(').replace(/(?<![.\w])m_\w+/g, s => 'this.' + s)
    .replace(/(?<![.\w])s_Instance\b/g, 'scope.instance');
  for (const member of members) body = body.replace(new RegExp('(?<![.\\w])' + member + '\\b', 'g'), 'this.' + member);
  return new Function(...Object.keys(env), 'scope', `return function(${args}) {${body}}`)(...Object.values(env), scope);
}
function widget(text = '') { return { text, GetText() { return this.text; }, SetText(t) { this.text = t; }, SetCurrent(v) { this.current = v; }, SetVisible() {} }; }
function app(name) {
  const result = { SaveState: method(sources[name], 'SaveState', ['ReadSearch']), RestoreState: method(sources[name], 'RestoreState') };
  // Seed the actual saved fields with distinct data, so missing assignments and swapped fields fail round-trip checks.
  for (const m of sources[name].matchAll(/\b(?:protected )?(?:ref )?([\w<>]+) (m_\w+)(?:\s*=|;)/g)) {
    const [, type, field] = m;
    result[field] = type.startsWith('array<') ? [2, 7] : type === 'string' ? field + '-draft' : type === 'bool' ? true : type === 'vector' ? [250, 0, 700] : 3;
  }
  result.m_wName = widget('Falcon draft'); result.m_wSearch = widget('rifle'); result.m_sQuery = 'rifle';
  result.m_wAmountEdit = widget('1250'); result.m_wReadiness = widget(); result.m_wCrackdown = widget();
  result.m_aCartPrefabs = ['rifle', 'magazine']; result.m_aCartQuantities = [1, 4]; result.m_bAwaiting = false;
  result.ReadSearch = () => {}; result.m_Map = { SavePhoneView() {}, RestorePhoneView() {} };
  return result;
}
function phone() {
  const result = { m_bSessionReady: true, m_eApp: apps.HOME, m_fTurnTarget: 0, m_fTurn: 0,
    SaveSession: method(sources.PhoneMenu, 'SaveSession'), RestoreSession: method(sources.PhoneMenu, 'RestoreSession', ['OpenApp']),
    OpenApp: method(sources.PhoneMenu, 'OpenApp', ['IsLandscapeApp']), IsLandscapeApp: method(sources.PhoneMenu, 'IsLandscapeApp'),
    OnMenuClose: method(sources.PhoneMenu, 'OnMenuClose', ['SaveSession', 'OnBack']), OnBack() {}, BaseOnMenuClose() { events.push('native close'); }
  };
  for (const name of ['HiComm', 'Ops', 'RebIntel', 'Squad', 'BlackMarket', 'Imports', 'Mars', 'BankTransfer']) {
    result['m_' + name] = app(name + 'App'); result['m_' + name].Shutdown = () => events.push('shutdown ' + name);
  }
  return result;
}
session = freshState();
check(Object.keys(session).length > 40, 'all session fields were initialized from the actual class');
check(!/\b(?:Widget|IEntity|PlayerController|RplProp|RplRpc)\s+m_/.test(sources.PhoneSessionState), 'session holds presentation data, not widgets, entities or replicated authority');
let initial = phone(); initial.RestoreSession(); equal(initial.m_eApp, apps.HOME, 'first open remains Home');
session = null; initial.SaveSession(); initial.RestoreSession(); equal(initial.m_eApp, apps.HOME, 'unavailable session owner is safe');
session = freshState(); initial.m_bSessionReady = false; initial.m_eApp = apps.HICOMM; initial.SaveSession();
equal(session.m_bCaptured, false, 'failed widget initialization cannot overwrite a prior session');
for (const selectedApp of Object.values(apps)) {
  session = freshState(); const before = phone(); before.m_eApp = selectedApp;
  before.m_HiComm.m_eScreen = hiScreens.STRATEGIC;
  before.SaveSession();
  const snapshot = structuredClone(session), reopened = phone();
  for (const name of ['HiComm', 'Ops', 'RebIntel', 'Squad', 'BlackMarket', 'Imports', 'Mars', 'BankTransfer']) {
    // Restore into new app objects with deliberately different prior values.
    for (const field of Object.keys(reopened['m_' + name])) {
      if (/^m_[iesbfv]/.test(field)) reopened['m_' + name][field] = null;
      else if (field.startsWith('m_a')) reopened['m_' + name][field] = [];
    }
  }
  reopened.RestoreSession();
  equal(reopened.m_eApp, selectedApp, 'each app, including Home and Transfer, resumes');
  equal(reopened.m_fTurn, reopened.IsLandscapeApp(selectedApp) ? 1 : 0, 'resume opens in the correct orientation');
  reopened.SaveSession(); equal(session, snapshot, 'all app presentation values round-trip across menu destruction');
  equal(reopened.m_HiComm.m_eScreen, hiScreens.STRATEGIC, 'HICOMM skips login after successful session sign-in');
  equal(reopened.m_Squad.m_iPage, snapshot.m_iSquadPage, 'Squad page resumes without a login');
}
session = freshState(); let p = phone(); p.m_HiComm.m_eScreen = hiScreens.LOGIN; p.SaveSession();
const loggedOut = phone(); loggedOut.RestoreSession(); equal(loggedOut.m_HiComm.m_eScreen, hiScreens.LOGIN, 'explicit HICOMM logout stays logged out');
check(/m_eScreen != AROR_EHiCommScreen.LOGIN && \(!highCommand \|\| !highCommand.IsLeader/.test(sources.HiCommApp), 'live HICOMM leadership revocation remains enforced');
let leader = true, drawn;
env.AROR_HighCommandComponent = { GetInstance: () => ({ IsLeader: () => leader }) };
env.System = { GetTickCount: () => 5000 }; env.WIDTH = 1700; env.HEIGHT = 900; env.CONTENT_SCALE = 1;
const hiComm = { m_eScreen: hiScreens.STRATEGIC, m_eLastScreen: hiScreens.STRATEGIC,
  GetLocalPlayerId: () => 7, PlaceRaw() {}, DrawLogin() { drawn = 'login'; }, DrawTopBar() {},
  DrawCommand() { drawn = 'command'; }, DrawStrategic() { drawn = 'strategic'; }, DrawUnits() { drawn = 'units'; },
  CloseGameMap() {}, HideUnused() {}, m_Map: { UsesGameMap: () => false }
};
for (const name of ['m_wRoot', 'm_wBackground', 'm_wGameMapRoot', 'm_wMapClip', 'm_wMapCanvas', 'm_wMapOverlay', 'm_wMapButton']) hiComm[name] = widget();
const hiLayout = method(sources.HiCommApp, 'Layout', ['GetLocalPlayerId', 'PlaceRaw', 'DrawLogin', 'DrawTopBar', 'DrawCommand', 'DrawStrategic', 'DrawUnits', 'CloseGameMap', 'HideUnused']);
hiLayout.call(hiComm, 0, 0, 1); equal(drawn, 'strategic', 'actual layout resumes the signed-in screen');
leader = false; hiLayout.call(hiComm, 0, 0, 1); equal([drawn, hiComm.m_eScreen], ['login', hiScreens.LOGIN], 'actual layout revokes the cached UI session on leadership loss');
leader = true; hiLayout.call(hiComm, 0, 0, 1); equal(drawn, 'login', 'regaining leadership does not bypass explicit reauthentication after revocation');
session = freshState(); p = phone(); p.m_eApp = apps.SQUAD; p.m_Squad = null; p.SaveSession();
const missingApp = phone(); missingApp.m_Squad = null; missingApp.RestoreSession(); equal(missingApp.m_eApp, apps.HOME, 'missing optional app falls back to Home');
p = phone(); let saved = false; p.SaveSession = () => { saved = true; events.push('save'); };
events = []; p.OnMenuClose(); equal(events[0], 'save', 'state captured before map/widgets/resources are shut down');
check(saved && !p.m_bSessionReady && p.m_HiComm === null && p.m_Squad === null, 'menu still releases app objects after capture');
const openBody = sources.PhoneMenu.slice(sources.PhoneMenu.indexOf('override void OnMenuOpen()'), sources.PhoneMenu.indexOf('override void OnMenuClose()'));
check(openBody.indexOf('RestoreSession();') > openBody.indexOf('m_eApp = AROR_EPhoneApp.HOME;'), 'restore follows initial Home reset');
check(openBody.indexOf('RestoreSession();') < openBody.indexOf('UpdateLayout();'), 'restore precedes first layout');
check(openBody.includes('m_bHotkeyDown = true;'), 'opening key is consumed again on menu reuse');
const market = app('BlackMarketApp'); market.m_bAwaiting = true; market.SaveState(session);
equal(session.m_aMarketCartPrefabs, [], 'submitted checkout is not restored as an unpaid draft');
equal(session.m_aMarketCartQuantities, [], 'submitted checkout quantities are cleared with the draft');
market.m_wSearch = null; market.RestoreState(session); equal(market.m_sQuery, session.m_sMarketQuery, 'optional search widget can be absent during restore');
check(!/m_bAwaiting = false/.test(sources.BlackMarketApp.match(/void OnOpened\(\)[\s\S]*?\n\t\}/)[0]), 'switching apps does not clear an in-flight checkout guard');
env.AROR_NotificationManagerComponent = { ShowHint() {} }; env.AROR_PhoneMenu = { GetInstance: () => null };
const transferReply = method(fs.readFileSync(path.join(root, 'Scripts/Game/GameFlow/AROR_ModdedPlayerController.c'), 'utf8'), 'AROR_RpcDo_BankTransferResult');
session.m_sTransferAmount = '1250'; transferReply(false, ''); equal(session.m_sTransferAmount, '1250', 'failed transfer retains its draft');
transferReply(true, ''); equal(session.m_sTransferAmount, '', 'late successful transfer clears the saved amount while phone is closed');
// Replace the pooled widget superclass only; exercise the real visibility transition policy.
const squadVisibleSource = sources.SquadApp.replace('super.SetVisible(', 'BaseSetVisible(');
const squad = { m_iGroupId: 10, m_iPage: 1, m_iConfirmDismiss: 8, m_bShown: false, m_wName: widget(), BaseSetVisible() {} };
method(squadVisibleSource, 'SetVisible', ['BaseSetVisible']).call(squad, true, 1);
equal([squad.m_iGroupId, squad.m_iPage, squad.m_iConfirmDismiss], [10, 1, -1], 'Squad preserves location but cancels old dismissal confirmation');
// Native map attachment may complete after the app's first frame.
const map = { m_bHasView: false, m_bRestorePhoneView: false, m_fWidth: 0, m_fHeight: 0, m_fZoom: 1, m_GameMap: null,
  UsesGameMap() { return !!this.m_GameMap; }, GetZoom() { return this.m_GameMap ? this.m_GameMap.zoom : this.m_fZoom; },
  ScreenToWorld() { return this.m_GameMap ? this.m_GameMap.centre : this.m_vCentre; }, ClampCentre() {},
  SavePhoneView: method(sources.HiCommMap, 'SavePhoneView', ['ScreenToWorld', 'GetZoom']), RestorePhoneView: method(sources.HiCommMap, 'RestorePhoneView'),
  SetView: method(sources.HiCommMap, 'SetView', ['ClampCentre']), SetGameMap: method(sources.HiCommMap, 'SetGameMap', ['UsesGameMap', 'ScreenToWorld', 'GetZoom']) };
session.m_bHiCommMapView = true; session.m_vHiCommMapCentre = [900, 0, 600]; session.m_fHiCommMapZoom = 4;
map.RestorePhoneView(session); const min = [0, 0, 0], max = [12000, 0, 12000]; map.SetView(min, max, 800, 500);
equal([map.m_vCentre, map.m_fZoom], [[900, 0, 600], 4], 'map restores after initial bounds setup');
const nativeMap = { zoom: 1, centre: [0, 0, 0], GetMinZoom: () => 1, GetMaxZoom: () => 32,
  ZoomPanSmooth(zoom, x, z) { this.zoom = zoom; this.centre = [x, 0, z]; } };
map.SetGameMap(nativeMap); equal([nativeMap.centre, nativeMap.zoom], [[900, 0, 600], 4], 'async terrain map receives restored view');
nativeMap.zoom = 7; nativeMap.centre = [3000, 0, 2100]; map.SavePhoneView(session); map.SetGameMap(null);
equal([map.m_vCentre, map.m_fZoom], [[3000, 0, 2100], 7], 'leaving terrain map caches its latest view');
map.SetGameMap(nativeMap); equal(nativeMap.centre, session.m_vHiCommMapCentre, 'terrain map resumes after switching to Home and back');
map.SetGameMap(null); session.m_fHiCommMapZoom = env.MAX_ZOOM * 2;
map.RestorePhoneView(session); map.SetView(min, max, 800, 500);
equal(map.m_fZoom, session.m_fHiCommMapZoom, 'terrain zoom is not truncated to the smaller procedural-map limit');
map.SetGameMap(nativeMap); equal(nativeMap.zoom, session.m_fHiCommMapZoom, 'restored terrain zoom uses native limits');
// The component owns/reset the state at world lifetime, independently of pawn or menu lifetime.
const owner = { BaseOnPostInit() {}, BaseOnDelete() {}, Update() {}, OnPhoneAction() {} };
owner.OnPostInit = method(sources.PhoneComponent, 'OnPostInit', ['Update', 'OnPhoneAction'], componentScope);
owner.OnDelete = method(sources.PhoneComponent, 'OnDelete', ['Update', 'OnPhoneAction'], componentScope);
owner.OnPostInit(null); check(componentScope.instance === owner && owner.m_Session, 'client world creates session state');
const getSession = method(sources.PhoneComponent, 'GetSession', [], componentScope);
equal(getSession(), owner.m_Session, 'menu can retrieve component-owned session');
owner.m_Session.m_eApp = apps.HICOMM; owner.OnDelete(null);
equal(getSession(), null, 'world teardown releases the old client session');
owner.OnPostInit(null); equal(getSession().m_eApp, apps.HOME, 'new world starts with fresh phone state');
owner.OnDelete(null); dedicated = true; inputCalls = []; owner.OnPostInit(null);
equal([componentScope.instance, inputCalls], [null, []], 'dedicated server creates no client session or input hooks');
dedicated = false; play = false; owner.OnPostInit(null); equal(componentScope.instance, null, 'editor mode creates no client session');
console.log(JSON.stringify({ assertions, note: 'Actual presentation/lifecycle bodies with native services mocked. Workbench compile, phone input/rendering and real map lifecycle still required.' }));
