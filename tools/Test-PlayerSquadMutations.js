// Check that the squad tests reject realistic broken server/lifecycle policies.
// All mutations exist only in VM memory; source files and native engine state are never changed.
const fs = require('fs'), path = require('path'), vm = require('vm'), assert = require('assert');
const root = path.resolve(__dirname, '..');
const target = path.join(root, 'AROpenResistance/Open Resistance/Scripts/Game/Squads/AROR_SquadManagerComponent.c');
const source = fs.readFileSync(target, 'utf8').replace(/\r\n/g, '\n');
const tests = fs.readFileSync(path.join(__dirname, 'Test-PlayerSquads.js'), 'utf8');
function run(candidate) {
  const output = [];
  const fakeFs = { ...fs, readFileSync(file, options) {
    if (typeof file === 'string' && path.resolve(file) === target) return candidate;
    return fs.readFileSync(file, options);
  } };
  try {
    vm.runInNewContext(tests, { __dirname, require: name => name === 'fs' ? fakeFs : require(name),
      console: { log: value => output.push(value) } }, { filename: 'Test-PlayerSquads.js', timeout: 10000 });
    return { passed: true, output };
  } catch (error) { return { passed: false, reason: error.message }; }
}
function mutate(method, before, after) {
  const pattern = new RegExp('^\\t[^\\n]*\\b' + method + '\\([^\\n]*\\)\\n\\t\\{[\\s\\S]*?^\\t\\}', 'm');
  const body = source.match(pattern)?.[0];
  assert.ok(body && body.includes(before), 'Mutation target missing: ' + method);
  return source.replace(body, body.replace(before, after));
}
const baseline = run(source);
assert.ok(baseline.passed, 'Mutation baseline must pass: ' + baseline.reason);
const cases = [
  ['client-side renaming', 'Rename_S', '\t\tif (!Replication.IsServer())\n\t\t\treturn;\n', ''],
  ['pending slots ignored', 'CountMembers', 'if (recruit.m_Master == group && !recruit.m_bJoined)', 'if (false)'],
  ['leader checks removed', 'GetLedSquad', ' || !group.IsPlayerLeader(playerId)', ''],
  ['cross-squad dismissal', 'Dismiss_S', ' || recruit.m_Master != group', ''],
  ['foreign AI parent ignored', 'CanRetireRecruit', 'if (actual && actual != expected)', 'if (false)'],
  ['released membership ignored', 'CanRetireRecruit', 'if (recruit.m_bJoined && expected && !expected.IsAIControlledCharacterMember(recruit.m_Character))', 'if (false)'],
  ['deleted publication unguarded', 'Publish_S', 'if (recruit.m_bJoined && !AROR_ForceSpawner.IsAlive(recruit.m_Character))', 'if (false)'],
  ['dead or transferred arrival reused', 'Update_S', 'if (recruit.m_Character && (!AROR_ForceSpawner.IsAlive(recruit.m_Character) || (currentGroup && currentGroup != recruit.m_SpawnGroup)))', 'if (false)'],
  ['name validation bypassed', 'Rename_S', 'if (!AROR_ResistanceName.Validate(name, reason))', 'if (false)'],
  ['unchanged snapshots republished', 'Publish_S', 'if (snapshot == m_sRoster)', 'if (false)']
];
const results = cases.map(([name, method, before, after]) => {
  const result = run(mutate(method, before, after));
  assert.ok(!result.passed, 'Tests failed to detect ' + name);
  return { name, detected: true };
});
console.log(JSON.stringify({ assertions: results.length + 1, mutantsDetected: results.length, nativeVerified: false, results }));
